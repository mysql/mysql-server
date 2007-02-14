# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl26.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAXMVC" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMCXS" varchar(2) NOT NULL default '00',
  "AAAAXMVF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMCXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMVI" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMCXS" varchar(2) NOT NULL default '00',
  "AAAAXMVP" varchar(2) NOT NULL default '00',
  "AAAAXMVM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABMCXS","AAAAXMVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXNWB" varchar(4) NOT NULL default ' ',
  "AAAAXMVZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXNWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWP" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXMWI" varchar(2) NOT NULL default '00',
  "AAAAXMXX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXNWP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWP" varchar(2) NOT NULL default ' ',
  "AAAAXMXX" varchar(2) NOT NULL default '00',
  "AAABMLTD" varchar(3) NOT NULL default '000',
  "AAABJHFY" varchar(6) NOT NULL default ' ',
  "AAABDQNO" varchar(4) NOT NULL default '0000',
  "AAAAXMWU" varchar(1) NOT NULL default ' ',
  "AAAAXMWV" varchar(1) NOT NULL default '0',
  "AAAAXMWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWP","AAAAXMXX","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWP" varchar(2) NOT NULL default ' ',
  "AAAAXMXX" varchar(2) NOT NULL default '00',
  "AAABJHFY" varchar(6) NOT NULL default ' ',
  "AAABAELT" varchar(10) NOT NULL default ' ',
  "AAABMLCO" varchar(1) NOT NULL default ' ',
  "AAABHEQK" varchar(8) NOT NULL default ' ',
  "AAABFREQ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWP","AAAAXMXX","AAABJHFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWP" varchar(2) NOT NULL default ' ',
  "AAAAXMXX" varchar(2) NOT NULL default '00',
  "AAABJHFY" varchar(6) NOT NULL default ' ',
  "AAAAXMXZ" varchar(25) NOT NULL default ' ',
  "AAABKKVS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWP","AAAAXMXX","AAABJHFY","AAAAXMXZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXMZV" varchar(6) NOT NULL default ' ',
  "AAABMRCU" varchar(7) NOT NULL default '0000000',
  "AAAAXNWP" varchar(2) NOT NULL default ' ',
  "AAABMELN" varchar(2) NOT NULL default ' ',
  "AAAAXMZK" varchar(1) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(15) NOT NULL default ' ',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAAAXMZS" smallint(6) NOT NULL default '0',
  "AAAAXMZT" text,
  PRIMARY KEY ("AAABMRIL","AAAAXMZV","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXMZV" varchar(6) NOT NULL default ' ',
  "AAAAXMZK" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(15) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABKAFO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXMZV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXMZV" varchar(6) NOT NULL default ' ',
  "AAABMRCU" varchar(7) NOT NULL default '0000000',
  "AAAAXNWP" varchar(2) NOT NULL default ' ',
  "AAABMELN" varchar(2) NOT NULL default ' ',
  "AAAAXMZK" varchar(1) NOT NULL default ' ',
  "AAABLRBI" varchar(1) NOT NULL default ' ',
  "AAABMDXM" varchar(15) NOT NULL default ' ',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAAAXMZS" smallint(6) NOT NULL default '0',
  "AAAAXMZT" text,
  PRIMARY KEY ("AAABMRIL","AAAAXMZV","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXMZX" (
  "AAAAXNWP" varchar(2) NOT NULL default ' ',
  "AAABAELT" varchar(10) NOT NULL default ' ',
  "AAABAZVX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXNWP","AAABAELT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAAAXNNU" varchar(4) NOT NULL default ' ',
  "AAAAXNAI" varchar(10) NOT NULL default ' ',
  "AAAAXNLM" varchar(20) NOT NULL default ' ',
  "AAAAXNAK" varchar(8) NOT NULL default '00000000',
  "AAAAXNAL" varchar(10) NOT NULL default ' ',
  "AAAAXNAM" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWO","AAAAXNWM","AAAAXNNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAABILTS" varchar(20) NOT NULL default ' ',
  "AAAAXNAW" varchar(20) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAAAXNBB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWO","AAAAXNWM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNBK" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABBAHO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNBO" (
  "AAABKVPX" varchar(2) NOT NULL default '00',
  "AAABAQKT" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGZRZ" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABKVPX","AAABAQKT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNCC" varchar(4) NOT NULL default ' ',
  "AAAAXNCD" varchar(4) NOT NULL default ' ',
  "AAAAXNCE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNCI" (
  "AAABAXKZ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNCN" decimal(15,2) NOT NULL default '0.00',
  "AAAAYGJN" decimal(15,2) NOT NULL default '0.00',
  "AAAAXNCP" decimal(9,2) NOT NULL default '0.00',
  "AAAAXNCQ" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAXKZ","AAABKFCZ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNTB" varchar(4) NOT NULL default '0000',
  "AAABLWJT" varchar(57) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZNTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVTU" varchar(5) NOT NULL default '00000',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABDSYZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAXNDX" varchar(3) NOT NULL default ' ',
  "AAAAXNDY" varchar(4) NOT NULL default ' ',
  "AAAAXNDZ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHXLS" varchar(5) NOT NULL default ' ',
  "AAAAXUZF" varchar(10) NOT NULL default ' ',
  "AAAAXNDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAAAXNDX","AAAAXNDY","AAAAXNDZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAXNEL" varchar(4) NOT NULL default ' ',
  "AAABHXLS" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAAQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAAAXNEL","AAABHXLS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCAB" varchar(1) NOT NULL default ' ',
  "AAABKVPX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNEU" decimal(9,2) NOT NULL default '0.00',
  "AAAAXNEV" decimal(11,8) NOT NULL default '0.00000000',
  "AAAAXXDT" decimal(13,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABJCAB","AAABKVPX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNFM" varchar(7) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQKE" varchar(7) NOT NULL default '0000000',
  "AAAAXNFH" varchar(7) NOT NULL default ' ',
  "AAAAXNFI" varchar(7) NOT NULL default ' ',
  "AAAAXNFJ" varchar(4) NOT NULL default '0000',
  "AAAAXNFK" varchar(7) NOT NULL default '0000000',
  PRIMARY KEY ("AAABMRIL","AAAAXNFM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZECI" varchar(2) NOT NULL default ' ',
  "AAABBOPO" varchar(1) NOT NULL default ' ',
  "AAAAZBEI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMECK" decimal(7,2) NOT NULL default '0.00',
  "AAAAXNFW" decimal(7,2) NOT NULL default '0.00',
  "AAAAXNFX" varchar(1) NOT NULL default ' ',
  "AAAAXNFY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZECI","AAABBOPO","AAAAZBEI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAZBEI" varchar(2) NOT NULL default ' ',
  "AAAAXNGI" varchar(3) NOT NULL default '000',
  "AAAAXNWD" varchar(1) NOT NULL default ' ',
  "AAAAXNGK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAAAZBEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMZS" varchar(2) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABLSIN" varchar(4) NOT NULL default ' ',
  "AAAAXNGT" varchar(5) NOT NULL default ' ',
  "AAAAXNGU" varchar(2) NOT NULL default '00',
  "AAAAXNGV" varchar(2) NOT NULL default '00',
  "AAAAXNGW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMMZS","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMZS" varchar(2) NOT NULL default ' ',
  "AAAAXNHD" varchar(6) NOT NULL default ' ',
  "AAAAXNHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAABMBXO" varchar(4) NOT NULL default ' ',
  "AAABMMZS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWO","AAAAXNWM","AAABMBXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAABMBXO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNHY" varchar(1) NOT NULL default ' ',
  "AAABIVCA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWO","AAAAXNWM","AAABMBXO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNIF" (
  "AAAAXWZX" varchar(1) NOT NULL default ' ',
  "AAAAXWZO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXWZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWZX" varchar(1) NOT NULL default ' ',
  "AAAAXWZY" varchar(2) NOT NULL default ' ',
  "AAAAZNSB" varchar(30) NOT NULL default ' ',
  "AAAAXWZV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWZX","AAAAXWZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXAN" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(50) NOT NULL default ' ',
  "AAABLWIT" varchar(1) NOT NULL default ' ',
  "AAABLWIV" varchar(1) NOT NULL default ' ',
  "AAABLWIX" varchar(1) NOT NULL default ' ',
  "AAABLWIZ" varchar(1) NOT NULL default ' ',
  "AAABLWJB" varchar(1) NOT NULL default ' ',
  "AAABLWJD" varchar(1) NOT NULL default ' ',
  "AAABMLCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXXAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXAN" varchar(2) NOT NULL default ' ',
  "AAABETDL" varchar(2) NOT NULL default ' ',
  "AAABLWJT" varchar(10) NOT NULL default ' ',
  "AAABKCGA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXXAN","AAABETDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNKO" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNJU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKMH" varchar(1) NOT NULL default ' ',
  "AAABBMVH" varchar(2) NOT NULL default ' ',
  "AAABHANX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKMH","AAABBMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNKN" varchar(2) NOT NULL default ' ',
  "AAAAXNKO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNKI" varchar(1) NOT NULL default ' ',
  "AAAAXNKJ" varchar(2) NOT NULL default ' ',
  "AAABLZMW" varchar(1) NOT NULL default ' ',
  "AAAAXNKL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXNKN","AAAAXNKO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAXNMH" varchar(8) NOT NULL default '00000000',
  "AAAAXNMI" varchar(8) NOT NULL default '00000000',
  "AAAAXNMJ" varchar(1) NOT NULL default ' ',
  "AAAAXNKW" varchar(8) NOT NULL default '00000000',
  "AAABEIIS" varchar(16) NOT NULL default ' ',
  "AAAAXNKY" varchar(4) NOT NULL default '0000',
  "AAAAXNKZ" varchar(8) NOT NULL default '00000000',
  "AAABBGRP" varchar(40) NOT NULL default ' ',
  "AAABBGZK" varchar(40) NOT NULL default ' ',
  "AAAAZAVJ" varchar(50) NOT NULL default ' ',
  "AAABHALW" varchar(15) NOT NULL default ' ',
  "AAAAXNLE" varchar(8) NOT NULL default '00000000',
  "AAAAXNLF" varchar(30) NOT NULL default ' ',
  "AAAAXNLG" varchar(8) NOT NULL default '00000000',
  "AAAAXNLH" varchar(8) NOT NULL default '00000000',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAAAXPHD" varchar(1) NOT NULL default '0',
  "AAABAQKT" varchar(1) NOT NULL default ' ',
  "AAAAXNLM" varchar(20) NOT NULL default ' ',
  "AAAAXNLN" varchar(2) NOT NULL default ' ',
  "AAAAZEDP" varchar(18) NOT NULL default ' ',
  "AAAAXNLP" varchar(5) NOT NULL default ' ',
  "AAAAXNLQ" varchar(2) NOT NULL default ' ',
  "AAAAXNLR" varchar(8) NOT NULL default '00000000',
  "AAAAXNLS" varchar(8) NOT NULL default '00000000',
  "AAAAXNLT" varchar(2) NOT NULL default ' ',
  "AAAAXNLU" varchar(8) NOT NULL default '00000000',
  "AAAAXNLV" varchar(30) NOT NULL default ' ',
  "AAAAXNLW" varchar(30) NOT NULL default ' ',
  "AAAAXNLX" varchar(80) NOT NULL default ' ',
  "AAAAXNLY" varchar(80) NOT NULL default ' ',
  "AAAAXNLZ" varchar(80) NOT NULL default ' ',
  "AAAAXNMA" varchar(80) NOT NULL default ' ',
  "AAAAXNMB" varchar(80) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAXNMH","AAAAXNMI","AAAAXNMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYBXE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABKAFO","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNMX" (
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNND" varchar(1) NOT NULL default ' ',
  "AAABAQKE" varchar(1) NOT NULL default ' ',
  "AAAAXNNF" varchar(1) NOT NULL default ' ',
  "AAAAXNNG" varchar(1) NOT NULL default ' ',
  "AAAAXNNH" varchar(1) NOT NULL default ' ',
  "AAAAXNNI" varchar(1) NOT NULL default ' ',
  "AAAAXNNJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLECD","AAABKAFO","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNNU" varchar(4) NOT NULL default ' ',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAAAXNNS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNNU","AAABBAHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMR" varchar(5) NOT NULL default ' ',
  "AAAAXVTU" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFKVH" decimal(5,2) NOT NULL default '0.00',
  "AAAAXNOD" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAMR","AAAAXVTU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMR" varchar(5) NOT NULL default ' ',
  "AAABFREQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNPB" varchar(2) NOT NULL default ' ',
  "AAAAXNRA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXNPB" varchar(2) NOT NULL default ' ',
  "AAAAXNOY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXNPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDHAD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDHAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABDHAD" varchar(2) NOT NULL default ' ',
  "AAAAXNPL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABDHAD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXNRA" varchar(2) NOT NULL default ' ',
  "AAAAXNRD" varchar(2) NOT NULL default ' ',
  "AAABAELT" varchar(10) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABAAQA" varchar(1) NOT NULL default ' ',
  "AAAAXNPX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXNRA","AAAAXNRD","AAABAELT","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXNRA" varchar(2) NOT NULL default ' ',
  "AAABHXLS" varchar(5) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAXNRD" varchar(2) NOT NULL default ' ',
  "AAABAELT" varchar(10) NOT NULL default ' ',
  "AAABAAQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXNRA","AAABHXLS","AAABHVYL","AAAAXNRD","AAABAELT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABAXLA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXWXP" varchar(2) NOT NULL default '00',
  "AAABKRZW" varchar(4) NOT NULL default '0000',
  "AAAAXNRS" varchar(2) NOT NULL default ' ',
  "AAAAZDYE" varchar(8) NOT NULL default ' ',
  "AAAAZDYF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAABAXLA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAAAXNWB" varchar(4) NOT NULL default ' ',
  "AAAAYDFX" varchar(3) NOT NULL default ' ',
  "AAAAXUZF" varchar(10) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAAQA" varchar(1) NOT NULL default ' ',
  "AAAAXNSQ" varchar(2) NOT NULL default '00',
  "AAABFKVH" decimal(5,2) NOT NULL default '0.00',
  "AAAAXNSS" varchar(1) NOT NULL default ' ',
  "AAAAXWZG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXNWO","AAAAXNWM","AAAAXNWB","AAAAYDFX","AAAAXUZF","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAAAXNWB" varchar(4) NOT NULL default ' ',
  "AAAAXUZF" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNTM" varchar(5) NOT NULL default ' ',
  "AAAAXNTN" varchar(5) NOT NULL default ' ',
  "AAAAXNTO" varchar(1) NOT NULL default ' ',
  "AAAAXNTP" varchar(3) NOT NULL default ' ',
  "AAAAXNTQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXNWO","AAAAXNWM","AAAAXNWB","AAAAXUZF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAAAXNWB" varchar(4) NOT NULL default ' ',
  "AAABAXLA" varchar(3) NOT NULL default ' ',
  "AAAAXUZF" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNUI" varchar(4) NOT NULL default '0000',
  "AAAAXNUJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXNWO","AAAAXNWM","AAAAXNWB","AAABAXLA","AAAAXUZF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNWD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNVJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  "AAAAXNWB" varchar(4) NOT NULL default ' ',
  "AAAAXXBS" varchar(1) NOT NULL default ' ',
  "AAAAXNWD" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNVX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWO","AAAAXNWM","AAAAXNWB","AAAAXXBS","AAAAXNWD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNWO" varchar(1) NOT NULL default ' ',
  "AAAAXNWP" varchar(2) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXNWM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNWO","AAAAXNWP","AAABMQNB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNXC" varchar(2) NOT NULL default ' ',
  "AAAAXNXD" varchar(1) NOT NULL default '0',
  "AAABLXQP" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLONY" decimal(5,2) NOT NULL default '0.00',
  "AAAAXNXA" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXNXC","AAAAXNXD","AAABLXQP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDFX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDFX" varchar(3) NOT NULL default ' ',
  "AAAAYGQC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNYH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNYM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNYR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOAO" varchar(4) NOT NULL default ' ',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNZE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAXNZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXNZY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXNZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPC" varchar(2) NOT NULL default ' ',
  "AAAAXNZN" varchar(2) NOT NULL default ' ',
  "AAAAXPAX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXNZY" varchar(1) NOT NULL default ' ',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXNZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXNZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAAAXOAJ" varchar(1) NOT NULL default '0',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPHD" varchar(1) NOT NULL default '0',
  "AAAAXOAG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXQPQ","AAAAXOAJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAXOAO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOAV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPRC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOBF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCXYD" varchar(1) NOT NULL default ' ',
  "AAAAXPRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCXYD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOCD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOCD" varchar(1) NOT NULL default ' ',
  "AAAAXOCA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOCS" varchar(2) NOT NULL default ' ',
  "AAAAXOCH" varchar(16) NOT NULL default ' ',
  "AAAAXOCI" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOCS" varchar(2) NOT NULL default ' ',
  "AAAAXOCP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAYHYO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYYWH" varchar(4) NOT NULL default ' ',
  "AAAAYLJE" varchar(20) NOT NULL default ' ',
  "AAAAXOEU" varchar(25) NOT NULL default ' ',
  "AAAAXOEV" varchar(25) NOT NULL default ' ',
  "AAAAXOEW" varchar(20) NOT NULL default ' ',
  "AAAAXOEX" varchar(20) NOT NULL default ' ',
  "AAAAXOEY" varchar(20) NOT NULL default ' ',
  "AAAAXOEZ" varchar(60) NOT NULL default ' ',
  "AAAAXOFA" varchar(3) NOT NULL default ' ',
  "AAAAXOFB" varchar(8) NOT NULL default '00000000',
  "AAAAXOFC" varchar(20) NOT NULL default ' ',
  "AAAAXOFD" varchar(20) NOT NULL default ' ',
  "AAAAXOFE" varchar(20) NOT NULL default ' ',
  "AAAAXOFF" varchar(60) NOT NULL default ' ',
  "AAAAXOFG" varchar(3) NOT NULL default ' ',
  "AAAAXOFH" varchar(8) NOT NULL default '00000000',
  "AAAAXOFI" varchar(20) NOT NULL default ' ',
  "AAAAXOFJ" varchar(20) NOT NULL default ' ',
  "AAAAXOFK" varchar(20) NOT NULL default ' ',
  "AAAAXOFL" varchar(60) NOT NULL default ' ',
  "AAAAXOFM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAAAYHYO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXODY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYHXB" varchar(1) NOT NULL default ' ',
  "AAAAYPUL" varchar(8) NOT NULL default ' ',
  "AAAAXOSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYHYO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYYWH" varchar(4) NOT NULL default ' ',
  "AAAAYLJE" varchar(20) NOT NULL default ' ',
  "AAAAXOEU" varchar(25) NOT NULL default ' ',
  "AAAAXOEV" varchar(25) NOT NULL default ' ',
  "AAAAXOEW" varchar(20) NOT NULL default ' ',
  "AAAAXOEX" varchar(20) NOT NULL default ' ',
  "AAAAXOEY" varchar(20) NOT NULL default ' ',
  "AAAAXOEZ" varchar(60) NOT NULL default ' ',
  "AAAAXOFA" varchar(3) NOT NULL default ' ',
  "AAAAXOFB" varchar(8) NOT NULL default '00000000',
  "AAAAXOFC" varchar(20) NOT NULL default ' ',
  "AAAAXOFD" varchar(20) NOT NULL default ' ',
  "AAAAXOFE" varchar(20) NOT NULL default ' ',
  "AAAAXOFF" varchar(60) NOT NULL default ' ',
  "AAAAXOFG" varchar(3) NOT NULL default ' ',
  "AAAAXOFH" varchar(8) NOT NULL default '00000000',
  "AAAAXOFI" varchar(20) NOT NULL default ' ',
  "AAAAXOFJ" varchar(20) NOT NULL default ' ',
  "AAAAXOFK" varchar(20) NOT NULL default ' ',
  "AAAAXOFL" varchar(60) NOT NULL default ' ',
  "AAAAXOFM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAAAYHYO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXOFX" varchar(4) NOT NULL default ' ',
  "AAAAYHXB" varchar(1) NOT NULL default ' ',
  "AAAAXOQS" varchar(1) NOT NULL default ' ',
  "AAAAXOQT" varchar(1) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAAAXOGC" varchar(15) NOT NULL default '000000000000000',
  "AAAAXPQL" varchar(4) NOT NULL default ' ',
  "AAAAXOGE" varchar(4) NOT NULL default ' ',
  "AAAAYKKF" varchar(1) NOT NULL default '0',
  "AAAAXORF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJXCE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABJXCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOIZ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXOIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPXV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOJP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXOJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOHO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOJX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOHT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOKF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOKN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOKV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOLD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOLL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXOLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOIZ" varchar(1) NOT NULL default '0',
  "AAAAXOIW" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABKPXV" varchar(1) NOT NULL default ' ',
  "AAAAXOJE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABKPXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOJP" varchar(2) NOT NULL default '00',
  "AAAAXOJM" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOJX" varchar(2) NOT NULL default ' ',
  "AAAAXOJU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOKF" varchar(1) NOT NULL default ' ',
  "AAAAXOKC" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOKN" varchar(1) NOT NULL default ' ',
  "AAAAXOKK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOKV" varchar(1) NOT NULL default ' ',
  "AAAAXOKS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOLD" varchar(1) NOT NULL default ' ',
  "AAABAZOC" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOLD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOLL" varchar(1) NOT NULL default '0',
  "AAAAXOLI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXOLT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABGCHU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOLZ" (
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAQKE" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMCRV","AAABGCHU","AAABKFCZ","AAABKFCV","AAABAQKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNXD" varchar(2) NOT NULL default ' ',
  "AAABHFFR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZNXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOMQ" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXONW" varchar(5) NOT NULL default ' ',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAXONW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOMW" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXONI" varchar(5) NOT NULL default '00000',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAXONI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXONC" (
  "AAAAXONI" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAXONI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXONF" (
  "AAAAXONU" varchar(5) NOT NULL default ' ',
  "AAAAXONH" varchar(5) NOT NULL default '00000',
  "AAAAXONI" varchar(5) NOT NULL default '00000',
  "AAAAXONJ" varchar(1) NOT NULL default ' ',
  "AAAAXONK" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAAAXONU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXONM" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXONU" varchar(5) NOT NULL default ' ',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAXONU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXONS" (
  "AAAAXONW" varchar(5) NOT NULL default ' ',
  "AAAAXONU" varchar(5) NOT NULL default ' ',
  "AAAAXPQC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXONW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXONX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQFC" varchar(7) NOT NULL default ' ',
  "AAAAXPHD" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXQFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOOD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPRG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPSR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXORS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAXORS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQLX" varchar(10) NOT NULL default ' ',
  "AAAAXQLY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXQLX","AAAAXQLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQLX" varchar(10) NOT NULL default ' ',
  "AAAAXOPH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQLX" varchar(10) NOT NULL default ' ',
  "AAAAXQLY" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXOPQ" varchar(4) NOT NULL default ' ',
  "AAABMAYR" varchar(16) NOT NULL default ' ',
  "AAABMAYP" int(11) NOT NULL default '0',
  "AAABMAYQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXQLX","AAAAXQLY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAZAIM" varchar(4) NOT NULL default '0000',
  "AAAAYYWH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXOS" varchar(25) NOT NULL default ' ',
  "AAABCUWM" varchar(30) NOT NULL default ' ',
  "AAAAYXOW" varchar(10) NOT NULL default ' ',
  "AAAAXOQI" varchar(25) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAAAXOQK" varchar(25) NOT NULL default ' ',
  "AAABMGVM" varchar(18) NOT NULL default ' ',
  "AAAAXOQM" varchar(15) NOT NULL default '000000000000000',
  "AAAAXOQN" varchar(4) NOT NULL default ' ',
  "AAAAXPQL" varchar(4) NOT NULL default ' ',
  "AAAAYYVT" varchar(6) NOT NULL default ' ',
  "AAABBCDL" varchar(6) NOT NULL default '000000',
  "AAAAXOQR" varchar(25) NOT NULL default ' ',
  "AAAAXOQS" varchar(1) NOT NULL default ' ',
  "AAAAXOQT" varchar(1) NOT NULL default ' ',
  "AAAAXOQU" varchar(4) NOT NULL default ' ',
  "AAABAXPS" varchar(14) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBHSN" varchar(8) NOT NULL default '00000000',
  "AAAAXOQY" varchar(20) NOT NULL default ' ',
  "AAABGRXB" varchar(60) NOT NULL default ' ',
  "AAAAXORA" varchar(20) NOT NULL default ' ',
  "AAAAXORB" varchar(20) NOT NULL default ' ',
  "AAAAXORC" varchar(10) NOT NULL default '0000000000',
  "AAAAXORD" varchar(3) NOT NULL default ' ',
  "AAAAYKKF" varchar(1) NOT NULL default '0',
  "AAAAXORF" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAZAIM","AAAAYYWH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXORL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXORS" varchar(4) NOT NULL default '0000',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXORS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXORT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXQLX" varchar(10) NOT NULL default ' ',
  "AAAAXQLY" varchar(2) NOT NULL default '00',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXQLX","AAAAXQLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXQLX" varchar(10) NOT NULL default ' ',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXQLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXOSP" varchar(1) NOT NULL default ' ',
  "AAAAXQPO" varchar(9) NOT NULL default '000000000',
  "AAAAXOSR" varchar(4) NOT NULL default ' ',
  "AAAAXOSS" varchar(4) NOT NULL default ' ',
  "AAAAXOST" varchar(14) NOT NULL default ' ',
  "AAABEIVY" varchar(4) NOT NULL default ' ',
  "AAAAXOSV" varchar(14) NOT NULL default ' ',
  "AAAAXOSW" varchar(4) NOT NULL default ' ',
  "AAAAXOSX" varchar(14) NOT NULL default ' ',
  "AAAAXOSY" varchar(4) NOT NULL default ' ',
  "AAABHOML" varchar(14) NOT NULL default ' ',
  "AAAAXOTA" varchar(4) NOT NULL default ' ',
  "AAAAXOTB" varchar(14) NOT NULL default ' ',
  "AAAAXOTC" varchar(4) NOT NULL default ' ',
  "AAAAXOTD" varchar(14) NOT NULL default ' ',
  "AAAAXOTE" varchar(4) NOT NULL default ' ',
  "AAAAXOTF" varchar(14) NOT NULL default ' ',
  "AAAAXOTG" varchar(4) NOT NULL default ' ',
  "AAAAXOTH" varchar(14) NOT NULL default ' ',
  "AAAAXOTI" varchar(4) NOT NULL default ' ',
  "AAAAXOTJ" varchar(14) NOT NULL default ' ',
  "AAAAXOTK" varchar(4) NOT NULL default '0000',
  "AAAAXOTL" varchar(4) NOT NULL default ' ',
  "AAAAXOTM" varchar(14) NOT NULL default ' ',
  "AAAAXOTN" varchar(4) NOT NULL default ' ',
  "AAAAXOTO" varchar(4) NOT NULL default ' ',
  "AAAAXOTP" varchar(14) NOT NULL default ' ',
  "AAAAXOTQ" varchar(4) NOT NULL default ' ',
  "AAAAXQFD" varchar(4) NOT NULL default ' ',
  "AAAAXOTS" varchar(14) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOTU" varchar(6) NOT NULL default ' ',
  "AAAAXOTV" varchar(10) NOT NULL default '0000000000',
  "AAAAXOTW" varchar(4) NOT NULL default ' ',
  "AAAAXOTX" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPBG" varchar(4) NOT NULL default '0000',
  "AAABIXKR" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXOUH" decimal(6,2) NOT NULL default '0.00',
  "AAAAXOUI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPBG","AAABIXKR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVMG" varchar(2) NOT NULL default '00',
  "AAAAXOVJ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXOUT" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOUU" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOUV" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOUW" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOUX" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOUY" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOUZ" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOVA" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOVB" decimal(4,1) NOT NULL default '0.0',
  "AAAAXOVC" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOVD" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOVE" decimal(9,2) NOT NULL default '0.00',
  "AAAAXOVF" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVMG","AAAAXOVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOVY" varchar(10) NOT NULL default '0000000000',
  "AAAAYPUL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOVY" varchar(10) NOT NULL default '0000000000',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOVY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOWV" varchar(4) NOT NULL default ' ',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXOWG" varchar(4) NOT NULL default ' ',
  "AAAAXOWH" decimal(15,2) NOT NULL default '0.00',
  "AAAAXOWI" decimal(5,2) NOT NULL default '0.00',
  "AAAAXOWJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAXOWK" varchar(4) NOT NULL default ' ',
  "AAABLGEL" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXOWV","AAAAXQPN","AAAAXQPQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOWV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOXD" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOYC" varchar(3) NOT NULL default '000',
  "AAAAXPHD" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXOYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOYC" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOYI" varchar(4) NOT NULL default ' ',
  "AAAAXOYC" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAEVD" decimal(5,2) NOT NULL default '0.00',
  "AAAAXOXZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOYI","AAAAXOYC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOYI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXOYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOYQ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOYQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOYR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOZD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAXOZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOZD" varchar(3) NOT NULL default '000',
  "AAABBAWZ" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXOZQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAXOZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXOZQ" varchar(3) NOT NULL default '000',
  "AAAAXOZN" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXOZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXOZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPBG" varchar(4) NOT NULL default '0000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXOZW" varchar(1) NOT NULL default ' ',
  "AAAAXOZX" varchar(2) NOT NULL default ' ',
  "AAABGWVP" varchar(1) NOT NULL default ' ',
  "AAAAXOZZ" varchar(1) NOT NULL default ' ',
  "AAAAXPAA" varchar(1) NOT NULL default ' ',
  "AAAAXPAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPBG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPAX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPAX" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPAX" varchar(4) NOT NULL default ' ',
  "AAAAXPBG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAXPAX","AAAAXPBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPBG" varchar(4) NOT NULL default '0000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQLE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPBM" decimal(12,2) NOT NULL default '0.00',
  "AAABLATN" decimal(12,2) NOT NULL default '0.00',
  "AAAAXPBO" decimal(5,2) NOT NULL default '0.00',
  "AAAAXPBP" decimal(12,2) NOT NULL default '0.00',
  "AAABLRVM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQLE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABANVG" varchar(4) NOT NULL default ' ',
  "AAABBGQC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABANVG","AAABBGQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYHYO" varchar(4) NOT NULL default ' ',
  "AAAAYYWH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAXPQL" varchar(4) NOT NULL default ' ',
  "AAABDROJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYHYO","AAAAYYWH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAAAXPCQ" varchar(2) NOT NULL default ' ',
  "AAAAXPCR" varchar(1) NOT NULL default ' ',
  "AAAAXPCS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAZTYI" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPPE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAAAZTYI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAXPHD" varchar(1) NOT NULL default '0',
  "AAAAXPPM" varchar(2) NOT NULL default '00',
  "AAAAZTYI" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXDZ" varchar(4) NOT NULL default '0000',
  "AAAAXPDR" varchar(1) NOT NULL default ' ',
  "AAAAXPDS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAAAXPHD","AAAAXPPM","AAAAZTYI","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAXPHD" varchar(1) NOT NULL default '0',
  "AAAAXPPM" varchar(2) NOT NULL default '00',
  "AAAAXPFO" varchar(1) NOT NULL default ' ',
  "AAAAXPFP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPEJ" varchar(4) NOT NULL default '0000',
  "AAAAXPEK" varchar(1) NOT NULL default ' ',
  "AAAAXPEL" varchar(1) NOT NULL default ' ',
  "AAAAXPEM" varchar(1) NOT NULL default ' ',
  "AAAAXPEN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAAAXPHD","AAAAXPPM","AAAAXPFO","AAAAXPFP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAXPHD" varchar(1) NOT NULL default '0',
  "AAAAXPPM" varchar(2) NOT NULL default '00',
  "AAAAXPFO" varchar(1) NOT NULL default ' ',
  "AAAAXPFP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPFE" varchar(1) NOT NULL default ' ',
  "AAAAXPFF" varchar(4) NOT NULL default '0000',
  "AAAAXPFG" varchar(2) NOT NULL default '00',
  "AAAAXPFH" varchar(1) NOT NULL default ' ',
  "AAAAXPFI" varchar(4) NOT NULL default '0000',
  "AAAAXPFJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAAAXPHD","AAAAXPPM","AAAAXPFO","AAAAXPFP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPPE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAXPHD" varchar(1) NOT NULL default '0',
  "AAAAXPPM" varchar(2) NOT NULL default '00',
  "AAAAZTYI" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPGE" varchar(1) NOT NULL default ' ',
  "AAAAXPGF" varchar(1) NOT NULL default ' ',
  "AAAAXPGG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAAAXPHD","AAAAXPPM","AAAAZTYI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAXPHD" varchar(1) NOT NULL default '0',
  "AAAAXPPM" varchar(2) NOT NULL default '00',
  "AAAAZTYI" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPGV" varchar(1) NOT NULL default ' ',
  "AAAAXPGW" varchar(4) NOT NULL default '0000',
  "AAAAXPGX" varchar(2) NOT NULL default '00',
  "AAAAXPGY" varchar(1) NOT NULL default ' ',
  "AAABBGIT" varchar(1) NOT NULL default ' ',
  "AAAAXPHA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAAAXPHD","AAAAXPPM","AAAAZTYI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPHQ" varchar(2) NOT NULL default ' ',
  "AAAAXPJJ" varchar(4) NOT NULL default ' ',
  "AAAAZNME" varchar(2) NOT NULL default '00',
  "AAABBHDK" decimal(13,2) NOT NULL default '0.00',
  "AAABFTKZ" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPIM" varchar(2) NOT NULL default '00',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  "AAABBHDK" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPJJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXPJJ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXPJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPJW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPJW" varchar(1) NOT NULL default ' ',
  "AAAAXPJT" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPKJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPKJ" varchar(1) NOT NULL default ' ',
  "AAAAXPKG" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPKW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPKW" varchar(2) NOT NULL default ' ',
  "AAAAXPKT" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPKW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPLK" varchar(1) NOT NULL default '0',
  "AAAAXPLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPLK" varchar(1) NOT NULL default '0',
  "AAAAXPLH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHLV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  "AAAAXPNF" varchar(4) NOT NULL default '0000',
  "AAAAXPMW" varchar(8) NOT NULL default '00000000',
  "AAAAXPLT" varchar(1) NOT NULL default ' ',
  "AAAAXPLU" varchar(1) NOT NULL default ' ',
  "AAABFHKK" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHLV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPNF" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAXPNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPMW" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAXPMW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXPMW" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLPWW" varchar(15) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXPMW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPNF" varchar(4) NOT NULL default '0000',
  "AAAAXPNC" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGEH" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPOD" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGEH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPOD" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAAAXPOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPOD" varchar(5) NOT NULL default '00000',
  "AAAAXPOA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPOD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXPOI" varchar(4) NOT NULL default ' ',
  "AAAAXPOJ" varchar(8) NOT NULL default '00000000',
  "AAAAXPOK" varchar(4) NOT NULL default ' ',
  "AAAAXPOL" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPPM" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAXPPM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPPE" varchar(3) NOT NULL default ' ',
  "AAABMDXM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXPPM" varchar(2) NOT NULL default '00',
  "AAABMDXM" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXPPM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPVA" varchar(6) NOT NULL default '000000',
  "AAAAXPVB" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPO" varchar(9) NOT NULL default '000000000',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAXPPV" varchar(5) NOT NULL default ' ',
  "AAAAXPPW" varchar(11) NOT NULL default '00000000000',
  "AAABFDPJ" varchar(8) NOT NULL default '00000000',
  "AAABFDPI" varchar(40) NOT NULL default ' ',
  "AAAAXPPZ" varchar(5) NOT NULL default ' ',
  "AAAAXPQA" varchar(1) NOT NULL default ' ',
  "AAAAXPQB" varchar(15) NOT NULL default ' ',
  "AAAAXPQC" varchar(1) NOT NULL default ' ',
  "AAABBGRP" varchar(40) NOT NULL default ' ',
  "AAABBGZK" varchar(40) NOT NULL default ' ',
  "AAABAQKT" varchar(1) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABBCDL" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(40) NOT NULL default ' ',
  "AAAAYYVT" varchar(10) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAAAXPQL" varchar(5) NOT NULL default ' ',
  "AAAAXPQM" varchar(8) NOT NULL default '00000000',
  "AAAAXPQN" varchar(8) NOT NULL default '00000000',
  "AAAAXPQO" varchar(8) NOT NULL default '00000000',
  "AAABMNMP" varchar(8) NOT NULL default '00000000',
  "AAABIIZV" varchar(1) NOT NULL default ' ',
  "AAAAXPQR" varchar(1) NOT NULL default ' ',
  "AAAAXPQS" varchar(1) NOT NULL default ' ',
  "AAABBHNF" varchar(1) NOT NULL default ' ',
  "AAAAXPQU" varchar(1) NOT NULL default ' ',
  "AAAAXPQV" varchar(8) NOT NULL default '00000000',
  "AAABAXWY" varchar(8) NOT NULL default '00000000',
  "AAAAXPQX" varchar(1) NOT NULL default ' ',
  "AAAAXPQY" varchar(1) NOT NULL default ' ',
  "AAAAXPQZ" varchar(2) NOT NULL default ' ',
  "AAAAXPRA" varchar(1) NOT NULL default ' ',
  "AAAAXPRB" varchar(1) NOT NULL default ' ',
  "AAAAXPRC" varchar(1) NOT NULL default ' ',
  "AAABBAVT" varchar(4) NOT NULL default ' ',
  "AAAAXPRE" varchar(8) NOT NULL default '00000000',
  "AAAAXPRF" varchar(8) NOT NULL default '00000000',
  "AAAAXPRG" varchar(1) NOT NULL default ' ',
  "AAAAXQFC" varchar(7) NOT NULL default ' ',
  "AAAAXPRI" varchar(15) NOT NULL default ' ',
  "AAAAXPRJ" varchar(8) NOT NULL default '00000000',
  "AAAAXPRK" varchar(2) NOT NULL default ' ',
  "AAABAOPS" varchar(2) NOT NULL default ' ',
  "AAAAXPRM" varchar(3) NOT NULL default ' ',
  "AAAAXPRN" varchar(4) NOT NULL default ' ',
  "AAAAXPRO" varchar(1) NOT NULL default '0',
  "AAAAXPRP" int(11) NOT NULL default '0',
  "AAABADDM" int(11) NOT NULL default '0',
  "AAAAXPRR" int(11) NOT NULL default '0',
  "AAAAXPRS" varchar(1) NOT NULL default ' ',
  "AAAAXPRT" int(11) NOT NULL default '0',
  "AAAAXPRU" int(11) NOT NULL default '0',
  "AAAAXPRV" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXPVA","AAAAXPVB","AAAAXQPN","AAAAXQPO","AAAAXQPQ","AAABMCRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPVA" varchar(6) NOT NULL default '000000',
  "AAAAXPVB" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPO" varchar(9) NOT NULL default '000000000',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAXPTG" varchar(8) NOT NULL default '00000000',
  "AAAAXPTH" varchar(8) NOT NULL default '00000000',
  "AAAAZOCT" varchar(2) NOT NULL default ' ',
  "AAAAXPTJ" varchar(2) NOT NULL default ' ',
  "AAAAXPTK" varchar(8) NOT NULL default '00000000',
  "AAAAXPTL" varchar(8) NOT NULL default '00000000',
  "AAAAXPSR" varchar(1) NOT NULL default ' ',
  "AAAAXPSS" varchar(5) NOT NULL default ' ',
  "AAAAXPST" int(11) NOT NULL default '0',
  "AAABBNVV" int(11) NOT NULL default '0',
  "AAAAXPSV" decimal(15,2) NOT NULL default '0.00',
  "AAAAXPSW" int(11) NOT NULL default '0',
  "AAAAXPSX" int(11) NOT NULL default '0',
  "AAAAXPSY" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXPVA","AAAAXPVB","AAAAXQPN","AAAAXQPO","AAAAXQPQ","AAABMCRV","AAAAXPTG","AAAAXPTH","AAAAZOCT","AAAAXPTJ","AAAAXPTK","AAAAXPTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPVA" varchar(6) NOT NULL default '000000',
  "AAABEHZX" varchar(4) NOT NULL default '0000',
  "AAAAXPUI" varchar(1) NOT NULL default '0',
  "AAAAXPUJ" varchar(2) NOT NULL default '00',
  "AAABJXFC" varchar(1) NOT NULL default ' ',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABHKSD" varchar(6) NOT NULL default '000000',
  "AAABLMKI" varchar(8) NOT NULL default '00000000',
  "AAABLMFS" varchar(6) NOT NULL default '000000',
  "AAABIJTI" varchar(8) NOT NULL default '00000000',
  "AAABIJTJ" varchar(6) NOT NULL default '000000',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABKWUO" varchar(12) NOT NULL default ' ',
  "AAAAXPVB" varchar(3) NOT NULL default '000',
  "AAAAXPUE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPVA","AAABEHZX","AAAAXPUI","AAAAXPUJ","AAABJXFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPVA" varchar(6) NOT NULL default '000000',
  "AAAAXPVB" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPO" varchar(9) NOT NULL default '000000000',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMCWW" varchar(3) NOT NULL default ' ',
  "AAAAXQPC" varchar(2) NOT NULL default ' ',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" int(11) NOT NULL default '0',
  "AAABFTKZ" int(11) NOT NULL default '0',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXPVA","AAAAXPVB","AAAAXQPN","AAAAXQPO","AAAAXQPQ","AAABMCRV","AAABMCWW","AAAAXQPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQAI" varchar(1) NOT NULL default ' ',
  "AAABMCTK" varchar(3) NOT NULL default ' ',
  "AAAAXQLE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPWO" decimal(12,2) NOT NULL default '0.00',
  "AAAAXQAF" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQAI","AAABMCTK","AAAAXQLE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCTK" varchar(3) NOT NULL default ' ',
  "AAAAXPZF" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPWO" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCTK","AAAAXPZF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCTK" varchar(3) NOT NULL default ' ',
  "AAAAXPZF" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPZP" decimal(5,2) NOT NULL default '0.00',
  "AAAAXPWO" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCTK","AAAAXPZF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPXW" varchar(5) NOT NULL default '00000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPXX" varchar(5) NOT NULL default '00000',
  "AAAAXPXA" varchar(1) NOT NULL default ' ',
  "AAAAXPXB" varchar(1) NOT NULL default ' ',
  "AAAAXPXC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXPXW","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPXW" varchar(5) NOT NULL default '00000',
  "AAAAXPXX" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAAAXPXW","AAAAXPXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXPXW" varchar(5) NOT NULL default '00000',
  "AAAAXPXX" varchar(5) NOT NULL default '00000',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXPXW","AAAAXPXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPYW" varchar(9) NOT NULL default '000000000',
  "AAAAXPYK" varchar(2) NOT NULL default ' ',
  "AAAAXPYL" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPYF" decimal(10,2) NOT NULL default '0.00',
  "AAAAXPYG" decimal(10,2) NOT NULL default '0.00',
  "AAAAXPYT" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXPYW","AAAAXPYK","AAAAXPYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGEX" varchar(1) NOT NULL default ' ',
  "AAAAXPYW" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPYT" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBGEX","AAAAXPYW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXPZF" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXPZD" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXPZF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQAI" varchar(1) NOT NULL default ' ',
  "AAAAXQLD" varchar(1) NOT NULL default ' ',
  "AAAAXQLE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQPM" decimal(12,2) NOT NULL default '0.00',
  "AAAAXPZP" decimal(7,4) NOT NULL default '0.0000',
  "AAAAXQAG" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQAI","AAAAXQLD","AAAAXQLE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXPZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQAI" varchar(1) NOT NULL default ' ',
  "AAAAXQAJ" varchar(1) NOT NULL default ' ',
  "AAABBGEX" varchar(1) NOT NULL default ' ',
  "AAAAXQLE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQPM" decimal(12,2) NOT NULL default '0.00',
  "AAAAXQAF" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQAG" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQAI","AAAAXQAJ","AAABBGEX","AAAAXQLE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABARXL" varchar(6) NOT NULL default ' ',
  "AAAAXQAS" varchar(1) NOT NULL default ' ',
  "AAAAXQAT" varchar(8) NOT NULL default '00000000',
  "AAAAXQAU" varchar(1) NOT NULL default ' ',
  "AAAAXQAV" varchar(8) NOT NULL default '00000000',
  "AAAAXQAW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOTX","AAABMCRV","AAABARXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQFM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQBH" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQPC" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQPN","AAAAXQFM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAAAXQFM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQBU" varchar(1) NOT NULL default ' ',
  "AAAAXQBV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPN","AAAAXQPQ","AAAAXQFM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQCH" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQCI" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQCJ" varchar(1) NOT NULL default ' ',
  "AAAAXQCK" varchar(1) NOT NULL default ' ',
  "AAAAXQCL" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQPN","AAAAXQPQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQCV" varchar(1) NOT NULL default ' ',
  "AAAAXQCW" varchar(1) NOT NULL default ' ',
  "AAAAXQCX" varchar(1) NOT NULL default ' ',
  "AAAAXQCY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQDI" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQDJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQDK" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQDL" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQDM" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQDN" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQDO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPN","AAAAXQPQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQFM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQEE" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQEF" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQPN","AAAAXQPQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQFD" varchar(4) NOT NULL default ' ',
  "AAAAXQES" varchar(4) NOT NULL default ' ',
  "AAAAXQEO" varchar(5) NOT NULL default '00000',
  "AAAAXQEP" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAAAXQFD","AAAAXQES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQFC" varchar(7) NOT NULL default ' ',
  "AAAAXQFD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQEZ" varchar(1) NOT NULL default ' ',
  "AAAAXQFA" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQFC","AAAAXQFD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXQFM" varchar(2) NOT NULL default ' ',
  "AAAAXQFJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXQFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOPS" varchar(2) NOT NULL default ' ',
  "AAAAXQFQ" varchar(1) NOT NULL default ' ',
  "AAAAXQFR" varchar(1) NOT NULL default ' ',
  "AAAAXQFS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAOPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPXV" varchar(1) NOT NULL default ' ',
  "AAAAXQLD" varchar(1) NOT NULL default ' ',
  "AAAAXQLE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQPL" decimal(10,2) NOT NULL default '0.00',
  "AAAAXQPM" decimal(10,2) NOT NULL default '0.00',
  "AAAAXQKX" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKPXV","AAAAXQLD","AAAAXQLE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAUV" varchar(2) NOT NULL default ' ',
  "AAABLIXT" varchar(2) NOT NULL default ' ',
  "AAAAZEIJ" varchar(2) NOT NULL default ' ',
  "AAAAXQGP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAUV","AAABLIXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQHD" varchar(1) NOT NULL default ' ',
  "AAAAXQLE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQPL" decimal(12,2) NOT NULL default '0.00',
  "AAAAXQPM" decimal(12,2) NOT NULL default '0.00',
  "AAAAXQHB" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQHD","AAAAXQLE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQHP" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQHL" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQHM" decimal(9,2) NOT NULL default '0.00',
  "AAAAXQHN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQHP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQLE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQPL" varchar(7) NOT NULL default '0000000',
  "AAAAXQPM" varchar(7) NOT NULL default '0000000',
  "AAABAOLK" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQLE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQLL" varchar(6) NOT NULL default ' ',
  "AAABBHOA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMMJT" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQLL","AAABBHOA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQLE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQPL" varchar(3) NOT NULL default '000',
  "AAAAXQPM" varchar(3) NOT NULL default '000',
  "AAAAXQIU" varchar(1) NOT NULL default ' ',
  "AAAAXQIV" varchar(1) NOT NULL default ' ',
  "AAAAXQIW" varchar(1) NOT NULL default ' ',
  "AAAAXQIX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQLE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQJN" varchar(2) NOT NULL default ' ',
  "AAAAXQJO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQJH" decimal(12,2) NOT NULL default '0.00',
  "AAAAXQJI" decimal(12,2) NOT NULL default '0.00',
  "AAAAXQJJ" decimal(12,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAXQJL" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAXQJN","AAAAXQJO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPO" varchar(9) NOT NULL default '000000000',
  "AAAAXQLL" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMMJT" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQPO","AAAAXQLL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPC" varchar(2) NOT NULL default ' ',
  "AAAAXQNI" varchar(2) NOT NULL default '00',
  "AAAAXQNB" varchar(2) NOT NULL default '00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABEJKE" varchar(3) NOT NULL default '000',
  "AAABAQKE" decimal(13,2) NOT NULL default '0.00',
  "AAABHDRZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPC","AAAAXQNI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPXV" varchar(1) NOT NULL default ' ',
  "AAAAXQLD" varchar(1) NOT NULL default ' ',
  "AAAAXQLE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQPL" decimal(10,2) NOT NULL default '0.00',
  "AAAAXQPM" decimal(10,2) NOT NULL default '0.00',
  "AAAAXQKX" decimal(10,2) NOT NULL default '0.00',
  "AAABEJKE" decimal(9,2) NOT NULL default '0.00',
  "AAAAXQKZ" decimal(10,2) NOT NULL default '0.00',
  "AAAAXQLA" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKPXV","AAAAXQLD","AAAAXQLE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQLG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPC" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQLL" varchar(3) NOT NULL default ' ',
  "AAAAXQLM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXQPC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAAAXQLX" varchar(10) NOT NULL default ' ',
  "AAAAXQLY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXQPQ","AAAAXQLX","AAAAXQLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAAAXQMJ" varchar(2) NOT NULL default '00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKOTX","AAABBGCX","AAAAXQMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQMT" varchar(1) NOT NULL default ' ',
  "AAAAXQMU" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQMQ" decimal(8,2) NOT NULL default '0.00',
  "AAAAXQMR" decimal(8,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQMT","AAAAXQMU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPC" varchar(2) NOT NULL default ' ',
  "AAAAXQNI" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAXQNB" varchar(2) NOT NULL default '00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABEJKE" varchar(3) NOT NULL default '000',
  "AAABAQKE" decimal(13,2) NOT NULL default '0.00',
  "AAABHDRZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPC","AAAAXQNI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXR" varchar(5) NOT NULL default ' ',
  "AAABEETX" varchar(2) NOT NULL default ' ',
  "AAAAXQNO" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLXR","AAABEETX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPO" varchar(9) NOT NULL default '000000000',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAAAXQPE" varchar(3) NOT NULL default ' ',
  "AAAAXQOK" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQOB" varchar(15) NOT NULL default ' ',
  "AAABBHCI" varchar(4) NOT NULL default ' ',
  "AAAAXQOX" varchar(5) NOT NULL default ' ',
  "AAAAXQOY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPN","AAAAXQPO","AAAAXQPQ","AAAAXQPE","AAAAXQOK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPO" varchar(9) NOT NULL default '000000000',
  "AAAAXQPC" varchar(2) NOT NULL default ' ',
  "AAAAXQPQ" varchar(3) NOT NULL default ' ',
  "AAAAXQPE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQOV" varchar(3) NOT NULL default ' ',
  "AAABBHCI" varchar(4) NOT NULL default ' ',
  "AAAAXQOX" varchar(5) NOT NULL default ' ',
  "AAAAXQOY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPN","AAAAXQPO","AAAAXQPC","AAAAXQPQ","AAAAXQPE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQPW" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQPL" varchar(3) NOT NULL default '000',
  "AAAAXQPM" varchar(3) NOT NULL default '000',
  "AAAAXQPN" varchar(3) NOT NULL default ' ',
  "AAAAXQPO" varchar(9) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXQPQ" varchar(2) NOT NULL default ' ',
  "AAABMCRV" varchar(7) NOT NULL default ' ',
  "AAAAXQPS" varchar(3) NOT NULL default ' ',
  "AAAAXQPT" varchar(2) NOT NULL default ' ',
  "AAAAXQPU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQPW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQQI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQQD" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQQE" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQQF" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQQG" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXQQI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAAAXQQO" varchar(1) NOT NULL default ' ',
  "AAAAXQQP" varchar(1) NOT NULL default ' ',
  "AAAAXQQQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAAAXUZF" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXUXK" varchar(2) NOT NULL default ' ',
  "AAAAXUWO" varchar(2) NOT NULL default ' ',
  "AAABBHDK" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQRN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXUXK","AAAAXUWO","AAABBHDK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXUXK" varchar(2) NOT NULL default ' ',
  "AAAAXUXL" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXUXK","AAAAXUXL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABABPL" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABABPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(2) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABLHXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUSV" varchar(2) NOT NULL default ' ',
  "AAAAXQTA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQUB" varchar(1) NOT NULL default ' ',
  "AAAAXQTG" varchar(1) NOT NULL default '0',
  "AAAAXQTH" varchar(1) NOT NULL default '0',
  "AAAAXQTI" varchar(1) NOT NULL default '0',
  "AAAAXQTJ" varchar(1) NOT NULL default '0',
  "AAAAXQTK" varchar(1) NOT NULL default '0',
  "AAAAXQTL" varchar(1) NOT NULL default '0',
  "AAAAXQTM" varchar(1) NOT NULL default '0',
  "AAAAXQTN" varchar(1) NOT NULL default '0',
  "AAAAXQTO" varchar(1) NOT NULL default '0',
  "AAAAXQTP" varchar(1) NOT NULL default '0',
  "AAAAXQTQ" varchar(1) NOT NULL default '0',
  "AAAAXQTR" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXQUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXQUB" varchar(1) NOT NULL default ' ',
  "AAABILTS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXQUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQUC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(2) NOT NULL default ' ',
  "AAAAXQUF" varchar(1) NOT NULL default ' ',
  "AAAAXQUG" varchar(2) NOT NULL default '00',
  "AAAAXQUH" varchar(2) NOT NULL default '00',
  "AAAAXQUI" varchar(1) NOT NULL default ' ',
  "AAAAXQUJ" varchar(2) NOT NULL default ' ',
  "AAAAXQUK" varchar(1) NOT NULL default ' ',
  "AAAAXQUL" varchar(1) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABABPL" varchar(2) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABABPL","AAABLHXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXQVG" varchar(2) NOT NULL default ' ',
  "AAAAXQVD" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXQVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXUXK" varchar(2) NOT NULL default ' ',
  "AAAAXUWO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCLR" decimal(7,2) NOT NULL default '0.00',
  "AAAAYCLS" decimal(7,2) NOT NULL default '0.00',
  "AAAAXQVQ" varchar(1) NOT NULL default ' ',
  "AAAAYADW" varchar(2) NOT NULL default '00',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAXVAI" varchar(1) NOT NULL default ' ',
  "AAAAXQVW" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUUV" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUUW" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQVZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXUXK","AAAAXUWO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXUSV" varchar(2) NOT NULL default ' ',
  "AAABAKLL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXQWW" varchar(4) NOT NULL default ' ',
  "AAAAXQWR" varchar(4) NOT NULL default ' ',
  "AAAAXQWS" varchar(3) NOT NULL default ' ',
  "AAABHPIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXQWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQXO" varchar(2) NOT NULL default '00',
  "AAAAXQXP" varchar(3) NOT NULL default ' ',
  "AAAAXQXB" varchar(4) NOT NULL default ' ',
  "AAAAXQXC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQXO","AAAAXQXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXQXO" varchar(2) NOT NULL default '00',
  "AAAAXQXP" varchar(3) NOT NULL default ' ',
  "AAAAXQXL" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXQXO","AAAAXQXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXQXV" varchar(4) NOT NULL default ' ',
  "AAAAXQXT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXQXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHZ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHAY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABGCHT","AAABGCHS","AAABGCHZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXRDU" varchar(1) NOT NULL default ' ',
  "AAAAXQYP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXRDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXQYY" varchar(10) NOT NULL default ' ',
  "AAAAXQYZ" varchar(10) NOT NULL default ' ',
  "AAAAXQZA" varchar(2) NOT NULL default ' ',
  "AAAAXQZB" varchar(1) NOT NULL default ' ',
  "AAAAXQZC" varchar(1) NOT NULL default ' ',
  "AAAAXQZD" decimal(5,2) NOT NULL default '0.00',
  "AAAAXQZE" decimal(8,2) NOT NULL default '0.00',
  "AAAAXQZF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXQZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXRDU" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYADW" varchar(2) NOT NULL default '00',
  "AAAAYDHJ" varchar(2) NOT NULL default ' ',
  "AAAAXUVA" varchar(1) NOT NULL default ' ',
  "AAAAYCZZ" varchar(6) NOT NULL default '000000',
  "AAAAXQZT" varchar(2) NOT NULL default ' ',
  "AAAAXQZU" varchar(1) NOT NULL default ' ',
  "AAAAXQZV" varchar(6) NOT NULL default '000000',
  "AAAAXQZW" varchar(16) NOT NULL default ' ',
  "AAABMLZH" varchar(1) NOT NULL default ' ',
  "AAAAXRAT" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXRDU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXRDU" varchar(1) NOT NULL default ' ',
  "AAAAXWVR" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRAK" varchar(1) NOT NULL default ' ',
  "AAAAXRAL" decimal(5,2) NOT NULL default '0.00',
  "AAAAXRAM" varchar(1) NOT NULL default ' ',
  "AAAAXRAN" decimal(8,2) NOT NULL default '0.00',
  "AAAAXRAO" decimal(5,2) NOT NULL default '0.00',
  "AAAAXRAP" decimal(5,2) NOT NULL default '0.00',
  "AAAAXRAQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAXRAR" varchar(1) NOT NULL default ' ',
  "AAAAXRAS" varchar(1) NOT NULL default ' ',
  "AAAAXRAT" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXRDU","AAAAXWVR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXWVR" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYHSV" decimal(8,2) NOT NULL default '0.00',
  "AAAAXRBG" decimal(8,2) NOT NULL default '0.00',
  "AAAAXRBH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXWVR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXROH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXREH" varchar(6) NOT NULL default ' ',
  "AAABILRJ" varchar(40) NOT NULL default ' ',
  "AAABLPWW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXROH","AAAAXREH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXREH" varchar(6) NOT NULL default ' ',
  "AAAAXRDE" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRCZ" varchar(6) NOT NULL default '000000',
  "AAAAXRDA" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXREH","AAAAXRDE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXREH" varchar(6) NOT NULL default ' ',
  "AAAAXRDE" varchar(7) NOT NULL default '0000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRCZ" varchar(6) NOT NULL default '000000',
  "AAAAXRDA" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXREH","AAAAXRDE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRDG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWCC" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAABISXO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWCC","AAABBHJS","AAAAXROH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXRDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXRDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAAAXREH" varchar(6) NOT NULL default ' ',
  "AAAAXRDZ" varchar(2) NOT NULL default ' ',
  "AAABIVOD" varchar(6) NOT NULL default ' ',
  "AAABMGOZ" varchar(1) NOT NULL default '0',
  "AAAAXWXP" varchar(2) NOT NULL default '00',
  "AAAAXRED" varchar(10) NOT NULL default ' ',
  "AAAAXREE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXROH","AAAAXREH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXREI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  "AAABISXO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAAAXROH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXREQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRFN" varchar(25) NOT NULL default ' ',
  "AAAAXRFO" varchar(1) NOT NULL default ' ',
  "AAAAXRFP" varchar(1) NOT NULL default ' ',
  "AAABAMIY" varchar(1) NOT NULL default ' ',
  "AAAAXRFR" varchar(1) NOT NULL default ' ',
  "AAAAXRFS" varchar(1) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRFY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRGE" varchar(100) NOT NULL default ' ',
  "AAAAXRGF" varchar(100) NOT NULL default ' ',
  "AAAAXRGG" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXRIN" varchar(2) NOT NULL default ' ',
  "AAAAXRHV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRGZ" varchar(2) NOT NULL default ' ',
  "AAAAXRHA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXRIN","AAAAXRHV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXRIN" varchar(2) NOT NULL default ' ',
  "AAAAXRHV" varchar(8) NOT NULL default '00000000',
  "AAABBHLN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXRIN","AAAAXRHV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXRIN" varchar(2) NOT NULL default ' ',
  "AAABBHLV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRHV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAXRIN","AAABBHLV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXRIN" varchar(2) NOT NULL default ' ',
  "AAAAXRID" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXRIN" varchar(2) NOT NULL default ' ',
  "AAABCTSD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXRIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTCA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYTCA" varchar(4) NOT NULL default ' ',
  "AAAAXRIX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYTCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRJB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAXRJL" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRJH" decimal(5,2) NOT NULL default '0.00',
  "AAAAXRJI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAAAXRJL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXRKW" varchar(1) NOT NULL default ' ',
  "AAAAXULX" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRJT" varchar(1) NOT NULL default ' ',
  "AAAAXRJU" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRJV" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRJW" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRJX" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRJY" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRJZ" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKA" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKB" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKC" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKD" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKE" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKF" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKG" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKH" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKI" decimal(6,3) NOT NULL default '0.000',
  "AAAAXRKJ" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAXRKW","AAAAXULX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXRKW" varchar(1) NOT NULL default ' ',
  "AAAAXULX" varchar(2) NOT NULL default '00',
  "AAAAXRKT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXRKW","AAAAXULX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVBE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGOU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXRLP" varchar(8) NOT NULL default ' ',
  "AAABCUHM" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXRLN" decimal(3,1) NOT NULL default '0.0',
  PRIMARY KEY ("AAABMRIL","AAAAXRLP","AAABCUHM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMORN" varchar(4) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYBWT" varchar(1) NOT NULL default ' ',
  "AAAAYBWU" varchar(1) NOT NULL default ' ',
  "AAAAYBWV" varchar(1) NOT NULL default ' ',
  "AAAAYBWW" varchar(1) NOT NULL default ' ',
  "AAAAYBWX" varchar(1) NOT NULL default ' ',
  "AAAAYBWY" varchar(1) NOT NULL default ' ',
  "AAAAYBWZ" varchar(1) NOT NULL default ' ',
  "AAAAYBXA" varchar(1) NOT NULL default ' ',
  "AAAAYBXB" varchar(1) NOT NULL default ' ',
  "AAAAYBXC" varchar(1) NOT NULL default ' ',
  "AAAAYBXD" varchar(1) NOT NULL default ' ',
  "AAAAYBXE" varchar(1) NOT NULL default ' ',
  "AAAAXRML" varchar(1) NOT NULL default ' ',
  "AAAAXRMM" varchar(1) NOT NULL default ' ',
  "AAAAXRMN" varchar(2) NOT NULL default '00',
  "AAAAXRMO" varchar(4) NOT NULL default ' ',
  "AAAAXRMP" varchar(4) NOT NULL default ' ',
  "AAAAXRMQ" varchar(4) NOT NULL default ' ',
  "AAAAXRMR" varchar(4) NOT NULL default ' ',
  "AAAAXRMS" varchar(4) NOT NULL default ' ',
  "AAAAXRMT" varchar(4) NOT NULL default ' ',
  "AAAAXRMU" varchar(1) NOT NULL default ' ',
  "AAAAXRMV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMORN","AAABBGDJ","AAABLIXT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAABEDYX" varchar(2) NOT NULL default ' ',
  "AAABHXLS" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAAAXRNM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXUHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLADO" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRNZ" (
  "AAAAXROC" varchar(7) NOT NULL default ' ',
  "AAAAXROB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAXROC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXROD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTOI" varchar(4) NOT NULL default '0000',
  "AAAAXROG" varchar(30) NOT NULL default ' ',
  "AAAAXROH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXTOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXROK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXTGG" varchar(2) NOT NULL default ' ',
  "AAABFLXW" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAAAXTGG","AAABFLXW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXROY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXTGG" varchar(2) NOT NULL default ' ',
  "AAABFLXW" varchar(2) NOT NULL default '00',
  "AAAAXRPF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAAAXTGG","AAABFLXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAAAXSJL" varchar(2) NOT NULL default ' ',
  "AAAAXRPT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  "AAABLPWW" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAAAXRQS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRQX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABAATI" varchar(20) NOT NULL default ' ',
  "AAAAXRRE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABABEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBACP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABBACP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBACP" varchar(2) NOT NULL default ' ',
  "AAABGGHA" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABBACP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABBACP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABABJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABBACP" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAAAXSZB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXSZC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABBACP","AAABFMBR","AAAAXSZB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSKC" varchar(4) NOT NULL default ' ',
  "AAAAZTSZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAAAXSKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSKC" varchar(4) NOT NULL default ' ',
  "AAABAJFY" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAAAXSKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABAAXO" varchar(4) NOT NULL default ' ',
  "AAAAXSKC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABFMBR","AAABAAXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAAAXSKC" varchar(4) NOT NULL default ' ',
  "AAAAXRVK" varchar(1) NOT NULL default ' ',
  "AAAAYZTE" varchar(12) NOT NULL default ' ',
  "AAAAYZTF" varchar(12) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMMJT" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABFMBR","AAAAXSKC","AAAAXRVK","AAAAYZTE","AAAAYZTF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRVO" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFLSG" varchar(2) NOT NULL default ' ',
  "AAAAXRVS" varchar(20) NOT NULL default ' ',
  "AAAAXRVT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVYP","AAABGYLC","AAABFLSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRVX" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFLSG" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(25) NOT NULL default ' ',
  "AAABLPWW" varchar(12) NOT NULL default ' ',
  "AAAAXSCF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHVYP","AAABGYLC","AAABFLSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABBACP" varchar(2) NOT NULL default ' ',
  "AAAAXSZB" varchar(2) NOT NULL default ' ',
  "AAAAXSZC" varchar(2) NOT NULL default ' ',
  "AAAAXSZD" varchar(3) NOT NULL default '000',
  "AAAAXSCH" varchar(3) NOT NULL default ' ',
  "AAABFLSG" varchar(2) NOT NULL default ' ',
  "AAAAXTTI" varchar(4) NOT NULL default ' ',
  "AAABCUCW" varchar(18) NOT NULL default ' ',
  "AAAAXTTJ" varchar(4) NOT NULL default ' ',
  "AAABCUCY" varchar(18) NOT NULL default ' ',
  "AAAAXTTK" varchar(4) NOT NULL default ' ',
  "AAABCUDA" varchar(18) NOT NULL default ' ',
  "AAAAXTTL" varchar(4) NOT NULL default ' ',
  "AAABDOIZ" varchar(18) NOT NULL default ' ',
  "AAAAXTTM" varchar(4) NOT NULL default ' ',
  "AAABDOJA" varchar(18) NOT NULL default ' ',
  "AAAAXRXF" varchar(4) NOT NULL default ' ',
  "AAABCUDG" varchar(18) NOT NULL default ' ',
  "AAAAXRXH" varchar(4) NOT NULL default ' ',
  "AAAAZADS" varchar(18) NOT NULL default ' ',
  "AAAAXRXJ" varchar(4) NOT NULL default ' ',
  "AAAAZADT" varchar(18) NOT NULL default ' ',
  "AAAAXRXL" varchar(4) NOT NULL default ' ',
  "AAAAZADU" varchar(18) NOT NULL default ' ',
  "AAAAXRXN" varchar(4) NOT NULL default ' ',
  "AAAAXRXO" varchar(18) NOT NULL default ' ',
  "AAAAXRXP" varchar(2) NOT NULL default ' ',
  "AAAAXRXQ" varchar(3) NOT NULL default '000',
  "AAAAXRXR" varchar(2) NOT NULL default ' ',
  "AAAAXRXS" varchar(3) NOT NULL default '000',
  "AAAAXRXT" varchar(2) NOT NULL default ' ',
  "AAAAXRXU" varchar(3) NOT NULL default '000',
  "AAAAXRXV" varchar(2) NOT NULL default ' ',
  "AAAAXRXW" varchar(3) NOT NULL default '000',
  "AAAAXRXX" varchar(2) NOT NULL default ' ',
  "AAAAXRXY" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABFMBR","AAABBACP","AAAAXSZB","AAAAXSZC","AAAAXSZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAAAXRYO" varchar(1) NOT NULL default ' ',
  "AAAAXRYP" varchar(1) NOT NULL default ' ',
  "AAABLADO" varchar(5) NOT NULL default ' ',
  "AAAAXSZB" varchar(2) NOT NULL default ' ',
  "AAABAAWV" varchar(2) NOT NULL default ' ',
  "AAAAXRYT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAAAXSZB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAAAXSZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAAAXSZB" varchar(2) NOT NULL default ' ',
  "AAAAXRZR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABABEW","AAAAXSZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXRZY" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFLSG" varchar(2) NOT NULL default ' ',
  "AAABDISV" varchar(1) NOT NULL default '0',
  "AAAAXSCF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHVYP","AAABGYLC","AAABFLSG","AAABDISV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSAK" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFLSG" varchar(2) NOT NULL default ' ',
  "AAABDISV" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABHVYP","AAABGYLC","AAABFLSG","AAABDISV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSAT" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFLSG" varchar(2) NOT NULL default ' ',
  "AAABMJWN" varchar(2) NOT NULL default '00',
  "AAAAXSAY" varchar(1) NOT NULL default ' ',
  "AAAAXSAZ" varchar(2) NOT NULL default ' ',
  "AAAAXSBA" varchar(1) NOT NULL default ' ',
  "AAAAXSBB" varchar(2) NOT NULL default ' ',
  "AAAAXSBC" varchar(1) NOT NULL default ' ',
  "AAAAXSBD" varchar(2) NOT NULL default ' ',
  "AAAAXSBE" varchar(1) NOT NULL default ' ',
  "AAAAXSBF" varchar(2) NOT NULL default ' ',
  "AAAAXSBG" varchar(1) NOT NULL default ' ',
  "AAAAXSBH" varchar(2) NOT NULL default ' ',
  "AAAAXSBI" varchar(1) NOT NULL default ' ',
  "AAAAXSBJ" varchar(2) NOT NULL default ' ',
  "AAAAXSBK" varchar(1) NOT NULL default ' ',
  "AAAAXSBL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVYP","AAABGYLC","AAABFLSG","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSBQ" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFLSG" varchar(2) NOT NULL default ' ',
  "AAABMJWN" varchar(2) NOT NULL default '00',
  "AAABBDPW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHVYP","AAABGYLC","AAABFLSG","AAABMJWN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSCC" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXSCH" varchar(3) NOT NULL default ' ',
  "AAAAXSCF" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAXSCH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABBACP" varchar(2) NOT NULL default ' ',
  "AAAAXSZB" varchar(2) NOT NULL default ' ',
  "AAAAXSZC" varchar(2) NOT NULL default ' ',
  "AAAAXSDB" varchar(2) NOT NULL default ' ',
  "AAAAXSZD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABFMBR","AAABBACP","AAAAXSZB","AAAAXSZC","AAAAXSDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXTGG" varchar(2) NOT NULL default ' ',
  "AAAAXSDH" varchar(2) NOT NULL default ' ',
  "AAAAXSDI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAAAXTGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXTGG" varchar(2) NOT NULL default ' ',
  "AAAAXSDT" varchar(30) NOT NULL default ' ',
  "AAABANPX" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAAAXTGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSEA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSEQ" varchar(2) NOT NULL default ' ',
  "AAABDSZU" varchar(2) NOT NULL default '00',
  "AAAAXSES" varchar(2) NOT NULL default ' ',
  "AAABDSZV" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABLTKC" varchar(20) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAAAXSEQ","AAABDSZU","AAAAXSES","AAABDSZV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABEGKI" varchar(2) NOT NULL default '00',
  "AAAAXTGG" varchar(2) NOT NULL default ' ',
  "AAAAXSFD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABJFGL","AAABJFHB","AAABEGKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABNF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABNF" varchar(2) NOT NULL default '00',
  "AAAAZMOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABABNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABABNF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABFMBR","AAABABNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAXO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAAXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAXO" varchar(4) NOT NULL default ' ',
  "AAAAXSHH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABAAXO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  "AAABAEOJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABABJU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSJL" varchar(2) NOT NULL default ' ',
  "AAAAXSIN" varchar(20) NOT NULL default ' ',
  "AAAAXSIO" varchar(12) NOT NULL default ' ',
  "AAAAXSIP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAAAXSJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSJL" varchar(2) NOT NULL default ' ',
  "AAAAXSJA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAAAXSJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAAAXSJL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABBHTC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSKC" varchar(4) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAAAXSJW" varchar(1) NOT NULL default ' ',
  "AAAAXTTI" varchar(4) NOT NULL default ' ',
  "AAAAXTTJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAAAXSKC","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABCADZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABCADZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABBNBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABBNBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBFWK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABBFWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBFWK" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABBFWK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBFWK" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABBFWK","AAABCADZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBFWK" varchar(2) NOT NULL default ' ',
  "AAABAAVE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABBFWK","AAABAAVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAABAAVE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABCADZ","AAABAAVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  "AAAAXSOB" varchar(1) NOT NULL default ' ',
  "AAAAXSOC" varchar(70) NOT NULL default ' ',
  "AAAAXSOD" varchar(70) NOT NULL default ' ',
  "AAAAXSOE" varchar(70) NOT NULL default ' ',
  "AAAAXSOF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABBNBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABBFWK" varchar(2) NOT NULL default ' ',
  "AAAAXSRZ" varchar(2) NOT NULL default ' ',
  "AAABKAFO" varchar(11) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABBFWK","AAAAXSRZ","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAABFVKG" varchar(2) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  "AAAAXSRZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABCADZ","AAABFVKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAVE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAAVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAVE" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABAAVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSSH" varchar(2) NOT NULL default ' ',
  "AAAAXSQL" varchar(11) NOT NULL default ' ',
  "AAAAXSQM" varchar(11) NOT NULL default ' ',
  "AAAAXSQN" varchar(11) NOT NULL default ' ',
  "AAAAXSQO" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAAAXSSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSSH" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAAAXSSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSRZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAAAXSRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAXSRZ" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAAAXSRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAVE" varchar(2) NOT NULL default ' ',
  "AAAAXSSP" varchar(2) NOT NULL default '00',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  "AAAAXSSH" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAAVE","AAAAXSSP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFLSG" varchar(2) NOT NULL default ' ',
  "AAABDISV" varchar(1) NOT NULL default '0',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAAAXSSY" varchar(2) NOT NULL default '00',
  "AAAAXSSZ" varchar(2) NOT NULL default '00',
  "AAAAXSTA" varchar(2) NOT NULL default '00',
  "AAAAXSTB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABFLSG","AAABDISV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSTH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAABMAKT" varchar(1) NOT NULL default '0',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABCADZ","AAABMAKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAWV" varchar(2) NOT NULL default ' ',
  "AAABAZXG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAAWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAWV" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABAAWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABDHAM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABDHAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABDHAM" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAABDHAM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABAAWV" varchar(2) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  "AAABDHAM" varchar(2) NOT NULL default ' ',
  "AAAAXSVS" varchar(4) NOT NULL default ' ',
  "AAAAXSVT" varchar(4) NOT NULL default ' ',
  "AAAAXSVU" varchar(4) NOT NULL default ' ',
  "AAAAXSVV" varchar(4) NOT NULL default ' ',
  "AAAAXSVW" varchar(4) NOT NULL default ' ',
  "AAAAXSVX" varchar(4) NOT NULL default ' ',
  "AAAAXSVY" varchar(4) NOT NULL default ' ',
  "AAAAXSVZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABAAWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABDHAM" varchar(2) NOT NULL default ' ',
  "AAAAXSWT" varchar(2) NOT NULL default '00',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAAAXSWN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABDHAM","AAAAXSWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABABNF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABABNF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSXF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAAAXSXK" varchar(1) NOT NULL default ' ',
  "AAAAXSXL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABCADZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSXQ" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABLQRT" varchar(20) NOT NULL default ' ',
  "AAAAXSXU" varchar(30) NOT NULL default ' ',
  "AAAAXSXV" varchar(20) NOT NULL default ' ',
  "AAAAXSXW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVYP","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSXZ" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHVYP","AAABGYLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABBACP" varchar(2) NOT NULL default ' ',
  "AAAAXSZB" varchar(2) NOT NULL default ' ',
  "AAAAXSZC" varchar(2) NOT NULL default ' ',
  "AAAAXSZD" varchar(3) NOT NULL default '000',
  "AAAAXSYR" varchar(1) NOT NULL default ' ',
  "AAAAXSYS" varchar(1) NOT NULL default ' ',
  "AAAAXSYT" varchar(1) NOT NULL default ' ',
  "AAAAXSYU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABABEW","AAABFMBR","AAABBACP","AAAAXSZB","AAAAXSZC","AAAAXSZD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABAAVE" varchar(2) NOT NULL default ' ',
  "AAAAXSZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAAVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAYBXK" varchar(2) NOT NULL default ' ',
  "AAAAXSZT" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAAAYBXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXSZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAAAYBXK" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGYLC","AAAAYBXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAAAYBXK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  "AAAAXTAZ" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXTHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTCK" varchar(2) NOT NULL default ' ',
  "AAAAXTBQ" varchar(30) NOT NULL default ' ',
  "AAAAXTBR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXTCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXTCK" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXTCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAAAXTCK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAAAXTCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAAAXTCU" varchar(2) NOT NULL default '00',
  "AAAAXTHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAAAXTCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTEG" varchar(2) NOT NULL default ' ',
  "AAAAXTCY" varchar(30) NOT NULL default ' ',
  "AAAAXTCZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXTEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXTEG" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXTEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTDK" (
  "AAAAXTFG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXTFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTDN" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXTFG" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAXTFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAXTEG" varchar(2) NOT NULL default ' ',
  "AAABABKM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAABJFGL","AAABJFHB","AAAAXTEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABHFFQ" varchar(32) NOT NULL default ' ',
  "AAAAXTHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAABJFGL","AAABJFHB","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAAAXTFG" varchar(2) NOT NULL default ' ',
  "AAABHFFQ" varchar(32) NOT NULL default ' ',
  "AAAAXTHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAAAXTFG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABHFFQ" varchar(32) NOT NULL default ' ',
  "AAAAXTHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAXTGG" varchar(2) NOT NULL default ' ',
  "AAABHFFQ" varchar(32) NOT NULL default ' ',
  "AAAAXTHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAABJFGL","AAABJFHB","AAAAXTGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAABHFFQ" varchar(32) NOT NULL default ' ',
  "AAAAXTHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAAAXUHE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABHFFQ" varchar(32) NOT NULL default ' ',
  "AAAAXTHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAAAXUHE","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXTHO" varchar(2) NOT NULL default ' ',
  "AAAAZNXD" varchar(2) NOT NULL default ' ',
  "AAABHFFQ" varchar(32) NOT NULL default ' ',
  "AAAAXTHL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXTHO","AAAAZNXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAAAXTIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABABPL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABAAXO" varchar(4) NOT NULL default ' ',
  "AAAAXTIL" varchar(10) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default '00',
  "AAABEGKI" varchar(2) NOT NULL default '00',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  "AAAAYZYW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABLFJP","AAABEGKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABETFA" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABMMJT" varchar(18) NOT NULL default ' ',
  "AAAAXTVY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABCADZ","AAABJFGL","AAABETFA","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABETFA" varchar(2) NOT NULL default '00',
  "AAABGIKF" varchar(40) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABLTIX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABCADZ","AAABJFGL","AAABETFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFJP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default '00',
  "AAAAXTKR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default '00',
  "AAAAYZZI" varchar(40) NOT NULL default ' ',
  "AAAAYZZJ" varchar(40) NOT NULL default ' ',
  "AAAAYZZK" varchar(40) NOT NULL default ' ',
  "AAAAYZZL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABABPL","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBGJO" varchar(2) NOT NULL default '00',
  "AAABABFV" varchar(1) NOT NULL default ' ',
  "AAABABEV" varchar(1) NOT NULL default ' ',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABCPET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFMBR","AAABABPL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUFO" varchar(10) NOT NULL default '0000000000',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABMMJT" varchar(18) NOT NULL default ' ',
  "AAABABCK" varchar(8) NOT NULL default '00000000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABFLQF" varchar(1) NOT NULL default ' ',
  "AAABAORB" varchar(2) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAYZRG" varchar(10) NOT NULL default '0000000000',
  "AAAAYZQV" varchar(1) NOT NULL default ' ',
  "AAABJFLQ" varchar(8) NOT NULL default '00000000',
  "AAAAXTMV" varchar(1) NOT NULL default ' ',
  "AAABHMRN" varchar(10) NOT NULL default '0000000000',
  "AAABJBTB" varchar(4) NOT NULL default ' ',
  "AAAAXTMY" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABLUFO","AAABMQBT","AAABLQVJ"),
  KEY "AAAAXTND" ("AAAAYZRG","AAABFMBR"),
  KEY "AAAAXTNG" ("AAABMRIL","AAABLUFO","AAABAWGV","AAABBGCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTNY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXTNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXTNY" varchar(3) NOT NULL default ' ',
  "AAAAXTNU" varchar(40) NOT NULL default ' ',
  "AAAAXTNV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXTNY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTOI" varchar(4) NOT NULL default '0000',
  "AAAAXTOJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXTOS" varchar(2) NOT NULL default '00',
  "AAAAXVTT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXTOI","AAAAXTOJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTOS" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXTOQ" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXTOS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAAWZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAAWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAAWZ" varchar(2) NOT NULL default ' ',
  "AAAAXTPD" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAAWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABAAWZ" varchar(2) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  "AAAAXTUX" varchar(2) NOT NULL default ' ',
  "AAAAXTPN" varchar(2) NOT NULL default '00',
  "AAAAXTPO" varchar(4) NOT NULL default ' ',
  "AAAAXTPP" varchar(4) NOT NULL default ' ',
  "AAAAXTPQ" varchar(4) NOT NULL default ' ',
  "AAAAXTPR" varchar(4) NOT NULL default ' ',
  "AAAAXTPS" varchar(1) NOT NULL default ' ',
  "AAABESXK" varchar(1) NOT NULL default ' ',
  "AAABJFGQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABAAWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAAAXTUA" varchar(2) NOT NULL default ' ',
  "AAAAXTQC" varchar(1) NOT NULL default ' ',
  "AAAAXTQD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAAAXTUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTQH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAAAXTUA" varchar(2) NOT NULL default ' ',
  "AAAAXTQM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABABPL","AAAAXTUA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYUV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABKYUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABKYUV" varchar(2) NOT NULL default '00',
  "AAAAZNSB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABKYUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCADZ" varchar(2) NOT NULL default ' ',
  "AAAAXTRH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCADZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAAAXTUA" varchar(2) NOT NULL default ' ',
  "AAAAXTUB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAAAXTUA","AAAAXTUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAAAXTUA" varchar(2) NOT NULL default ' ',
  "AAAAXTUB" varchar(2) NOT NULL default ' ',
  "AAAAXTRZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABABPL","AAAAXTUA","AAAAXTUB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUFO" varchar(10) NOT NULL default '0000000000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAAAXTSJ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBGJO" varchar(2) NOT NULL default '00',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAAAXTSP" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKYUV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLUFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLFJP" varchar(2) NOT NULL default '00',
  "AAAAXTTI" varchar(4) NOT NULL default ' ',
  "AAAAXTTJ" varchar(4) NOT NULL default ' ',
  "AAAAXTTK" varchar(4) NOT NULL default ' ',
  "AAAAXTTL" varchar(4) NOT NULL default ' ',
  "AAAAXTTM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABLFJP" varchar(2) NOT NULL default '00',
  "AAAAXTTI" varchar(4) NOT NULL default ' ',
  "AAAAXTTJ" varchar(4) NOT NULL default ' ',
  "AAAAXTTK" varchar(4) NOT NULL default ' ',
  "AAAAXTTL" varchar(4) NOT NULL default ' ',
  "AAAAXTTM" varchar(4) NOT NULL default ' ',
  "AAAAXTTN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABLFJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAAAXTUA" varchar(2) NOT NULL default ' ',
  "AAAAXTUB" varchar(2) NOT NULL default ' ',
  "AAAAXTUC" varchar(3) NOT NULL default '000',
  "AAAAZADL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAAAXTUA","AAAAXTUB","AAAAXTUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTUX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXTUX" varchar(2) NOT NULL default ' ',
  "AAAAXTUM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXTUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTUX" varchar(2) NOT NULL default ' ',
  "AAABAAUX" varchar(2) NOT NULL default '00',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABEGDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXTUX","AAABAAUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABAAWZ" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABABJU" varchar(20) NOT NULL default ' ',
  "AAABBLCE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABAAWZ","AAABJFGL","AAABJFHB","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABAAWZ" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAABMMJT" varchar(18) NOT NULL default ' ',
  "AAAAXTVY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABAAWZ","AAABJFGL","AAABJFHB","AAABLUVE","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTYL" varchar(2) NOT NULL default ' ',
  "AAAAXTWJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXTYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXTYL" varchar(2) NOT NULL default ' ',
  "AAAAXTWQ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXTYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZADL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTWZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZADL" varchar(2) NOT NULL default ' ',
  "AAAAXTXD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAAAZADL" varchar(2) NOT NULL default ' ',
  "AAAAXTXL" varchar(1) NOT NULL default ' ',
  "AAAAXTXM" varchar(1) NOT NULL default ' ',
  "AAAAXTXN" varchar(1) NOT NULL default ' ',
  "AAAAXTXO" varchar(40) NOT NULL default ' ',
  "AAAAXTXP" varchar(14) NOT NULL default ' ',
  "AAAAXTXQ" varchar(1) NOT NULL default ' ',
  "AAAAXTXR" varchar(20) NOT NULL default ' ',
  "AAAAXTXS" varchar(30) NOT NULL default ' ',
  "AAAAXTXT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAAAZADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABPL" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABPL","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXTYL" varchar(2) NOT NULL default ' ',
  "AAAAXTYM" varchar(2) NOT NULL default '00',
  "AAAAXTYJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXTYL","AAAAXTYM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUAZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXUAZ" varchar(2) NOT NULL default ' ',
  "AAAAXTYW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXUAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUFO" varchar(10) NOT NULL default '0000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLUFO","AAABMCRV","AAABKFCZ"),
  KEY "AAAAXTZK" ("AAABMRIL","AAABMCRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAAAXUAZ" varchar(2) NOT NULL default ' ',
  "AAABABJU" varchar(2) NOT NULL default ' ',
  "AAAAXTZS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABEW","AAAAXUAZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXTZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAAAXUAZ" varchar(2) NOT NULL default ' ',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAAAXUAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABEW","AAAAXUAZ","AAABFMBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAABBGJO" varchar(2) NOT NULL default '00',
  "AAAAXUAP" varchar(3) NOT NULL default '000',
  "AAAAXUAZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABEW","AAABBGJO","AAAAXUAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABABEW" varchar(2) NOT NULL default ' ',
  "AAAAXUAZ" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABFMBR" varchar(4) NOT NULL default ' ',
  "AAAAXUAW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABABEW","AAAAXUAZ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABAQKT" varchar(1) NOT NULL default ' ',
  "AAAAXUBQ" varchar(8) NOT NULL default '00000000',
  "AAAAXUBH" varchar(8) NOT NULL default '00000000',
  "AAAAXUBI" varchar(3) NOT NULL default '000',
  "AAAAXUBJ" varchar(8) NOT NULL default '00000000',
  "AAABHPNG" varchar(2) NOT NULL default ' ',
  "AAAAXUBL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGYLC","AAABAQKT","AAAAXUBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAAAXUHF" varchar(4) NOT NULL default ' ',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUBY" varchar(8) NOT NULL default ' ',
  "AAAAXUBZ" varchar(8) NOT NULL default ' ',
  "AAAAXUCA" varchar(8) NOT NULL default ' ',
  "AAABHEQK" varchar(40) NOT NULL default ' ',
  "AAABFTKZ" varchar(4) NOT NULL default ' ',
  "AAAAXUCD" varchar(4) NOT NULL default ' ',
  "AAAAXUCE" varchar(4) NOT NULL default ' ',
  "AAAAXUCF" varchar(4) NOT NULL default ' ',
  "AAAAXUCG" varchar(4) NOT NULL default ' ',
  "AAAAXUCH" varchar(4) NOT NULL default ' ',
  "AAAAXUCI" varchar(4) NOT NULL default ' ',
  "AAAAXUCJ" varchar(4) NOT NULL default ' ',
  "AAAAXUCK" varchar(4) NOT NULL default ' ',
  "AAAAXUCL" varchar(4) NOT NULL default ' ',
  "AAAAXUCM" varchar(4) NOT NULL default ' ',
  "AAAAXUCN" varchar(4) NOT NULL default ' ',
  "AAAAXUCO" varchar(4) NOT NULL default ' ',
  "AAAAXUCP" varchar(4) NOT NULL default ' ',
  "AAAAXUCQ" varchar(4) NOT NULL default ' ',
  "AAAAXUCR" varchar(4) NOT NULL default ' ',
  "AAAAXUCS" varchar(4) NOT NULL default ' ',
  "AAAAXUCT" varchar(4) NOT NULL default ' ',
  "AAAAXUCU" varchar(4) NOT NULL default ' ',
  "AAAAXUCV" varchar(4) NOT NULL default ' ',
  "AAAAXUCW" varchar(5) NOT NULL default ' ',
  "AAAAXUCX" varchar(5) NOT NULL default ' ',
  "AAAAXUCY" varchar(4) NOT NULL default ' ',
  "AAAAXUCZ" varchar(4) NOT NULL default ' ',
  "AAAAXUDA" varchar(4) NOT NULL default ' ',
  "AAAAXUDB" varchar(4) NOT NULL default ' ',
  "AAAAXUDC" varchar(4) NOT NULL default ' ',
  "AAAAXUDD" varchar(4) NOT NULL default ' ',
  "AAAAXUDE" varchar(4) NOT NULL default ' ',
  "AAAAXUDF" varchar(4) NOT NULL default ' ',
  "AAAAXUDG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUHE","AAAAXUHF","AAAAXURZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAAAXUHF" varchar(4) NOT NULL default ' ',
  "AAAAXUDR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXUHE","AAAAXUHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAAAXUHF" varchar(4) NOT NULL default ' ',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUFF" varchar(4) NOT NULL default ' ',
  "AAABHBXY" varchar(4) NOT NULL default ' ',
  "AAAAXUEH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUHE","AAAAXUHF","AAAAXURZ","AAABHVYL","AAABMRCU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAAAXUHF" varchar(4) NOT NULL default ' ',
  "AAAAXUET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUHE","AAAAXUHF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAAAXUHF" varchar(4) NOT NULL default ' ',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUFF" varchar(4) NOT NULL default ' ',
  "AAABHBXY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUHE","AAAAXUHF","AAAAXURZ","AAABMRCU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUFW" varchar(4) NOT NULL default ' ',
  "AAAAXUFQ" varchar(4) NOT NULL default ' ',
  "AAAAXUFR" varchar(4) NOT NULL default ' ',
  "AAAAXUFS" varchar(4) NOT NULL default ' ',
  "AAAAXUFT" varchar(4) NOT NULL default ' ',
  "AAAAXUFU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUFW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAAAXUHF" varchar(4) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default '00',
  "AAABEDYX" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUGF" decimal(9,2) NOT NULL default '0.00',
  "AAAAXUGG" decimal(8,3) NOT NULL default '0.000',
  "AAAAXUGH" decimal(8,3) NOT NULL default '0.000',
  "AAAAXUGI" decimal(8,3) NOT NULL default '0.000',
  "AAAAXUGJ" decimal(8,3) NOT NULL default '0.000',
  "AAABFSWG" varchar(1) NOT NULL default ' ',
  "AAABGNWR" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKRXX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUHE","AAAAXUHF","AAABGAMR","AAABEDYX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXUHE" varchar(4) NOT NULL default ' ',
  "AAAAXUHF" varchar(4) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default '00',
  "AAABEDYX" varchar(2) NOT NULL default '00',
  "AAAAXUHB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXUHE","AAAAXUHF","AAABGAMR","AAABEDYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAAAXUHO" varchar(15) NOT NULL default ' ',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAAAXUHQ" varchar(3) NOT NULL default '000',
  "AAAAXUHR" varchar(3) NOT NULL default '000',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  "AAAAXUHU" varchar(5) NOT NULL default ' ',
  "AAAAXUHV" varchar(1) NOT NULL default ' ',
  "AAAAXUHW" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAAAXULX" varchar(2) NOT NULL default '00',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAABLHMS" varchar(15) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAABLHMS" varchar(15) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAABGAMR" varchar(2) NOT NULL default '00',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAABLHMS" varchar(15) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAABGAMR" varchar(2) NOT NULL default '00',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAABLHMS" varchar(15) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXULJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXULR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAAAXULX" varchar(2) NOT NULL default '00',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAABLHMS" varchar(15) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUMS" varchar(20) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  "AAAAXUMV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUNM" varchar(8) NOT NULL default '00000000',
  "AAAAXUNN" varchar(7) NOT NULL default '0000000',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXURZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAAAXUOK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXURZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGAMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default '00',
  "AAAAXUOX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGAMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUPG" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUPH" varchar(60) NOT NULL default ' ',
  "AAABMJON" varchar(2) NOT NULL default ' ',
  "AAAAXUPJ" varchar(60) NOT NULL default ' ',
  "AAAAXUPK" varchar(80) NOT NULL default ' ',
  "AAAAXUPL" varchar(80) NOT NULL default ' ',
  "AAAAXUPM" varchar(80) NOT NULL default ' ',
  "AAAAXUPN" varchar(60) NOT NULL default ' ',
  "AAAAXUPO" varchar(4) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAABGAMR" varchar(2) NOT NULL default '00',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAABLHMS" varchar(15) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAABGAMR" varchar(2) NOT NULL default '00',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAABLHMS" varchar(15) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXURM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXURU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXURZ" varchar(2) NOT NULL default '00',
  "AAABGAMR" varchar(2) NOT NULL default '00',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAABLHMS" varchar(15) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUSV" varchar(2) NOT NULL default ' ',
  "AAAAXUUX" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUSX" varchar(2) NOT NULL default ' ',
  "AAAAXUSY" varchar(1) NOT NULL default ' ',
  "AAAAXUSZ" varchar(30) NOT NULL default ' ',
  "AAABLHMS" varchar(15) NOT NULL default ' ',
  "AAAAXUTB" varchar(30) NOT NULL default ' ',
  "AAABLYNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXUTN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAXUTN" varchar(8) NOT NULL default ' ',
  "AAAAXUTK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAXUTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXUXV" varchar(3) NOT NULL default ' ',
  "AAABIUYL" varchar(1) NOT NULL default ' ',
  "AAAAXUTS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXUXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXUXV" varchar(3) NOT NULL default ' ',
  "AAAAXUTZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXUXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAH" varchar(2) NOT NULL default ' ',
  "AAAAXUXK" varchar(2) NOT NULL default ' ',
  "AAAAXUWO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUUK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXVAH","AAAAXUXK","AAAAXUWO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAI" varchar(3) NOT NULL default ' ',
  "AAAAXUVJ" varchar(6) NOT NULL default '000000',
  "AAAAXUUU" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUUV" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUUW" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUUX" varchar(1) NOT NULL default ' ',
  "AAAAXUUY" varchar(1) NOT NULL default ' ',
  "AAAAXUUZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUVA" varchar(1) NOT NULL default ' ',
  "AAAAYCZZ" varchar(6) NOT NULL default '000000',
  "AAAAXUZH" varchar(1) NOT NULL default ' ',
  "AAAAXUVD" varchar(1) NOT NULL default ' ',
  "AAAAXUVE" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUVF" decimal(5,2) NOT NULL default '0.00',
  "AAAAXUVG" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXVAI","AAAAXUVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAXVAI" varchar(3) NOT NULL default ' ',
  "AAAAXUVO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAXVAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAH" varchar(2) NOT NULL default ' ',
  "AAAAXUXK" varchar(2) NOT NULL default ' ',
  "AAAAXUWO" varchar(2) NOT NULL default ' ',
  "AAAAXUWP" varchar(2) NOT NULL default '00',
  "AAAAXUWQ" varchar(2) NOT NULL default '00',
  "AAAAXUWR" varchar(2) NOT NULL default '00',
  "AAAAXUWS" varchar(2) NOT NULL default '00',
  "AAAAXUWT" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUWE" varchar(4) NOT NULL default ' ',
  "AAABMQVX" decimal(7,2) NOT NULL default '0.00',
  "AAAAXUYV" decimal(7,2) NOT NULL default '0.00',
  "AAAAXUXV" varchar(3) NOT NULL default ' ',
  "AAAAXUXW" varchar(3) NOT NULL default ' ',
  "AAAAXUYR" varchar(3) NOT NULL default ' ',
  "AAAAXUYS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVAH","AAAAXUXK","AAAAXUWO","AAAAXUWP","AAAAXUWQ","AAAAXUWR","AAAAXUWS","AAAAXUWT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAH" varchar(2) NOT NULL default ' ',
  "AAAAXUXK" varchar(2) NOT NULL default ' ',
  "AAAAXUXL" varchar(2) NOT NULL default ' ',
  "AAABBYEZ" varchar(1) NOT NULL default '0',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUXD" varchar(4) NOT NULL default ' ',
  "AAABMQVX" decimal(7,2) NOT NULL default '0.00',
  "AAAAXUYV" decimal(7,2) NOT NULL default '0.00',
  "AAAAXUYS" varchar(3) NOT NULL default ' ',
  "AAAAXUYR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVAH","AAAAXUXK","AAAAXUXL","AAABBYEZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUYR" varchar(3) NOT NULL default ' ',
  "AAAAXUYS" varchar(3) NOT NULL default ' ',
  "AAAAXUXV" varchar(3) NOT NULL default ' ',
  "AAAAXUXW" varchar(3) NOT NULL default ' ',
  "AAAAXUYW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVAH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUYR" varchar(3) NOT NULL default ' ',
  "AAAAXUYS" varchar(3) NOT NULL default ' ',
  "AAAAXUYW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVAH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUYR" varchar(3) NOT NULL default ' ',
  "AAAAXUYS" varchar(3) NOT NULL default ' ',
  "AAAAXUYT" varchar(4) NOT NULL default ' ',
  "AAABMQVX" decimal(7,2) NOT NULL default '0.00',
  "AAAAXUYV" decimal(7,2) NOT NULL default '0.00',
  "AAAAXUYW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVAH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUZF" decimal(6,3) NOT NULL default '0.000',
  "AAAAXUZG" varchar(4) NOT NULL default ' ',
  "AAAAXUZH" varchar(1) NOT NULL default ' ',
  "AAAAXUZI" decimal(6,3) NOT NULL default '0.000',
  "AAAAXUZJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVAH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXUZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAH" varchar(2) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXUZT" varchar(1) NOT NULL default ' ',
  "AAAAXUZU" varchar(1) NOT NULL default ' ',
  "AAAAXUZV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVAH","AAABCAKN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVAH" varchar(2) NOT NULL default ' ',
  "AAAAXVAI" varchar(3) NOT NULL default ' ',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAAAXVAF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVAH","AAAAXVAI","AAABLIXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAAAXVAO" varchar(1) NOT NULL default ' ',
  "AAAAYBEA" varchar(1) NOT NULL default ' ',
  "AAAAZNAJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVHC" varchar(2) NOT NULL default '00',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVBA" varchar(1) NOT NULL default ' ',
  "AAAAXVBB" varchar(1) NOT NULL default ' ',
  "AAAAXVBC" varchar(1) NOT NULL default ' ',
  "AAAAXVBD" varchar(1) NOT NULL default ' ',
  "AAAAXVBE" varchar(2) NOT NULL default ' ',
  "AAAAXVBF" varchar(1) NOT NULL default ' ',
  "AAAAXVBG" varchar(1) NOT NULL default ' ',
  "AAAAXVBH" varchar(2) NOT NULL default ' ',
  "AAAAXVBI" varchar(1) NOT NULL default ' ',
  "AAAAXVBJ" varchar(1) NOT NULL default ' ',
  "AAAAXVBK" varchar(1) NOT NULL default ' ',
  "AAAAXVBL" varchar(1) NOT NULL default ' ',
  "AAAAXVBM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVHC","AAABBGOU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAAAXVCJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVBY" varchar(1) NOT NULL default '0',
  "AAAAXVBZ" varchar(1) NOT NULL default ' ',
  "AAAAXVCA" varchar(3) NOT NULL default '000',
  "AAAAXVCB" varchar(3) NOT NULL default ' ',
  "AAAAXVCC" varchar(1) NOT NULL default ' ',
  "AAAAXVCD" varchar(3) NOT NULL default '000',
  "AAAAXVCE" varchar(3) NOT NULL default ' ',
  "AAAAXVCF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABGCHE","AAAAXVCJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVEB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXVEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVEB" varchar(4) NOT NULL default ' ',
  "AAAAXVCX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXVEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVFB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXVFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVFB" varchar(4) NOT NULL default ' ',
  "AAAAXVDO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXVFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVEB" varchar(4) NOT NULL default ' ',
  "AAAAXVFB" varchar(4) NOT NULL default ' ',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXVEB","AAAAXVFB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVFB" varchar(4) NOT NULL default ' ',
  "AAAAXVFC" varchar(4) NOT NULL default ' ',
  "AAABBLTD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVEL" varchar(1) NOT NULL default ' ',
  "AAAAXVEM" varchar(2) NOT NULL default ' ',
  "AAAAXVEN" varchar(2) NOT NULL default ' ',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  "AAAAXVEP" varchar(1) NOT NULL default ' ',
  "AAABBHNX" varchar(1) NOT NULL default ' ',
  "AAAAYWUQ" varchar(1) NOT NULL default ' ',
  "AAAAYWUR" varchar(1) NOT NULL default ' ',
  "AAAAYWUS" varchar(1) NOT NULL default ' ',
  "AAAAYWUT" varchar(1) NOT NULL default ' ',
  "AAAAYWUU" varchar(1) NOT NULL default ' ',
  "AAAAYWUV" varchar(1) NOT NULL default ' ',
  "AAAAYWUW" varchar(1) NOT NULL default ' ',
  "AAAAYWUX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXVFB","AAAAXVFC","AAABBLTD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVGO" varchar(4) NOT NULL default ' ',
  "AAAAXVSH" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBLTD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXVGO","AAAAXVSH","AAABHVYL","AAABBLTD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVGO" varchar(4) NOT NULL default ' ',
  "AAAAXVSH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXVGO","AAAAXVSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVGO" varchar(4) NOT NULL default ' ',
  "AAAAXVSH" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXVGO","AAAAXVSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVHC" varchar(2) NOT NULL default '00',
  "AAABJBVB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVHC" varchar(2) NOT NULL default '00',
  "AAAAXVHD" varchar(2) NOT NULL default ' ',
  "AAAAXVHE" varchar(1) NOT NULL default ' ',
  "AAAAXVHF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVIF" varchar(2) NOT NULL default ' ',
  "AAAAXZPU" varchar(2) NOT NULL default ' ',
  "AAABLKIH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXVIF","AAAAXZPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVIF" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZPU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXVIF","AAABBEON","AAABBEOO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHEQK" varchar(40) NOT NULL default ' ',
  "AAABFREQ" varchar(8) NOT NULL default ' ',
  "AAAAXVIR" varchar(1) NOT NULL default ' ',
  "AAAAXVIS" varchar(1) NOT NULL default ' ',
  "AAAAXVIT" varchar(1) NOT NULL default ' ',
  "AAAAXVIU" varchar(1) NOT NULL default ' ',
  "AAAAXVIV" varchar(1) NOT NULL default ' ',
  "AAAAXVIW" varchar(1) NOT NULL default ' ',
  "AAAAXVIX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWCC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCOI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWCC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVQT" varchar(1) NOT NULL default ' ',
  "AAABCVFN" varchar(2) NOT NULL default ' ',
  "AAAAZNHR" varchar(8) NOT NULL default ' ',
  "AAAAXVRF" varchar(1) NOT NULL default ' ',
  "AAAAXVRG" varchar(1) NOT NULL default ' ',
  "AAAAXWZH" varchar(1) NOT NULL default ' ',
  "AAAAXVRI" varchar(1) NOT NULL default ' ',
  "AAAAXVRJ" varchar(1) NOT NULL default ' ',
  "AAAAXZSJ" varchar(30) NOT NULL default ' ',
  "AAAAXVJZ" varchar(1) NOT NULL default ' ',
  "AAABIVOD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZNNR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNVH" varchar(1) NOT NULL default ' ',
  "AAAAZNFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZNVH","AAAAZNFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVLJ" varchar(5) NOT NULL default ' ',
  "AAABAWCC" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXVLM" varchar(8) NOT NULL default ' ',
  "AAAAXVLN" varchar(6) NOT NULL default ' ',
  "AAAAZYHK" varchar(6) NOT NULL default '000000',
  "AAAAXVKT" varchar(2) NOT NULL default '00',
  "AAABLHXQ" varchar(2) NOT NULL default '00',
  "AAAAXVKV" varchar(2) NOT NULL default '00',
  "AAAAXVKW" varchar(2) NOT NULL default '00',
  "AAAAXVKX" varchar(128) NOT NULL default ' ',
  "AAAAXVKY" varchar(128) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAZNBH" varchar(20) NOT NULL default ' ',
  "AAAAXVLF" varchar(1) NOT NULL default ' ',
  "AAAAXWOX" varchar(8) NOT NULL default ' ',
  "AAABCSVP" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVLJ","AAABAWCC","AAABBHJS","AAAAXVLM","AAAAXVLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVMG" varchar(2) NOT NULL default '00',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAAAYBSQ" varchar(6) NOT NULL default ' ',
  "AAAAXVMO" varchar(1) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAXVMB" varchar(6) NOT NULL default '000000',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAAAXVMD" varchar(1) NOT NULL default ' ',
  "AAAAXVME" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVMG","AAABKOTX","AAABBGCX","AAABAWGV","AAABMCVI","AAABBHJS","AAAAZNNR","AAAAYBSQ","AAAAXVMO","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVMZ" varchar(8) NOT NULL default ' ',
  "AAAAXVNA" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAAAYBSQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVMZ","AAAAXVNA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAXVNW" varchar(1) NOT NULL default ' ',
  "AAAAXVNX" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVNL" varchar(1) NOT NULL default ' ',
  "AAAAXVNM" varchar(1) NOT NULL default ' ',
  "AAAAXVNN" varchar(1) NOT NULL default ' ',
  "AAAAXVNO" varchar(1) NOT NULL default ' ',
  "AAAAXVNP" varchar(1) NOT NULL default ' ',
  "AAAAZWEP" varchar(1) NOT NULL default ' ',
  "AAAAXVSH" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAMR","AAABHVYL","AAAAXVNW","AAAAXVNX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXVON" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAAAZNNS" varchar(6) NOT NULL default ' ',
  "AAAAXVOX" decimal(3,2) NOT NULL default '0.00',
  "AAAAXVOY" decimal(3,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAAAXVON","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABCVFN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAAAXVOX" decimal(3,2) NOT NULL default '0.00',
  "AAAAXVOY" decimal(3,2) NOT NULL default '0.00',
  "AAAAZNNS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABCVFN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNVH" varchar(1) NOT NULL default ' ',
  "AAAAXWZG" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZOYI" varchar(20) NOT NULL default ' ',
  "AAAAZOYJ" varchar(25) NOT NULL default ' ',
  "AAAAZNGL" varchar(25) NOT NULL default ' ',
  "AAABKHGO" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAAAZNHT" varchar(15) NOT NULL default ' ',
  "AAAAZNHU" varchar(10) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZNVH","AAAAXWZG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVQF" decimal(4,2) NOT NULL default '0.00',
  "AAAAXVQG" decimal(4,2) NOT NULL default '0.00',
  "AAAAXVQH" decimal(4,2) NOT NULL default '0.00',
  "AAAAXVQI" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAZNNR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAAAZNNS" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZOYI" varchar(20) NOT NULL default ' ',
  "AAAAXVQT" varchar(1) NOT NULL default ' ',
  "AAABCVFN" varchar(2) NOT NULL default ' ',
  "AAAAZOYJ" varchar(25) NOT NULL default ' ',
  "AAAAZNGL" varchar(25) NOT NULL default ' ',
  "AAABKHGO" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAAAZNHR" varchar(8) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAAAZNHT" varchar(15) NOT NULL default ' ',
  "AAAAZNHU" varchar(10) NOT NULL default ' ',
  "AAAAXVRF" varchar(1) NOT NULL default ' ',
  "AAAAXVRG" varchar(1) NOT NULL default ' ',
  "AAAAXWZH" varchar(1) NOT NULL default ' ',
  "AAAAXVRI" varchar(1) NOT NULL default ' ',
  "AAAAXVRJ" varchar(1) NOT NULL default ' ',
  "AAAAXVRK" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZNNR","AAAAZNNS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZNVH" varchar(1) NOT NULL default ' ',
  "AAAAZNKF" varchar(1) NOT NULL default ' ',
  "AAAAZSAE" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZNVH","AAAAZNKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABHPNG" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVSH" varchar(5) NOT NULL default ' ',
  "AAAAZNVH" varchar(1) NOT NULL default ' ',
  "AAAAXVSJ" varchar(4) NOT NULL default ' ',
  "AAAAXVSK" varchar(4) NOT NULL default ' ',
  "AAAAXVSL" varchar(4) NOT NULL default ' ',
  "AAAAXVSM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABHPNG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNVH" varchar(1) NOT NULL default ' ',
  "AAAAZNKF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZNVH","AAAAZNKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVSZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAAAZNML" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAZNNR","AAAAZNML","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVTK" (
  "AAAAZNTB" varchar(4) NOT NULL default '0000',
  "AAABLWJT" varchar(57) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZNTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVTR" (
  "AAAAXVTU" varchar(8) NOT NULL default '00000000',
  "AAAAXVTT" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXVTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVWI" varchar(1) NOT NULL default ' ',
  "AAAAXVWJ" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAXVWI","AAAAXVWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAWCC" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXVUR" varchar(8) NOT NULL default '00000000',
  "AAAAXVUJ" varchar(2) NOT NULL default '00',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLVVL" varchar(2) NOT NULL default '00',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAAAZNBH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAWCC","AAABBHJS","AAAAXVUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLLNS" varchar(4) NOT NULL default ' ',
  "AAABHIPZ" varchar(3) NOT NULL default ' ',
  "AAABLYMH" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKMWI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLLNS","AAABHIPZ","AAABLYMH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVVF" (
  "AAAAXVVL" varchar(3) NOT NULL default '000',
  "AAAAZEDC" varchar(57) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXVVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVVR" varchar(2) NOT NULL default ' ',
  "AAAAXVVP" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABGCKI" varchar(4) NOT NULL default '0000',
  "AAABMCMR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABGCKI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXVWI" varchar(1) NOT NULL default ' ',
  "AAAAXVWJ" varchar(1) NOT NULL default ' ',
  "AAAAZAZL" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXVWI","AAAAXVWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYMH" varchar(2) NOT NULL default '00',
  "AAAAXYXU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVXG" varchar(1) NOT NULL default ' ',
  "AAABLRYT" varchar(1) NOT NULL default ' ',
  "AAABBKQO" varchar(1) NOT NULL default ' ',
  "AAAAXVXJ" varchar(2) NOT NULL default '00',
  "AAAAXVXK" varchar(2) NOT NULL default '00',
  "AAABLPWW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYMH","AAAAXYXU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYMH" varchar(2) NOT NULL default '00',
  "AAAAXYXU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVXG" varchar(1) NOT NULL default ' ',
  "AAABLRYT" varchar(1) NOT NULL default ' ',
  "AAABBKQO" varchar(1) NOT NULL default ' ',
  "AAAAXVXJ" varchar(2) NOT NULL default '00',
  "AAAAXVXK" varchar(2) NOT NULL default '00',
  "AAABLPWW" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYMH","AAAAXYXU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYXW" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVXV" decimal(9,2) NOT NULL default '0.00',
  "AAAAXVXW" varchar(1) NOT NULL default ' ',
  "AAAAXVXX" varchar(2) NOT NULL default ' ',
  "AAAAXVXY" decimal(5,3) NOT NULL default '0.000',
  "AAAAXVXZ" decimal(5,3) NOT NULL default '0.000',
  "AAAAXVYA" decimal(5,3) NOT NULL default '0.000',
  "AAAAXVYB" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAXYXW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYXW" varchar(2) NOT NULL default ' ',
  "AAAAXVYU" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVYL" varchar(4) NOT NULL default '0000',
  "AAAAXVYM" decimal(4,2) NOT NULL default '0.00',
  "AAAAXVYN" varchar(5) NOT NULL default ' ',
  "AAAAXVYO" varchar(3) NOT NULL default '000',
  "AAAAXVYP" varchar(2) NOT NULL default ' ',
  "AAAAXVYQ" varchar(2) NOT NULL default ' ',
  "AAAAXVYR" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXYXW","AAAAXVYU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVZE" varchar(4) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAXVZB" varchar(8) NOT NULL default ' ',
  "AAAAZNHR" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXVZK" varchar(2) NOT NULL default '00',
  "AAAAZWZD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXVZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWAC" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXVZQ" varchar(4) NOT NULL default '0000',
  "AAAAXVZR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWAC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXVZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXWAC" varchar(1) NOT NULL default ' ',
  "AAABILTS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXWAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMJXW" varchar(1) NOT NULL default ' ',
  "AAAAXWAT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMJXW","AAAAXWAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMJXW" varchar(1) NOT NULL default ' ',
  "AAAAXWAT" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMJXW","AAAAXWAT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWAU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWDG" varchar(1) NOT NULL default ' ',
  "AAAAXWDH" varchar(2) NOT NULL default ' ',
  "AAAAYDVJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWDG","AAAAXWDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXWDG" varchar(1) NOT NULL default ' ',
  "AAAAXWDH" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXWDG","AAAAXWDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAWET" varchar(2) NOT NULL default '00',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXWCU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJFHB","AAABAWET","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABAWET" varchar(2) NOT NULL default '00',
  "AAAAXWCU" varchar(2) NOT NULL default ' ',
  "AAAAXWCP" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJFHB","AAABAWET","AAAAXWCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWDG" varchar(1) NOT NULL default ' ',
  "AAAAXWDH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXWDB" decimal(9,2) NOT NULL default '0.00',
  "AAAAXWDC" varchar(2) NOT NULL default ' ',
  "AAAAXWDD" varchar(4) NOT NULL default ' ',
  "AAAAXWDE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWDG","AAAAXWDH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYUW" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYXW" varchar(2) NOT NULL default ' ',
  "AAAAXYXX" varchar(2) NOT NULL default ' ',
  "AAAAXYXY" varchar(2) NOT NULL default ' ',
  "AAAAXYXZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYYT" varchar(10) NOT NULL default ' ',
  "AAAAXYYU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYUW","AAABMLTD","AAABMCRM","AAABMCRJ","AAABMCRK","AAABMCXJ","AAABLKML","AAABKROQ","AAABKROR","AAABMCXI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAXWFE" varchar(4) NOT NULL default ' ',
  "AAAAXWFF" varchar(4) NOT NULL default ' ',
  "AAABLXYU" varchar(70) NOT NULL default ' ',
  "AAAAYTCA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAXWFE","AAAAXWFF","AAABLXYU","AAAAYTCA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWFJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWJJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYBSA" varchar(1) NOT NULL default ' ',
  "AAAAXWFP" varchar(1) NOT NULL default ' ',
  "AAAAXWFQ" varchar(1) NOT NULL default ' ',
  "AAAAXWZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWJJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXWJJ" varchar(4) NOT NULL default ' ',
  "AAAAXWKD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXWJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWGY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWGI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXWGY" varchar(4) NOT NULL default ' ',
  "AAAAYCYP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXWGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWJJ" varchar(4) NOT NULL default ' ',
  "AAAAXWGY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAXWJJ","AAAAXWGY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXWJJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAXWZH" varchar(1) NOT NULL default ' ',
  "AAAAYFXI" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABHFAO" varchar(1) NOT NULL default ' ',
  "AAAAXWHS" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWHT" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWHU" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWHV" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWHW" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWHX" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWHY" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWHZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWIA" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWIB" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWIC" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWID" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWIE" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWIF" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWIG" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAAAXWIJ" varchar(1) NOT NULL default ' ',
  "AAAAXWIK" varchar(1) NOT NULL default ' ',
  "AAAAXWIL" varchar(1) NOT NULL default ' ',
  "AAAAXWIM" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAXWZH","AAAAYFXI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWIU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAXWJJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAXWIZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWJA" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWJB" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWJC" decimal(15,2) NOT NULL default '0.00',
  "AAABHWTO" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWJE" decimal(15,2) NOT NULL default '0.00',
  "AAAAXWJF" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAXWJJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWJY" varchar(2) NOT NULL default ' ',
  "AAAAXWJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXWJY" varchar(2) NOT NULL default ' ',
  "AAAAXWJV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXWJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXWKQ" varchar(1) NOT NULL default ' ',
  "AAAAXWKD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXWKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWKL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWKQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWKR" (
  "AAAAZNVH" varchar(1) NOT NULL default ' ',
  "AAAAXWKZ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZSAE" varchar(40) NOT NULL default ' ',
  "AAABMCNE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZNVH","AAAAXWKZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWLB" (
  "AAABJDAN" varchar(1) NOT NULL default ' ',
  "AAABDWNR" varchar(15) NOT NULL default ' ',
  "AAAAXWLE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABJDAN","AAABDWNR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWLH" (
  "AAAAXWLT" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAXWLT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWLN" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXWLT" varchar(3) NOT NULL default '000',
  "AAAAXWLQ" varchar(20) NOT NULL default ' ',
  "AAABBBDP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAXWLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABCVFN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAAAZNNS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABCVFN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAZNNR" varchar(8) NOT NULL default ' ',
  "AAAAZNNS" varchar(6) NOT NULL default ' ',
  "AAAAXWOT" varchar(6) NOT NULL default '000000',
  "AAAAXWMP" varchar(8) NOT NULL default '00000000',
  "AAAAXWMQ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAAAZNNR","AAAAZNNS","AAAAXWOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWOR" varchar(2) NOT NULL default '00',
  "AAAAZNHR" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAAAXZSJ" varchar(50) NOT NULL default ' ',
  "AAAAXWNF" varchar(8) NOT NULL default ' ',
  "AAAAXWPH" varchar(8) NOT NULL default ' ',
  "AAAAXWNH" varchar(8) NOT NULL default ' ',
  "AAAAXWNI" varchar(8) NOT NULL default ' ',
  "AAABJHAF" varchar(8) NOT NULL default '00000000',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWOR","AAAAZNHR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWOV" varchar(3) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXWOR" varchar(2) NOT NULL default '00',
  "AAAAZNHR" varchar(8) NOT NULL default ' ',
  "AAAAXWOT" varchar(6) NOT NULL default '000000',
  "AAABIISD" varchar(8) NOT NULL default '00000000',
  "AAABGHTH" varchar(6) NOT NULL default '000000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAXWOV","AAABMCVI","AAABBHJS","AAAAXWOR","AAAAZNHR","AAAAXWOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWOV" varchar(3) NOT NULL default ' ',
  "AAAAXWOW" varchar(15) NOT NULL default ' ',
  "AAAAXWOX" varchar(15) NOT NULL default ' ',
  "AAABMCUF" varchar(6) NOT NULL default '000000',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXWOR" varchar(2) NOT NULL default '00',
  "AAAAZNHR" varchar(8) NOT NULL default ' ',
  "AAAAXWOT" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAXWOV","AAAAXWOW","AAAAXWOX","AAABMCUF","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAZNHR" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXWPH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAAAZNHR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWQN" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXWZL" varchar(2) NOT NULL default ' ',
  "AAAAXWPT" varchar(3) NOT NULL default ' ',
  "AAAAXWPU" varchar(3) NOT NULL default ' ',
  "AAAAXWPV" varchar(3) NOT NULL default ' ',
  "AAAAXWPW" varchar(3) NOT NULL default ' ',
  "AAAAXWPX" varchar(3) NOT NULL default ' ',
  "AAAAXWPY" varchar(3) NOT NULL default ' ',
  "AAAAXWPZ" varchar(3) NOT NULL default ' ',
  "AAAAXWQA" varchar(3) NOT NULL default ' ',
  "AAAAXWQB" varchar(3) NOT NULL default ' ',
  "AAAAXWQC" varchar(3) NOT NULL default ' ',
  "AAAAXWQD" varchar(3) NOT NULL default ' ',
  "AAAAXWQE" varchar(3) NOT NULL default ' ',
  "AAAAXWQF" varchar(3) NOT NULL default ' ',
  "AAAAXWQG" varchar(3) NOT NULL default ' ',
  "AAAAXWQH" varchar(3) NOT NULL default ' ',
  "AAAAXWVR" varchar(1) NOT NULL default ' ',
  "AAAAXWVS" decimal(5,2) NOT NULL default '0.00',
  "AAAAXWVT" decimal(7,2) NOT NULL default '0.00',
  "AAAAXWVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWQN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWQP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWYI" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXWQU" decimal(4,2) NOT NULL default '0.00',
  "AAAAXWQV" decimal(4,2) NOT NULL default '0.00',
  "AAAAXWQW" decimal(4,2) NOT NULL default '0.00',
  "AAAAXWQX" decimal(9,2) NOT NULL default '0.00',
  "AAAAXWQY" decimal(9,2) NOT NULL default '0.00',
  "AAAAXWQZ" decimal(9,2) NOT NULL default '0.00',
  "AAAAXWRA" decimal(9,2) NOT NULL default '0.00',
  "AAAAXWRB" varchar(2) NOT NULL default ' ',
  "AAAAXWRC" decimal(4,2) NOT NULL default '0.00',
  "AAAAXWRD" varchar(1) NOT NULL default ' ',
  "AAAAXWRE" varchar(1) NOT NULL default ' ',
  "AAAAXWRF" varchar(1) NOT NULL default ' ',
  "AAAAXWRG" decimal(4,2) NOT NULL default '0.00',
  "AAAAXWRH" decimal(4,2) NOT NULL default '0.00',
  "AAAAXWRI" decimal(9,2) NOT NULL default '0.00',
  "AAAAXWRJ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYCHH" varchar(4) NOT NULL default ' ',
  "AAAAXWRL" varchar(4) NOT NULL default ' ',
  "AAAAXWRM" varchar(4) NOT NULL default ' ',
  "AAAAYCCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWYI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWYF" varchar(2) NOT NULL default '00',
  "AAAAXWUE" varchar(2) NOT NULL default '00',
  "AAAAXWTV" varchar(5) NOT NULL default ' ',
  "AAAAXWRW" varchar(3) NOT NULL default '000',
  "AAAAXWRX" varchar(3) NOT NULL default '000',
  "AAABGGHA" varchar(38) NOT NULL default ' ',
  "AAAAXWRZ" varchar(3) NOT NULL default ' ',
  "AAAAXWSA" varchar(3) NOT NULL default ' ',
  "AAAAXWSB" varchar(3) NOT NULL default ' ',
  "AAAAXWSC" varchar(3) NOT NULL default ' ',
  "AAAAXWSD" varchar(3) NOT NULL default ' ',
  "AAAAXWSE" varchar(3) NOT NULL default ' ',
  "AAAAXWSF" varchar(3) NOT NULL default ' ',
  "AAAAXWSG" varchar(3) NOT NULL default ' ',
  "AAAAXWSH" varchar(3) NOT NULL default ' ',
  "AAAAXWSI" varchar(3) NOT NULL default ' ',
  "AAAAXWSJ" varchar(3) NOT NULL default ' ',
  "AAAAXWSK" varchar(3) NOT NULL default ' ',
  "AAAAXWSL" varchar(3) NOT NULL default ' ',
  "AAAAXWSM" varchar(3) NOT NULL default ' ',
  "AAAAXWSN" varchar(3) NOT NULL default ' ',
  "AAAAXWSO" varchar(3) NOT NULL default ' ',
  "AAAAXWSP" varchar(3) NOT NULL default ' ',
  "AAAAXWSQ" varchar(3) NOT NULL default ' ',
  "AAAAXWSR" varchar(3) NOT NULL default ' ',
  "AAAAXWSS" varchar(3) NOT NULL default ' ',
  "AAAAXWST" varchar(3) NOT NULL default ' ',
  "AAAAXWSU" varchar(3) NOT NULL default ' ',
  "AAAAXWSV" varchar(3) NOT NULL default ' ',
  "AAAAXWSW" varchar(3) NOT NULL default ' ',
  "AAAAXWSX" varchar(3) NOT NULL default ' ',
  "AAAAXWSY" varchar(3) NOT NULL default ' ',
  "AAAAXWSZ" varchar(3) NOT NULL default ' ',
  "AAAAXWTA" varchar(3) NOT NULL default ' ',
  "AAAAXWTB" varchar(3) NOT NULL default ' ',
  "AAAAXWTC" varchar(3) NOT NULL default ' ',
  "AAAAXWTD" varchar(3) NOT NULL default ' ',
  "AAAAXWTE" varchar(3) NOT NULL default ' ',
  "AAAAXWTF" varchar(3) NOT NULL default ' ',
  "AAAAXWTG" varchar(3) NOT NULL default ' ',
  "AAAAXWTH" varchar(3) NOT NULL default ' ',
  "AAAAXWTI" varchar(3) NOT NULL default ' ',
  "AAAAXWTJ" varchar(3) NOT NULL default ' ',
  "AAAAXWTK" varchar(3) NOT NULL default ' ',
  "AAAAXWTL" varchar(3) NOT NULL default ' ',
  "AAAAXWTM" varchar(3) NOT NULL default ' ',
  "AAAAXWTN" varchar(3) NOT NULL default ' ',
  "AAAAXWTO" varchar(3) NOT NULL default ' ',
  "AAAAXWTP" varchar(3) NOT NULL default ' ',
  "AAAAXWTQ" varchar(3) NOT NULL default ' ',
  "AAAAXWTR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWYF","AAAAXWUE","AAAAXWTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWYG" varchar(2) NOT NULL default '00',
  "AAAAXWUJ" varchar(3) NOT NULL default '000',
  "AAAAXWUA" varchar(2) NOT NULL default '00',
  "AAABLBXJ" varchar(2) NOT NULL default '00',
  "AAABLYVY" varchar(3) NOT NULL default '000',
  "AAAAXWUD" varchar(1) NOT NULL default ' ',
  "AAAAXWUE" varchar(2) NOT NULL default '00',
  "AAABHPIX" varchar(45) NOT NULL default ' ',
  "AAABLRSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWYG","AAAAXWUJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWYH" varchar(2) NOT NULL default '00',
  "AAABLHXQ" varchar(2) NOT NULL default '00',
  "AAABMRCU" varchar(3) NOT NULL default '000',
  "AAAAXWUP" varchar(1) NOT NULL default ' ',
  "AAAAXWUQ" varchar(1) NOT NULL default ' ',
  "AAAAXWUR" varchar(1) NOT NULL default ' ',
  "AAAAXWUS" varchar(1) NOT NULL default ' ',
  "AAAAXWUT" varchar(4) NOT NULL default ' ',
  "AAAAXWUU" varchar(2) NOT NULL default '00',
  "AAAAXWUV" varchar(1) NOT NULL default ' ',
  "AAAAXWUW" varchar(4) NOT NULL default ' ',
  "AAAAXWUX" varchar(2) NOT NULL default '00',
  "AAAAXWUY" varchar(1) NOT NULL default ' ',
  "AAAAXWUZ" varchar(1) NOT NULL default ' ',
  "AAAAXWVA" varchar(1) NOT NULL default ' ',
  "AAAAXWVB" varchar(4) NOT NULL default ' ',
  "AAAAXWVC" varchar(2) NOT NULL default '00',
  "AAABLRBI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWYH","AAABLHXQ","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWVI" (
  "AAABLHXQ" varchar(2) NOT NULL default '00',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLHXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIBH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXWVR" varchar(1) NOT NULL default ' ',
  "AAAAXWVS" decimal(5,2) NOT NULL default '0.00',
  "AAAAXWVT" decimal(7,2) NOT NULL default '0.00',
  "AAAAXWVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIBH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIBH" varchar(2) NOT NULL default ' ',
  "AAAAXWWC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCHH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXWWM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAAAYCHH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWZL" varchar(2) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJRHR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAAAXWZL","AAABMREX","AAABMCVI","AAABBHJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWXO" varchar(2) NOT NULL default ' ',
  "AAAAXWXP" varchar(2) NOT NULL default '00',
  "AAABMCMG" varchar(2) NOT NULL default '00',
  "AAABLXIS" varchar(15) NOT NULL default ' ',
  "AAAAXWXM" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWXO","AAAAXWXP","AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWZL" varchar(2) NOT NULL default ' ',
  "AAAAXWXZ" varchar(2) NOT NULL default ' ',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAXWZL","AAAAXWXZ","AAABMRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWYB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWZL" varchar(2) NOT NULL default ' ',
  "AAABLXIS" varchar(25) NOT NULL default ' ',
  "AAAAXWYF" varchar(2) NOT NULL default '00',
  "AAAAXWYG" varchar(2) NOT NULL default '00',
  "AAAAXWYH" varchar(2) NOT NULL default '00',
  "AAAAXWYI" varchar(2) NOT NULL default '00',
  "AAABJBLF" varchar(4) NOT NULL default '0000',
  "AAAAXWYK" varchar(1) NOT NULL default ' ',
  "AAAAXWYL" varchar(1) NOT NULL default ' ',
  "AAAAXWYM" varchar(1) NOT NULL default ' ',
  "AAAAXWYN" varchar(1) NOT NULL default ' ',
  "AAAAXWYO" varchar(1) NOT NULL default ' ',
  "AAAAXWYP" varchar(1) NOT NULL default ' ',
  "AAAAXWYQ" varchar(1) NOT NULL default ' ',
  "AAAAXWYR" varchar(1) NOT NULL default ' ',
  "AAAAXWYS" varchar(1) NOT NULL default ' ',
  "AAAAXWYT" varchar(1) NOT NULL default ' ',
  "AAAAXWYU" varchar(1) NOT NULL default ' ',
  "AAAAXWYV" varchar(8) NOT NULL default ' ',
  "AAABKODI" varchar(4) NOT NULL default ' ',
  "AAAAXWYX" varchar(4) NOT NULL default ' ',
  "AAAAXWYY" varchar(6) NOT NULL default ' ',
  "AAABMRCU" varchar(4) NOT NULL default ' ',
  "AAAAXWZA" varchar(8) NOT NULL default ' ',
  "AAAAXWZB" varchar(1) NOT NULL default ' ',
  "AAAAXWZC" varchar(2) NOT NULL default ' ',
  "AAAAXWZD" varchar(2) NOT NULL default '00',
  "AAAAXWZE" varchar(9) NOT NULL default ' ',
  "AAAAXWZF" varchar(9) NOT NULL default ' ',
  "AAAAXWZG" varchar(8) NOT NULL default ' ',
  "AAAAXWZH" varchar(1) NOT NULL default ' ',
  "AAAAXWZI" varchar(3) NOT NULL default ' ',
  "AAAAYCFU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWZM" (
  "AAAAXWZX" varchar(1) NOT NULL default ' ',
  "AAAAXWZO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXWZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXWZX" varchar(1) NOT NULL default ' ',
  "AAAAXWZY" varchar(2) NOT NULL default ' ',
  "AAAAZNSB" varchar(30) NOT NULL default ' ',
  "AAAAXWZV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXWZX","AAAAXWZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXWZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXAN" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(50) NOT NULL default ' ',
  "AAAAXXAD" varchar(1) NOT NULL default ' ',
  "AAAAXXAE" varchar(1) NOT NULL default ' ',
  "AAABLWIT" varchar(1) NOT NULL default ' ',
  "AAABLWIV" varchar(1) NOT NULL default ' ',
  "AAABLWIX" varchar(1) NOT NULL default ' ',
  "AAABLWIZ" varchar(1) NOT NULL default ' ',
  "AAABLWJB" varchar(1) NOT NULL default ' ',
  "AAABLWJD" varchar(1) NOT NULL default ' ',
  "AAABMLCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXXAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDKMH" varchar(1) NOT NULL default ' ',
  "AAABBMVH" varchar(2) NOT NULL default ' ',
  "AAABHANX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDKMH","AAABBMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAAAXXBR" varchar(2) NOT NULL default ' ',
  "AAAAXXBS" varchar(1) NOT NULL default ' ',
  "AAAAXXBT" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAXXBV" varchar(4) NOT NULL default '0000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXBI" varchar(4) NOT NULL default '0000',
  "AAAAXXBJ" decimal(3,0) NOT NULL default '0',
  "AAAAXXBK" varchar(3) NOT NULL default ' ',
  "AAAAXXBL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAAAXYJK","AAAAXXBR","AAAAXXBS","AAAAXXBT","AAABKFCZ","AAAAXXBV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAAAXXYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBEON","AAABBEOO","AAAAXYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXCK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFSD" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFSD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFSD" varchar(6) NOT NULL default ' ',
  "AAAAXXDX" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXFJ" varchar(2) NOT NULL default '00',
  "AAAAXXDD" decimal(9,2) NOT NULL default '0.00',
  "AAAAXXDE" decimal(9,2) NOT NULL default '0.00',
  "AAAAXXDF" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABAFSD","AAAAXXDX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFSD" varchar(6) NOT NULL default ' ',
  "AAAAXXDX" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXEG" varchar(1) NOT NULL default ' ',
  "AAAAXXDR" decimal(5,2) NOT NULL default '0.00',
  "AAAAXXDS" varchar(1) NOT NULL default '0',
  "AAAAXXDT" decimal(4,2) NOT NULL default '0.00',
  "AAAAXXDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFSD","AAAAXXDX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXEG" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXEE" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXXEG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXFJ" varchar(2) NOT NULL default '00',
  "AAAAXXFQ" varchar(2) NOT NULL default '00',
  "AAAAXXEZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXXFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXEZ" varchar(2) NOT NULL default '00',
  "AAABKVPX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXEV" decimal(9,2) NOT NULL default '0.00',
  "AAAAXXEW" decimal(9,2) NOT NULL default '0.00',
  "AAAAXXEX" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXXEZ","AAABKVPX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXXFJ" varchar(2) NOT NULL default '00',
  "AAABJFNY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXXFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXFQ" varchar(2) NOT NULL default '00',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAXXFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXXFQ","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYEY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYEY" varchar(2) NOT NULL default ' ',
  "AAAAXXGB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYEY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYEY" varchar(2) NOT NULL default ' ',
  "AAABGLEK" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAXYEY","AAABGLEK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYEY" varchar(2) NOT NULL default ' ',
  "AAABGLEK" varchar(2) NOT NULL default ' ',
  "AAAAXXGU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYEY","AAABGLEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYEY" varchar(2) NOT NULL default ' ',
  "AAABGLEK" varchar(2) NOT NULL default ' ',
  "AAABGLEL" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAXYEY","AAABGLEK","AAABGLEL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYEY" varchar(2) NOT NULL default ' ',
  "AAABGLEK" varchar(2) NOT NULL default ' ',
  "AAABGLEL" varchar(2) NOT NULL default ' ',
  "AAAAXXHR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYEY","AAABGLEK","AAABGLEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXHX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXWX" varchar(14) NOT NULL default ' ',
  "AAAAXXIG" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXVA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXXWX","AAAAXXIG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXIX" varchar(2) NOT NULL default ' ',
  "AAAAXXIY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXXIX","AAAAXXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXXIX" varchar(2) NOT NULL default ' ',
  "AAAAXXIY" varchar(2) NOT NULL default ' ',
  "AAAAXXIU" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXXIX","AAAAXXIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXJI" varchar(2) NOT NULL default ' ',
  "AAAAXXJJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABHVXC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXKA" varchar(2) NOT NULL default ' ',
  "AAAAXXKB" varchar(2) NOT NULL default '00',
  "AAAAXXKC" varchar(2) NOT NULL default '00',
  "AAAAXXKD" varchar(2) NOT NULL default ' ',
  "AAAAXXKE" varchar(2) NOT NULL default ' ',
  "AAABJLFH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAXXLE" varchar(8) NOT NULL default ' ',
  "AAAAXXLF" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXKX" varchar(2) NOT NULL default ' ',
  "AAAAXXKY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAAAXXLE","AAAAXXLF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAAAXXLO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAAAXXMN" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXMF" varchar(2) NOT NULL default '00',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAAAXXMN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAXYEY" varchar(2) NOT NULL default ' ',
  "AAABLQTM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAXXVA" varchar(2) NOT NULL default ' ',
  "AAAAXXND" varchar(2) NOT NULL default '00',
  "AAAAXXOM" varchar(2) NOT NULL default '00',
  "AAAAXXNF" varchar(1) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAAAXXOO" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAAAXXVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAAAXXYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAAAXYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAXXVA" varchar(2) NOT NULL default ' ',
  "AAAAXXOM" varchar(2) NOT NULL default '00',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAAAXXOO" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAAAXXVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAAAXXYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAAAXYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAABAWEA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXQF" varchar(2) NOT NULL default ' ',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXXQF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAAAXYEJ" varchar(4) NOT NULL default ' ',
  "AAAAXXQT" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXQN" decimal(8,4) NOT NULL default '0.0000',
  "AAAAXXQO" decimal(6,2) NOT NULL default '0.00',
  "AAAAXXQP" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYEM","AAAAXYEJ","AAAAXXQT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAAAXYEJ" varchar(4) NOT NULL default ' ',
  "AAABAWEA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYEM","AAAAXYEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXRN" varchar(8) NOT NULL default ' ',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYEJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXXRN","AAAAXYEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYEY" varchar(2) NOT NULL default ' ',
  "AAABGLEK" varchar(2) NOT NULL default ' ',
  "AAABGLEL" varchar(2) NOT NULL default ' ',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYEJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYEY","AAABGLEK","AAABGLEL","AAAAXYEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHLV" varchar(8) NOT NULL default '00000000',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYEJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHLV","AAAAXYEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXXTG" varchar(2) NOT NULL default ' ',
  "AAAAXXTH" varchar(2) NOT NULL default ' ',
  "AAAAXXSU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXXTG","AAAAXXTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXXTG" varchar(2) NOT NULL default ' ',
  "AAAAXXTH" varchar(2) NOT NULL default ' ',
  "AAABILTS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXXTG","AAAAXXTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXXVA" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXXVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAAAXXYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXUD" (
  "AAAAXYIO" varchar(3) NOT NULL default '000',
  "AAAAXYIM" varchar(47) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXYIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXUH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXXVA" varchar(2) NOT NULL default ' ',
  "AAAAXXUL" varchar(1) NOT NULL default ' ',
  "AAAAXXUM" varchar(1) NOT NULL default ' ',
  "AAAAXXUN" varchar(1) NOT NULL default ' ',
  "AAAAXXUO" varchar(1) NOT NULL default ' ',
  "AAAAXXUP" varchar(4) NOT NULL default ' ',
  "AAAAXXUQ" varchar(4) NOT NULL default ' ',
  "AAAAXXUR" varchar(4) NOT NULL default ' ',
  "AAAAXXUS" varchar(4) NOT NULL default ' ',
  "AAAAXXUT" varchar(4) NOT NULL default ' ',
  "AAAAXXUU" varchar(5) NOT NULL default ' ',
  "AAAAXXUV" varchar(5) NOT NULL default ' ',
  "AAAAXXUW" varchar(4) NOT NULL default ' ',
  "AAAAXXUX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXXVA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAAAXXYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABFPGJ" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABFPGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABFPGJ" varchar(2) NOT NULL default ' ',
  "AAABLWJT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABFPGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXXWX" varchar(14) NOT NULL default ' ',
  "AAABFPGJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXXWX","AAABFPGJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXXZP" varchar(2) NOT NULL default ' ',
  "AAAAXXZQ" varchar(2) NOT NULL default ' ',
  "AAAAXXXJ" varchar(2) NOT NULL default ' ',
  "AAAAXYKS" varchar(2) NOT NULL default ' ',
  "AAAAXXXL" varchar(1) NOT NULL default ' ',
  "AAAAXXXM" decimal(3,0) NOT NULL default '0',
  "AAAAXXXN" varchar(3) NOT NULL default ' ',
  "AAAAXXXO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAAAXYJK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXXU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAAAXXYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBEON","AAAAXYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXYG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXXZP" varchar(2) NOT NULL default ' ',
  "AAAAXXZQ" varchar(2) NOT NULL default ' ',
  "AAAAXXYL" varchar(4) NOT NULL default ' ',
  "AAAAXXYM" varchar(4) NOT NULL default ' ',
  "AAAAXXYN" varchar(4) NOT NULL default ' ',
  "AAAAXXYO" varchar(4) NOT NULL default ' ',
  "AAAAXXYP" varchar(4) NOT NULL default ' ',
  "AAAAXXYQ" varchar(4) NOT NULL default ' ',
  "AAAAXXYR" varchar(4) NOT NULL default ' ',
  "AAAAXXYS" varchar(4) NOT NULL default ' ',
  "AAAAXXYT" varchar(4) NOT NULL default ' ',
  "AAAAXXYU" varchar(4) NOT NULL default ' ',
  "AAAAXXYV" varchar(2) NOT NULL default '00',
  "AAAAXXYW" varchar(2) NOT NULL default '00',
  "AAAAXXYX" varchar(4) NOT NULL default ' ',
  "AAAAXXYY" varchar(4) NOT NULL default ' ',
  "AAAAXXYZ" varchar(4) NOT NULL default ' ',
  "AAAAXXZA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXXZP","AAAAXXZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXXZP" varchar(2) NOT NULL default ' ',
  "AAAAXXZQ" varchar(2) NOT NULL default ' ',
  "AAAAXXZL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXXZP","AAAAXXZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXXZY" varchar(4) NOT NULL default ' ',
  "AAABHWSF" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXXZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXXZZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYAG" varchar(2) NOT NULL default ' ',
  "AAAAXYAD" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYAO" varchar(2) NOT NULL default ' ',
  "AAAAXYAL" varchar(27) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYAW" varchar(2) NOT NULL default ' ',
  "AAAAYWYN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYAX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYBJ" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYBE" varchar(20) NOT NULL default ' ',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYBJ","AAABBEOO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYBY" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZMOD" varchar(20) NOT NULL default ' ',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYBY","AAABBEOO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYCI" varchar(4) NOT NULL default ' ',
  "AAAAXYCF" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYCI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYCT" varchar(4) NOT NULL default ' ',
  "AAAAXYCN" varchar(7) NOT NULL default ' ',
  "AAAAXYCO" varchar(25) NOT NULL default ' ',
  "AAAAXYCP" varchar(1) NOT NULL default ' ',
  "AAAAXYCQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXYCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABGLBT" varchar(8) NOT NULL default '00000000',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYEJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABGLBT","AAAAXYEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYDP" varchar(14) NOT NULL default ' ',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYEJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYDP","AAAAXYEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYDS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGEH" varchar(8) NOT NULL default '00000000',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYEJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGEH","AAAAXYEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYEL" varchar(8) NOT NULL default ' ',
  "AAAAXYEM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYEJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYEL","AAAAXYEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABGLBT" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAAAXYEY" varchar(2) NOT NULL default ' ',
  "AAABGLEK" varchar(2) NOT NULL default ' ',
  "AAABGLEL" varchar(2) NOT NULL default ' ',
  "AAAAXYFB" varchar(4) NOT NULL default '0000',
  "AAAAXYFC" varchar(2) NOT NULL default ' ',
  "AAAAXYFD" varchar(3) NOT NULL default ' ',
  "AAAAXYFE" varchar(7) NOT NULL default ' ',
  "AAAAXYFF" varchar(3) NOT NULL default ' ',
  "AAAAXYFG" varchar(2) NOT NULL default ' ',
  "AAAAXYFH" decimal(6,2) NOT NULL default '0.00',
  "AAAAXYFI" decimal(6,2) NOT NULL default '0.00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYFK" varchar(1) NOT NULL default ' ',
  "AAAAXYFL" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLHPM","AAABGLBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYGD" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYFW" varchar(40) NOT NULL default ' ',
  "AAAAXYFX" varchar(25) NOT NULL default ' ',
  "AAAAXYFY" varchar(6) NOT NULL default '000000',
  "AAAAXYFZ" varchar(5) NOT NULL default '00000',
  "AAAAXYGA" varchar(30) NOT NULL default ' ',
  "AAAAXYGB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYGD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYGO" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYGL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYGO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFAO" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHFAO" varchar(6) NOT NULL default ' ',
  "AAAAXYGZ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHFAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAABHFAO" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYHK" varchar(2) NOT NULL default ' ',
  "AAAAXYHL" varchar(2) NOT NULL default ' ',
  "AAAAZNNR" varchar(1) NOT NULL default ' ',
  "AAAAXYHN" varchar(1) NOT NULL default ' ',
  "AAAAXYHO" varchar(1) NOT NULL default ' ',
  "AAAAXYHP" varchar(1) NOT NULL default ' ',
  "AAAAXYHQ" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(1) NOT NULL default ' ',
  "AAABEJEG" varchar(1) NOT NULL default ' ',
  "AAABFQYW" varchar(1) NOT NULL default ' ',
  "AAAAXYHU" varchar(1) NOT NULL default ' ',
  "AAAAXYHV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXYJK","AAABHFAO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYIG" varchar(2) NOT NULL default ' ',
  "AAAAXYIE" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYIO" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYIM" varchar(47) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYIO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABFPGJ" varchar(2) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABFPGJ","AAAAXYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABFPGJ" varchar(2) NOT NULL default ' ',
  "AAAAXYJK" varchar(2) NOT NULL default ' ',
  "AAABBFCQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABFPGJ","AAAAXYJK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYKF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYKF" varchar(2) NOT NULL default ' ',
  "AAABHMSB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYKS" varchar(2) NOT NULL default ' ',
  "AAAAXYKF" varchar(2) NOT NULL default ' ',
  "AAAAXYKC" decimal(2,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAXYKS","AAAAXYKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYKS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXYKS" varchar(2) NOT NULL default ' ',
  "AAAAXYKP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXYKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAAAXYNA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYTE","AAAAXYNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAAAXYNA" varchar(3) NOT NULL default ' ',
  "AAAAXYLF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLYTE","AAAAXYNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAAAXYNA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYNR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLYTE","AAAAXYNA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAXYMM" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAAAXYNR" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAAAXYMM","AAABGLBU","AAAAXYNR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAAAXYNA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYTE","AAAAXYNA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAXYNR" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAAAXYNR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYPP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXYPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYPP" varchar(4) NOT NULL default ' ',
  "AAAAXYOF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXYPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYPP" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYOQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYOR" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYOS" varchar(40) NOT NULL default ' ',
  "AAAAXYOT" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYOU" varchar(4) NOT NULL default ' ',
  "AAAAXYOV" varchar(1) NOT NULL default ' ',
  "AAAAXYOW" varchar(1) NOT NULL default ' ',
  "AAAAXYOX" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXYPP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYPP" varchar(4) NOT NULL default ' ',
  "AAAAXYRE" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYPK" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYPL" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYPM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXYPP","AAAAXYRE","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYQJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXYQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYQJ" varchar(2) NOT NULL default ' ',
  "AAAAXYQF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXYQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYRE" varchar(2) NOT NULL default ' ',
  "AAAAXYQO" varchar(1) NOT NULL default ' ',
  "AAAAXYQP" varchar(1) NOT NULL default ' ',
  "AAAAXYQQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYQR" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXYRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXYRE" varchar(2) NOT NULL default ' ',
  "AAAAXYRA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXYRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYRK" varchar(6) NOT NULL default '000000',
  "AAAAXZEK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYRK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYRL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZZE" varchar(2) NOT NULL default ' ',
  "AAAAXZPU" varchar(2) NOT NULL default ' ',
  "AAAAYBEA" varchar(1) NOT NULL default ' ',
  "AAAAXZKY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXYRU" decimal(6,2) NOT NULL default '0.00',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAXYRW" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXZZE","AAAAXZPU","AAAAYBEA","AAAAXZKY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYSE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYYS" varchar(6) NOT NULL default '000000',
  "AAAAXYSP" varchar(8) NOT NULL default '00000000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAXYYS","AAAAXYSP","AAABMCVI","AAABBHJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXYUW" varchar(1) NOT NULL default ' ',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAXYXK" varchar(8) NOT NULL default '00000000',
  "AAAAXYXL" varchar(8) NOT NULL default '00000000',
  "AAAAXYXM" varchar(25) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAXYXO" varchar(8) NOT NULL default '00000000',
  "AAAAXYXP" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYXQ" varchar(8) NOT NULL default '00000000',
  "AAAAXYXR" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYXS" varchar(8) NOT NULL default '00000000',
  "AAAAXYXT" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYXU" varchar(3) NOT NULL default ' ',
  "AAAAXYXV" varchar(4) NOT NULL default ' ',
  "AAAAXYXW" varchar(2) NOT NULL default ' ',
  "AAAAXYXX" varchar(2) NOT NULL default ' ',
  "AAAAXYXY" varchar(2) NOT NULL default ' ',
  "AAAAXYXZ" decimal(5,2) NOT NULL default '0.00',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABLVHJ" varchar(25) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAABGCHP" varchar(2) NOT NULL default ' ',
  "AAAAXYYH" varchar(8) NOT NULL default ' ',
  "AAAAXYYI" varchar(2) NOT NULL default ' ',
  "AAABKOPL" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYYK" varchar(2) NOT NULL default ' ',
  "AAAAXYYL" varchar(2) NOT NULL default ' ',
  "AAAAXYYM" varchar(2) NOT NULL default ' ',
  "AAAAXYYN" varchar(2) NOT NULL default ' ',
  "AAAAXYYO" varchar(2) NOT NULL default ' ',
  "AAAAXYYP" varchar(2) NOT NULL default ' ',
  "AAAAXYYQ" varchar(2) NOT NULL default ' ',
  "AAAAXYYR" varchar(2) NOT NULL default ' ',
  "AAAAXYYS" varchar(6) NOT NULL default '000000',
  "AAAAXYYT" varchar(10) NOT NULL default ' ',
  "AAAAXYYU" varchar(10) NOT NULL default ' ',
  "AAAAXZZU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXYUW","AAABMLTD","AAABMCRM","AAABMCRJ","AAABMCRK","AAABMCXJ","AAABLKML","AAABKROQ","AAABKROR","AAABMCXI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAXYXK" varchar(8) NOT NULL default '00000000',
  "AAAAXYXL" varchar(8) NOT NULL default '00000000',
  "AAAAXYXM" varchar(25) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAXYXO" varchar(8) NOT NULL default '00000000',
  "AAAAXYXP" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYXQ" varchar(8) NOT NULL default '00000000',
  "AAAAXYXR" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYXS" varchar(8) NOT NULL default '00000000',
  "AAAAXYXT" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYXU" varchar(3) NOT NULL default ' ',
  "AAAAXYXV" varchar(4) NOT NULL default ' ',
  "AAAAXYXW" varchar(2) NOT NULL default ' ',
  "AAAAXYXX" varchar(2) NOT NULL default ' ',
  "AAAAXYXY" varchar(2) NOT NULL default ' ',
  "AAAAXYXZ" decimal(5,2) NOT NULL default '0.00',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABLVHJ" varchar(25) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAABGCHP" varchar(2) NOT NULL default ' ',
  "AAAAXYYH" varchar(8) NOT NULL default ' ',
  "AAAAXYYI" varchar(2) NOT NULL default ' ',
  "AAABKOPL" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYYK" varchar(2) NOT NULL default ' ',
  "AAAAXYYL" varchar(2) NOT NULL default ' ',
  "AAAAXYYM" varchar(2) NOT NULL default ' ',
  "AAAAXYYN" varchar(2) NOT NULL default ' ',
  "AAAAXYYO" varchar(2) NOT NULL default ' ',
  "AAAAXYYP" varchar(2) NOT NULL default ' ',
  "AAAAXYYQ" varchar(2) NOT NULL default ' ',
  "AAAAXYYR" varchar(2) NOT NULL default ' ',
  "AAAAXYYS" varchar(6) NOT NULL default '000000',
  "AAAAXYYT" varchar(10) NOT NULL default ' ',
  "AAAAXYYU" varchar(10) NOT NULL default ' ',
  "AAAAXZZU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLTD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAAAXYXK" varchar(8) NOT NULL default '00000000',
  "AAAAXYXL" varchar(8) NOT NULL default '00000000',
  "AAAAXYXM" varchar(25) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAXYXO" varchar(8) NOT NULL default '00000000',
  "AAAAXYXP" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYXQ" varchar(8) NOT NULL default '00000000',
  "AAAAXYXR" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYXS" varchar(8) NOT NULL default '00000000',
  "AAAAXYXT" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYXU" varchar(3) NOT NULL default ' ',
  "AAAAXYXV" varchar(4) NOT NULL default ' ',
  "AAAAXYXW" varchar(2) NOT NULL default ' ',
  "AAAAXYXX" varchar(2) NOT NULL default ' ',
  "AAAAXYXY" varchar(2) NOT NULL default ' ',
  "AAAAXYXZ" decimal(5,2) NOT NULL default '0.00',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABLVHJ" varchar(25) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAABGCHP" varchar(2) NOT NULL default ' ',
  "AAAAXYYH" varchar(8) NOT NULL default ' ',
  "AAAAXYYI" varchar(2) NOT NULL default ' ',
  "AAABKOPL" decimal(5,2) NOT NULL default '0.00',
  "AAAAXYYK" varchar(2) NOT NULL default ' ',
  "AAAAXYYL" varchar(2) NOT NULL default ' ',
  "AAAAXYYM" varchar(2) NOT NULL default ' ',
  "AAAAXYYN" varchar(2) NOT NULL default ' ',
  "AAAAXYYO" varchar(2) NOT NULL default ' ',
  "AAAAXYYP" varchar(2) NOT NULL default ' ',
  "AAAAXYYQ" varchar(2) NOT NULL default ' ',
  "AAAAXYYR" varchar(2) NOT NULL default ' ',
  "AAAAXYYS" varchar(6) NOT NULL default '000000',
  "AAAAXYYT" varchar(10) NOT NULL default ' ',
  "AAAAXYYU" varchar(10) NOT NULL default ' ',
  "AAAAXZZU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYYZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXZBG" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABBAED" varchar(40) NOT NULL default ' ',
  "AAAAXYZJ" varchar(25) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAXYZL" varchar(1) NOT NULL default ' ',
  "AAAAZJQW" varchar(72) NOT NULL default ' ',
  "AAAAZJQY" varchar(72) NOT NULL default ' ',
  "AAAAZJRA" varchar(72) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAAAXZBG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXYZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXZBG" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMAKK" varchar(12) NOT NULL default ' ',
  "AAABGCHO" varchar(1) NOT NULL default ' ',
  "AAABGCHP" varchar(2) NOT NULL default ' ',
  "AAABLXUQ" varchar(8) NOT NULL default ' ',
  "AAAAXZAG" varchar(2) NOT NULL default ' ',
  "AAAAXZAH" varchar(2) NOT NULL default ' ',
  "AAAAXZAI" varchar(5) NOT NULL default ' ',
  "AAABKOPL" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZAK" varchar(2) NOT NULL default ' ',
  "AAAAXZAL" varchar(2) NOT NULL default ' ',
  "AAAAXZAM" varchar(2) NOT NULL default ' ',
  "AAAAXZAN" varchar(2) NOT NULL default ' ',
  "AAAAXZAO" varchar(2) NOT NULL default ' ',
  "AAAAXZAP" varchar(2) NOT NULL default ' ',
  "AAAAXZAQ" varchar(2) NOT NULL default ' ',
  "AAAAXZAR" varchar(2) NOT NULL default ' ',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAAAXZNW" varchar(2) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAAAXZBG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABDXWF" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAAAXZZE" varchar(2) NOT NULL default ' ',
  "AAAAXZBP" varchar(8) NOT NULL default ' ',
  "AAAAXZZF" varchar(8) NOT NULL default ' ',
  "AAAAXZZG" varchar(8) NOT NULL default '00000000',
  "AAABBWGQ" varchar(2) NOT NULL default '00',
  "AAABGAMR" varchar(1) NOT NULL default ' ',
  "AAABJSBE" varchar(1) NOT NULL default ' ',
  "AAABMCEZ" varchar(1) NOT NULL default ' ',
  "AAABKWQO" varchar(4) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZBY" varchar(13) NOT NULL default ' ',
  "AAAAYBEA" varchar(1) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAXZCC" varchar(4) NOT NULL default ' ',
  "AAAAXZCD" varchar(3) NOT NULL default ' ',
  "AAABILPS" varchar(20) NOT NULL default ' ',
  "AAABCSTT" varchar(240) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABDXWF","AAABKFCZ","AAABLUVE"),
  KEY "AAAAXZCL" ("AAAAXZZE","AAAAXZZF","AAAAXZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXZZE" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZCU" varchar(30) NOT NULL default ' ',
  "AAAAXZCV" varchar(30) NOT NULL default ' ',
  "AAAAXZCW" varchar(30) NOT NULL default ' ',
  "AAAAXZCX" varchar(30) NOT NULL default ' ',
  "AAAAXZCY" varchar(10) NOT NULL default ' ',
  "AAAAXZCZ" varchar(25) NOT NULL default ' ',
  "AAAAXZDA" varchar(3) NOT NULL default ' ',
  "AAAAXZDB" varchar(8) NOT NULL default '00000000',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAAAYBBL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXZZE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZZE" varchar(2) NOT NULL default ' ',
  "AAAAXZDZ" varchar(2) NOT NULL default ' ',
  "AAAAYBEA" varchar(1) NOT NULL default ' ',
  "AAAAXZEB" varchar(1) NOT NULL default ' ',
  "AAAAXZEC" varchar(2) NOT NULL default ' ',
  "AAABKNSR" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZDS" varchar(2) NOT NULL default ' ',
  "AAAAXZDT" decimal(5,2) NOT NULL default '0.00',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAXZDV" decimal(6,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXZZE","AAAAXZDZ","AAAAYBEA","AAAAXZEB","AAAAXZEC","AAABKNSR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXZEM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZEK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXZEM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZEO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHE" varchar(1) NOT NULL default ' ',
  "AAAAXZOJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZEV" varchar(4) NOT NULL default ' ',
  "AAAAYCZB" varchar(4) NOT NULL default ' ',
  "AAAAXZEX" decimal(9,2) NOT NULL default '0.00',
  "AAAAXZEY" decimal(9,2) NOT NULL default '0.00',
  "AAAAXZEZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZFA" decimal(3,0) NOT NULL default '0',
  "AAAAXZFB" varchar(1) NOT NULL default ' ',
  "AAAAXZFC" varchar(1) NOT NULL default ' ',
  "AAAAXZFD" decimal(5,0) NOT NULL default '0',
  "AAAAXZTG" varchar(1) NOT NULL default ' ',
  "AAAAXZFF" varchar(2) NOT NULL default ' ',
  "AAAAXZFG" varchar(2) NOT NULL default ' ',
  "AAAAXZFH" varchar(1) NOT NULL default ' ',
  "AAAAXZFI" varchar(1) NOT NULL default ' ',
  "AAAAXZFJ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHE","AAAAXZOJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYBEA" varchar(1) NOT NULL default ' ',
  "AAAAYBDQ" varchar(2) NOT NULL default ' ',
  "AAAAYBDR" varchar(8) NOT NULL default ' ',
  "AAAAYBDS" varchar(2) NOT NULL default ' ',
  "AAAAYBDT" varchar(8) NOT NULL default ' ',
  "AAAAYBDU" varchar(2) NOT NULL default ' ',
  "AAAAYBDV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAAAYBEA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDCP" varchar(2) NOT NULL default '00',
  "AAAAXZJV" varchar(2) NOT NULL default ' ',
  "AAAAYBEA" varchar(1) NOT NULL default ' ',
  "AAAAXZJX" varchar(1) NOT NULL default ' ',
  "AAAAXZJY" varchar(2) NOT NULL default '00',
  "AAAAXZJZ" varchar(2) NOT NULL default '00',
  "AAAAXZKA" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZGQ" varchar(2) NOT NULL default ' ',
  "AAAAXZGR" varchar(1) NOT NULL default ' ',
  "AAAAXZGS" varchar(1) NOT NULL default ' ',
  "AAAAXZGT" varchar(2) NOT NULL default ' ',
  "AAAAXZGU" varchar(1) NOT NULL default ' ',
  "AAAAXZGV" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZGW" decimal(6,4) NOT NULL default '0.0000',
  "AAAAXZGX" varchar(2) NOT NULL default ' ',
  "AAAAXZGY" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZGZ" decimal(6,2) NOT NULL default '0.00',
  "AAAAXZHA" varchar(4) NOT NULL default ' ',
  "AAAAXZHB" varchar(2) NOT NULL default ' ',
  "AAAAXZHC" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZHD" varchar(4) NOT NULL default ' ',
  "AAAAXZHE" varchar(2) NOT NULL default ' ',
  "AAAAXZHF" varchar(1) NOT NULL default ' ',
  "AAAAXZHG" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZHH" decimal(6,4) NOT NULL default '0.0000',
  "AAAAXZHI" varchar(2) NOT NULL default ' ',
  "AAAAXZHJ" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZHK" decimal(6,2) NOT NULL default '0.00',
  "AAAAXZHL" varchar(4) NOT NULL default ' ',
  "AAAAXZHM" varchar(2) NOT NULL default ' ',
  "AAAAXZHN" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZHO" varchar(4) NOT NULL default ' ',
  "AAAAXZHP" varchar(2) NOT NULL default ' ',
  "AAAAXZHQ" varchar(1) NOT NULL default ' ',
  "AAAAXZHR" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZHS" decimal(6,4) NOT NULL default '0.0000',
  "AAAAXZHT" varchar(2) NOT NULL default ' ',
  "AAAAXZHU" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZHV" decimal(6,2) NOT NULL default '0.00',
  "AAAAXZHW" varchar(4) NOT NULL default ' ',
  "AAAAXZHX" varchar(2) NOT NULL default ' ',
  "AAAAXZHY" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZHZ" varchar(4) NOT NULL default ' ',
  "AAAAXZIA" varchar(2) NOT NULL default ' ',
  "AAAAXZIB" varchar(1) NOT NULL default ' ',
  "AAAAXZIC" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZID" decimal(6,4) NOT NULL default '0.0000',
  "AAAAXZIE" varchar(2) NOT NULL default ' ',
  "AAAAXZIF" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZIG" decimal(6,2) NOT NULL default '0.00',
  "AAAAXZIH" varchar(4) NOT NULL default ' ',
  "AAAAXZII" varchar(2) NOT NULL default ' ',
  "AAAAXZIJ" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZIK" varchar(4) NOT NULL default ' ',
  "AAAAXZIL" varchar(2) NOT NULL default ' ',
  "AAAAXZIM" varchar(1) NOT NULL default ' ',
  "AAAAXZIN" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZIO" decimal(6,4) NOT NULL default '0.0000',
  "AAAAXZIP" varchar(2) NOT NULL default ' ',
  "AAAAXZIQ" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZIR" decimal(6,2) NOT NULL default '0.00',
  "AAAAXZIS" varchar(4) NOT NULL default ' ',
  "AAAAXZIT" varchar(2) NOT NULL default ' ',
  "AAAAXZIU" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZIV" varchar(4) NOT NULL default ' ',
  "AAAAXZIW" varchar(2) NOT NULL default ' ',
  "AAAAXZIX" varchar(1) NOT NULL default ' ',
  "AAAAXZIY" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZIZ" decimal(6,4) NOT NULL default '0.0000',
  "AAAAXZJA" varchar(2) NOT NULL default ' ',
  "AAAAXZJB" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZJC" decimal(6,2) NOT NULL default '0.00',
  "AAAAXZJD" varchar(4) NOT NULL default ' ',
  "AAAAXZJE" varchar(2) NOT NULL default ' ',
  "AAAAXZJF" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZJG" varchar(4) NOT NULL default ' ',
  "AAAAXZJH" varchar(2) NOT NULL default ' ',
  "AAAAXZJI" varchar(1) NOT NULL default ' ',
  "AAAAXZJJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZJK" decimal(6,4) NOT NULL default '0.0000',
  "AAAAXZJL" varchar(2) NOT NULL default ' ',
  "AAAAXZJM" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZJN" decimal(6,2) NOT NULL default '0.00',
  "AAAAXZJO" varchar(4) NOT NULL default ' ',
  "AAAAXZJP" varchar(2) NOT NULL default ' ',
  "AAAAXZJQ" decimal(4,2) NOT NULL default '0.00',
  "AAAAXZJR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDCP","AAAAXZJV","AAAAYBEA","AAAAXZJX","AAAAXZJY","AAAAXZJZ","AAAAXZKA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHE" varchar(1) NOT NULL default ' ',
  "AAAAXZOJ" varchar(2) NOT NULL default ' ',
  "AAABLPWW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABGCHE","AAAAXZOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXZKX" varchar(1) NOT NULL default ' ',
  "AAAAXZKY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZKU" varchar(20) NOT NULL default ' ',
  "AAAAXZKV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXZKX","AAAAXZKY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZLM" varchar(2) NOT NULL default ' ',
  "AAAAXZSN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXZLM","AAAAXZSN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABBAED" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLXZQ" decimal(9,2) NOT NULL default '0.00',
  "AAAAZSAM" varchar(2) NOT NULL default ' ',
  "AAAAXZME" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAEF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZSN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXZSN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(1) NOT NULL default ' ',
  "AAAAXZTE" varchar(1) NOT NULL default ' ',
  "AAAAXZTF" varchar(1) NOT NULL default ' ',
  "AAAAXZTG" varchar(1) NOT NULL default ' ',
  "AAABCUOP" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLPEC" decimal(9,2) NOT NULL default '0.00',
  "AAAAXZND" decimal(9,2) NOT NULL default '0.00',
  "AAAAXZSX" varchar(4) NOT NULL default ' ',
  "AAAAXZSY" varchar(4) NOT NULL default ' ',
  "AAAAXZSZ" varchar(4) NOT NULL default ' ',
  "AAAAXZTA" varchar(4) NOT NULL default ' ',
  "AAAAXZNI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMJXW","AAAAXZTE","AAAAXZTF","AAAAXZTG","AAABCUOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXZQE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZNW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXZQE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHE" varchar(1) NOT NULL default ' ',
  "AAAAXZOJ" varchar(2) NOT NULL default ' ',
  "AAAAYDVJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHE","AAAAXZOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZPU" varchar(2) NOT NULL default ' ',
  "AAAAYBEA" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZOR" varchar(2) NOT NULL default '00',
  "AAAAXZOS" varchar(2) NOT NULL default '00',
  "AAAAXZOT" varchar(2) NOT NULL default '00',
  "AAABBQON" varchar(2) NOT NULL default ' ',
  "AAAAXZOV" varchar(2) NOT NULL default '00',
  "AAABKOEQ" decimal(5,2) NOT NULL default '0.00',
  "AAABLOVD" decimal(7,2) NOT NULL default '0.00',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAAAXZPA" decimal(9,2) NOT NULL default '0.00',
  "AAAAXZPB" varchar(4) NOT NULL default ' ',
  "AAAAXZPC" varchar(4) NOT NULL default ' ',
  "AAAAXZPD" varchar(4) NOT NULL default ' ',
  "AAAAXZPE" varchar(4) NOT NULL default ' ',
  "AAAAXZPF" varchar(4) NOT NULL default ' ',
  "AAAAXZPG" varchar(4) NOT NULL default ' ',
  "AAAAXZPH" varchar(4) NOT NULL default ' ',
  "AAAAXZPI" varchar(4) NOT NULL default ' ',
  "AAAAXZPJ" varchar(4) NOT NULL default ' ',
  "AAABAPWT" varchar(4) NOT NULL default ' ',
  "AAABAPWU" varchar(4) NOT NULL default ' ',
  "AAABAPWV" varchar(4) NOT NULL default ' ',
  "AAAAXZPN" varchar(2) NOT NULL default ' ',
  "AAAAXZPO" varchar(2) NOT NULL default '00',
  "AAAAXZPP" varchar(8) NOT NULL default '00000000',
  "AAAAXZPQ" decimal(5,2) NOT NULL default '0.00',
  "AAABBBDA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXZPU","AAAAYBEA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXZQE" varchar(4) NOT NULL default ' ',
  "AAABJBVB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXZQE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZSD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZQL" decimal(7,2) NOT NULL default '0.00',
  "AAAAXZQM" varchar(4) NOT NULL default ' ',
  "AAAAXZQN" decimal(7,2) NOT NULL default '0.00',
  "AAAAXZQO" decimal(7,2) NOT NULL default '0.00',
  "AAAAXZQP" varchar(1) NOT NULL default ' ',
  "AAAAXZQQ" varchar(4) NOT NULL default ' ',
  "AAABJLBP" decimal(5,2) NOT NULL default '0.00',
  "AAAAXZQS" varchar(4) NOT NULL default ' ',
  "AAAAXZQT" decimal(7,2) NOT NULL default '0.00',
  "AAAAXZQU" varchar(4) NOT NULL default ' ',
  "AAAAXZQV" varchar(4) NOT NULL default ' ',
  "AAAAXZQW" varchar(8) NOT NULL default ' ',
  "AAAAXZQX" decimal(7,2) NOT NULL default '0.00',
  "AAAAXZQY" decimal(9,2) NOT NULL default '0.00',
  "AAAAXZQZ" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAXZRB" decimal(7,2) NOT NULL default '0.00',
  "AAAAXZRC" decimal(7,2) NOT NULL default '0.00',
  "AAAAXZRD" decimal(7,2) NOT NULL default '0.00',
  "AAAAXZRE" varchar(5) NOT NULL default ' ',
  "AAAAXZRF" varchar(5) NOT NULL default ' ',
  "AAAAXZRG" varchar(4) NOT NULL default ' ',
  "AAAAXZRH" varchar(4) NOT NULL default ' ',
  "AAAAXZRI" varchar(4) NOT NULL default ' ',
  "AAAAXZRJ" varchar(1) NOT NULL default ' ',
  "AAAAXZRK" varchar(4) NOT NULL default ' ',
  "AAAAXZRL" varchar(1) NOT NULL default ' ',
  "AAAAXZRM" varchar(8) NOT NULL default ' ',
  "AAAAXZRN" varchar(1) NOT NULL default ' ',
  "AAAAXZRO" varchar(1) NOT NULL default ' ',
  "AAABBBDA" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXZSD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZRU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZSD" varchar(4) NOT NULL default ' ',
  "AAAAXZRZ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAXZSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZSE" (
  "AAAAXZSK" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZSN" varchar(2) NOT NULL default ' ',
  "AAAAXZSJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAAAXZSK","AAABLSCE","AAABHVYP","AAAAXZSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMJXW" varchar(1) NOT NULL default ' ',
  "AAAAXZTE" varchar(1) NOT NULL default ' ',
  "AAAAXZTF" varchar(1) NOT NULL default ' ',
  "AAAAXZTG" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAXZSX" varchar(4) NOT NULL default ' ',
  "AAAAXZSY" varchar(4) NOT NULL default ' ',
  "AAAAXZSZ" varchar(4) NOT NULL default ' ',
  "AAAAXZTA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMJXW","AAAAXZTE","AAAAXZTF","AAAAXZTG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZVN" varchar(4) NOT NULL default ' ',
  "AAAAXZTM" varchar(25) NOT NULL default ' ',
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
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXZVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAXZVN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAXZVN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLFE" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
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
  PRIMARY KEY ("AAABMRIL","AAABGLFE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXZZE" varchar(2) NOT NULL default ' ',
  "AAAAXZZF" varchar(8) NOT NULL default ' ',
  "AAAAXZZG" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZBVY" varchar(2) NOT NULL default ' ',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAAAXZXU" varchar(36) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXZZE","AAAAXZZF","AAAAXZZG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXZZE" varchar(2) NOT NULL default ' ',
  "AAAAXZZF" varchar(8) NOT NULL default ' ',
  "AAAAXZZG" varchar(8) NOT NULL default '00000000',
  "AAAAXZZH" varchar(4) NOT NULL default '0000',
  "AAAAYBWT" varchar(8) NOT NULL default '00000000',
  "AAAAXZYK" varchar(2) NOT NULL default '00',
  "AAABGLGN" varchar(8) NOT NULL default '00000000',
  "AAAAXZYM" varchar(2) NOT NULL default '00',
  "AAAAYBWU" varchar(8) NOT NULL default '00000000',
  "AAAAXZYO" varchar(2) NOT NULL default '00',
  "AAABGLGV" varchar(8) NOT NULL default '00000000',
  "AAAAXZYQ" varchar(2) NOT NULL default '00',
  "AAAAYBWV" varchar(8) NOT NULL default '00000000',
  "AAAAXZYS" varchar(2) NOT NULL default '00',
  "AAABGLHD" varchar(8) NOT NULL default '00000000',
  "AAAAXZYU" varchar(2) NOT NULL default '00',
  "AAAAYBWW" varchar(8) NOT NULL default '00000000',
  "AAAAXZYW" varchar(2) NOT NULL default '00',
  "AAABGLHL" varchar(8) NOT NULL default '00000000',
  "AAAAXZYY" varchar(2) NOT NULL default '00',
  "AAAAYBWX" varchar(8) NOT NULL default '00000000',
  "AAAAXZZA" varchar(2) NOT NULL default '00',
  "AAABGLHT" varchar(8) NOT NULL default '00000000',
  "AAAAXZZC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAXZZE","AAAAXZZF","AAAAXZZG","AAAAXZZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAXZZU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAXZZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAXZZU" varchar(10) NOT NULL default ' ',
  "AAAAXZZR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAXZZU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAXZZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYAHN" varchar(2) NOT NULL default ' ',
  "AAAAXZZY" varchar(1) NOT NULL default ' ',
  "AAAAXZZZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYAHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBPOM" varchar(1) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAAAYAAJ" varchar(1) NOT NULL default ' ',
  "AAABAQKE" decimal(7,2) NOT NULL default '0.00',
  "AAABFJSR" varchar(1) NOT NULL default ' ',
  "AAABKOEQ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBPOM","AAABBAEF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBPOM" varchar(1) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABBAED" varchar(30) NOT NULL default ' ',
  "AAAAYAAX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBPOM","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYABC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCOI" varchar(4) NOT NULL default ' ',
  "AAAAYABF" varchar(1) NOT NULL default ' ',
  "AAAAYABG" varchar(30) NOT NULL default ' ',
  "AAAAYABH" varchar(1) NOT NULL default ' ',
  "AAABLZCR" varchar(40) NOT NULL default ' ',
  "AAABILNG" varchar(40) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLYNE" varchar(30) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAAAYABT" varchar(10) NOT NULL default ' ',
  "AAAAYABU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYCOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYABX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYADD" varchar(1) NOT NULL default ' ',
  "AAAAYDLB" varchar(2) NOT NULL default ' ',
  "AAAAYAMT" varchar(2) NOT NULL default ' ',
  "AAAAYAFJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYACF" varchar(2) NOT NULL default '00',
  "AAAAYACG" varchar(2) NOT NULL default '00',
  "AAAAYACH" varchar(1) NOT NULL default ' ',
  "AAAAYACI" varchar(1) NOT NULL default ' ',
  "AAAAYACJ" varchar(1) NOT NULL default ' ',
  "AAAAYACK" varchar(1) NOT NULL default ' ',
  "AAAAYACL" varchar(1) NOT NULL default ' ',
  "AAAAYACM" varchar(1) NOT NULL default ' ',
  "AAAAYACN" varchar(2) NOT NULL default '00',
  "AAAAYACO" varchar(2) NOT NULL default '00',
  "AAAAYACP" varchar(1) NOT NULL default ' ',
  "AAAAYACQ" varchar(4) NOT NULL default ' ',
  "AAAAYACR" varchar(4) NOT NULL default ' ',
  "AAAAYACS" varchar(1) NOT NULL default ' ',
  "AAAAYACT" varchar(1) NOT NULL default ' ',
  "AAAAYACU" varchar(1) NOT NULL default ' ',
  "AAAAYACV" varchar(1) NOT NULL default ' ',
  "AAAAYACW" varchar(1) NOT NULL default ' ',
  "AAAAYCPG" varchar(2) NOT NULL default ' ',
  "AAAAYACY" varchar(1) NOT NULL default ' ',
  "AAAAYACZ" varchar(2) NOT NULL default '00',
  "AAAAYADA" varchar(4) NOT NULL default ' ',
  "AAAAYADB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYADD","AAAAYDLB","AAAAYAMT","AAAAYAFJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYADI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCRG" varchar(2) NOT NULL default ' ',
  "AAABHOCX" varchar(1) NOT NULL default ' ',
  "AAAAYDLV" varchar(10) NOT NULL default ' ',
  "AAAAYDLW" varchar(10) NOT NULL default ' ',
  "AAAAYADO" varchar(1) NOT NULL default ' ',
  "AAAAYADP" varchar(1) NOT NULL default ' ',
  "AAAAYADQ" varchar(1) NOT NULL default ' ',
  "AAAAYADR" varchar(1) NOT NULL default ' ',
  "AAAAYADS" varchar(1) NOT NULL default ' ',
  "AAAAYADT" varchar(1) NOT NULL default ' ',
  "AAAAYADU" varchar(1) NOT NULL default ' ',
  "AAAAYADV" varchar(1) NOT NULL default ' ',
  "AAAAYADW" varchar(1) NOT NULL default ' ',
  "AAABLWYT" varchar(1) NOT NULL default ' ',
  "AAAAYADY" varchar(1) NOT NULL default ' ',
  "AAAAYADZ" varchar(1) NOT NULL default ' ',
  "AAAAYAEA" varchar(1) NOT NULL default ' ',
  "AAABHFFR" varchar(2) NOT NULL default ' ',
  "AAAAYAIB" varchar(1) NOT NULL default ' ',
  "AAAAYAIC" varchar(1) NOT NULL default ' ',
  "AAAAYAID" varchar(1) NOT NULL default ' ',
  "AAAAYAEF" varchar(2) NOT NULL default ' ',
  "AAAAYAEG" varchar(1) NOT NULL default ' ',
  "AAAAYAEH" varchar(1) NOT NULL default ' ',
  "AAAAYAEI" varchar(2) NOT NULL default ' ',
  "AAAAYAEJ" varchar(1) NOT NULL default ' ',
  "AAAAYAEK" varchar(2) NOT NULL default ' ',
  "AAAAYAEL" varchar(1) NOT NULL default ' ',
  "AAAAYAEM" varchar(1) NOT NULL default ' ',
  "AAAAYAFN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCRG","AAABHOCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCRG" varchar(2) NOT NULL default ' ',
  "AAABHOCX" varchar(1) NOT NULL default ' ',
  "AAABAEQI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCRG","AAABHOCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCRG" varchar(2) NOT NULL default ' ',
  "AAABHOCX" varchar(1) NOT NULL default ' ',
  "AAAAYAGE" varchar(1) NOT NULL default ' ',
  "AAAAYAFG" varchar(1) NOT NULL default ' ',
  "AAAAYAFH" varchar(1) NOT NULL default ' ',
  "AAAAYCRH" varchar(1) NOT NULL default ' ',
  "AAAAYAFJ" varchar(2) NOT NULL default ' ',
  "AAAAYAFK" varchar(1) NOT NULL default ' ',
  "AAAAYAFL" varchar(1) NOT NULL default ' ',
  "AAAAYAFM" varchar(1) NOT NULL default ' ',
  "AAAAYAFN" varchar(1) NOT NULL default ' ',
  "AAAAYAFO" varchar(1) NOT NULL default ' ',
  "AAAAYAFP" varchar(1) NOT NULL default ' ',
  "AAAAYAFQ" varchar(1) NOT NULL default ' ',
  "AAAAYAFR" varchar(1) NOT NULL default ' ',
  "AAAAYAFS" varchar(1) NOT NULL default ' ',
  "AAAAYAFT" varchar(1) NOT NULL default ' ',
  "AAAAYAFU" varchar(1) NOT NULL default ' ',
  "AAAAYAFV" varchar(1) NOT NULL default ' ',
  "AAAAYAFW" varchar(1) NOT NULL default ' ',
  "AAAAYAFX" varchar(1) NOT NULL default ' ',
  "AAAAYAFY" varchar(1) NOT NULL default ' ',
  "AAAAYAFZ" varchar(1) NOT NULL default ' ',
  "AAAAYDET" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCRG","AAABHOCX","AAAAYAGE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJVZ" varchar(1) NOT NULL default ' ',
  "AAABMCNO" varchar(2) NOT NULL default ' ',
  "AAAAYAGJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJVZ","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABFJVZ" varchar(1) NOT NULL default ' ',
  "AAABMCNO" varchar(2) NOT NULL default ' ',
  "AAAAYCNF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABFJVZ","AAABMCNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYAPP" varchar(1) NOT NULL default ' ',
  "AAAAYAPQ" varchar(1) NOT NULL default ' ',
  "AAABLJYA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYAPP","AAAAYAPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYAHL" varchar(2) NOT NULL default ' ',
  "AAAAYDKZ" varchar(5) NOT NULL default ' ',
  "AAAAYAHN" varchar(2) NOT NULL default ' ',
  "AAABGATU" varchar(1) NOT NULL default ' ',
  "AAABLWPB" varchar(1) NOT NULL default ' ',
  "AAAAYAOE" varchar(2) NOT NULL default '00',
  "AAABLCMU" varchar(20) NOT NULL default ' ',
  "AAABGCDF" varchar(1) NOT NULL default ' ',
  "AAAAYCXE" varchar(2) NOT NULL default ' ',
  "AAAAYAHU" varchar(1) NOT NULL default ' ',
  "AAAAYAHV" varchar(1) NOT NULL default ' ',
  "AAAAYAHW" varchar(1) NOT NULL default ' ',
  "AAAAYAHX" varchar(1) NOT NULL default ' ',
  "AAABIVOD" varchar(2) NOT NULL default ' ',
  "AAAAYAHZ" varchar(1) NOT NULL default ' ',
  "AAAAYAOG" varchar(1) NOT NULL default ' ',
  "AAAAYAIB" varchar(1) NOT NULL default ' ',
  "AAAAYAIC" varchar(1) NOT NULL default ' ',
  "AAAAYAID" varchar(1) NOT NULL default ' ',
  "AAAAYAIE" varchar(1) NOT NULL default ' ',
  "AAAAYAMP" varchar(4) NOT NULL default ' ',
  "AAAAYAMQ" varchar(1) NOT NULL default ' ',
  "AAAAYAIH" varchar(1) NOT NULL default ' ',
  "AAAAYAII" varchar(1) NOT NULL default ' ',
  "AAAAYAIJ" varchar(4) NOT NULL default ' ',
  "AAAAYAIK" varchar(1) NOT NULL default ' ',
  "AAAAYAIL" varchar(1) NOT NULL default ' ',
  "AAABKRZV" varchar(3) NOT NULL default ' ',
  "AAAAYAIN" decimal(7,2) NOT NULL default '0.00',
  "AAABKQHV" decimal(7,2) NOT NULL default '0.00',
  "AAAAYAIP" varchar(1) NOT NULL default ' ',
  "AAABLUGE" varchar(1) NOT NULL default ' ',
  "AAAAYAIR" varchar(1) NOT NULL default ' ',
  "AAAAYDKX" varchar(1) NOT NULL default ' ',
  "AAAAYAIT" varchar(1) NOT NULL default ' ',
  "AAAAYAIU" varchar(1) NOT NULL default ' ',
  "AAAAYAIV" varchar(4) NOT NULL default ' ',
  "AAAAYAIW" varchar(4) NOT NULL default ' ',
  "AAAAYDKY" varchar(1) NOT NULL default ' ',
  "AAAAYAIY" varchar(1) NOT NULL default ' ',
  "AAAAYAIZ" varchar(4) NOT NULL default ' ',
  "AAAAYAJA" varchar(4) NOT NULL default ' ',
  "AAAAYAJB" varchar(3) NOT NULL default ' ',
  "AAAAYAJC" varchar(1) NOT NULL default ' ',
  "AAAAYAJD" varchar(2) NOT NULL default ' ',
  "AAAAYAJE" varchar(2) NOT NULL default ' ',
  "AAAAYAJF" varchar(1) NOT NULL default ' ',
  "AAAAYTTL" varchar(1) NOT NULL default ' ',
  "AAAAYAJH" varchar(1) NOT NULL default ' ',
  "AAAAYAJI" varchar(1) NOT NULL default ' ',
  "AAABIBUK" varchar(1) NOT NULL default ' ',
  "AAAAYAOH" varchar(1) NOT NULL default ' ',
  "AAAAYAJL" varchar(4) NOT NULL default ' ',
  "AAAAYAJM" varchar(1) NOT NULL default ' ',
  "AAAAYAJN" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDMC","AAAAYDMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCXE" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYAJW" varchar(2) NOT NULL default ' ',
  "AAAAYAJX" varchar(2) NOT NULL default ' ',
  "AAAAYAJY" varchar(4) NOT NULL default ' ',
  "AAAAYAJZ" varchar(1) NOT NULL default ' ',
  "AAAAYAKA" varchar(6) NOT NULL default '000000',
  "AAAAYAKB" varchar(1) NOT NULL default ' ',
  "AAAAZWIU" varchar(4) NOT NULL default ' ',
  "AAAAYAKD" varchar(1) NOT NULL default ' ',
  "AAAAYAKE" varchar(6) NOT NULL default '000000',
  "AAAAYAKF" varchar(4) NOT NULL default ' ',
  "AAAAYAKG" varchar(1) NOT NULL default ' ',
  "AAAAYAKH" varchar(6) NOT NULL default '000000',
  "AAAAYAKI" varchar(4) NOT NULL default ' ',
  "AAAAYAKJ" varchar(4) NOT NULL default ' ',
  "AAAAYAKK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCXE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAABILTS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDMC","AAAAYDMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYDME" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYALF" varchar(4) NOT NULL default ' ',
  "AAAAYALG" varchar(5) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYAQT" varchar(4) NOT NULL default ' ',
  "AAAAYALJ" varchar(1) NOT NULL default ' ',
  "AAAAYALK" varchar(6) NOT NULL default '000000',
  "AAAAYCTN" varchar(4) NOT NULL default ' ',
  "AAAAYAXL" decimal(9,2) NOT NULL default '0.00',
  "AAAAYAXM" decimal(9,2) NOT NULL default '0.00',
  "AAAAYALO" varchar(1) NOT NULL default ' ',
  "AAAAYALP" varchar(1) NOT NULL default ' ',
  "AAAAYALQ" varchar(1) NOT NULL default ' ',
  "AAAAYALR" varchar(1) NOT NULL default ' ',
  "AAAAYALS" varchar(1) NOT NULL default ' ',
  "AAAAYALT" varchar(4) NOT NULL default ' ',
  "AAAAYALU" varchar(1) NOT NULL default ' ',
  "AAAAYALV" varchar(2) NOT NULL default ' ',
  "AAAAYALW" varchar(2) NOT NULL default ' ',
  "AAAAYALX" varchar(1) NOT NULL default ' ',
  "AAAAYALY" varchar(5) NOT NULL default ' ',
  "AAAAYALZ" varchar(5) NOT NULL default ' ',
  "AAAAYAMA" varchar(5) NOT NULL default ' ',
  "AAAAYAMB" varchar(5) NOT NULL default ' ',
  "AAAAYAMC" varchar(1) NOT NULL default ' ',
  "AAAAYAMD" varchar(4) NOT NULL default ' ',
  "AAAAYAME" varchar(1) NOT NULL default ' ',
  "AAAAYAMF" varchar(2) NOT NULL default ' ',
  "AAAAYAMG" varchar(1) NOT NULL default ' ',
  "AAAAYAMH" varchar(2) NOT NULL default ' ',
  "AAAAYAMI" varchar(2) NOT NULL default ' ',
  "AAAAYAMJ" varchar(1) NOT NULL default ' ',
  "AAAAYAMK" varchar(1) NOT NULL default ' ',
  "AAAAYAML" varchar(1) NOT NULL default ' ',
  "AAAAYAMM" varchar(5) NOT NULL default ' ',
  "AAAAYAMN" varchar(5) NOT NULL default ' ',
  "AAAAYAMO" varchar(1) NOT NULL default ' ',
  "AAAAYAMP" varchar(4) NOT NULL default ' ',
  "AAAAYAMQ" varchar(1) NOT NULL default ' ',
  "AAAAYAMR" varchar(1) NOT NULL default ' ',
  "AAAAYAMS" varchar(1) NOT NULL default ' ',
  "AAAAYAMT" varchar(2) NOT NULL default ' ',
  "AAAAYCPG" varchar(2) NOT NULL default ' ',
  "AAABKWRL" varchar(4) NOT NULL default ' ',
  "AAAAYAMW" varchar(1) NOT NULL default ' ',
  "AAAAYDKP" varchar(4) NOT NULL default ' ',
  "AAAAYAMY" varchar(4) NOT NULL default ' ',
  "AAAAYCVV" varchar(2) NOT NULL default '00',
  "AAAAYCVW" varchar(2) NOT NULL default '00',
  "AAAAYCVX" varchar(2) NOT NULL default '00',
  "AAAAYCVY" varchar(2) NOT NULL default '00',
  "AAAAYCVZ" varchar(1) NOT NULL default ' ',
  "AAAAYCWA" varchar(2) NOT NULL default ' ',
  "AAAAYDAC" varchar(2) NOT NULL default ' ',
  "AAAAYCWC" varchar(1) NOT NULL default ' ',
  "AAAAYCWD" varchar(1) NOT NULL default ' ',
  "AAABANVG" varchar(1) NOT NULL default ' ',
  "AAAAYCWF" varchar(1) NOT NULL default ' ',
  "AAABEBCS" varchar(1) NOT NULL default ' ',
  "AAAAYCWH" varchar(1) NOT NULL default ' ',
  "AAAAYANM" varchar(1) NOT NULL default ' ',
  "AAAAYANN" varchar(1) NOT NULL default ' ',
  "AAAAYANO" varchar(1) NOT NULL default ' ',
  "AAAAYANP" varchar(1) NOT NULL default ' ',
  "AAAAYANQ" varchar(1) NOT NULL default ' ',
  "AAAAYANR" varchar(4) NOT NULL default ' ',
  "AAAAYANS" varchar(4) NOT NULL default ' ',
  "AAAAYANT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDMC","AAAAYDMD","AAAAYDME","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYANZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYAQT" varchar(4) NOT NULL default ' ',
  "AAAAYAOC" varchar(1) NOT NULL default ' ',
  "AAAAYAOD" varchar(1) NOT NULL default ' ',
  "AAAAYAOE" varchar(2) NOT NULL default '00',
  "AAABLCMU" varchar(20) NOT NULL default ' ',
  "AAAAYAOG" varchar(1) NOT NULL default ' ',
  "AAAAYAOH" varchar(1) NOT NULL default ' ',
  "AAABAXDP" varchar(1) NOT NULL default ' ',
  "AAAAYAOJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYAQT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDIG" varchar(2) NOT NULL default ' ',
  "AAAAYAOP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDIG" varchar(2) NOT NULL default ' ',
  "AAABAMRI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYAPP" varchar(1) NOT NULL default ' ',
  "AAAAYAPQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYAPP","AAAAYAPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYAPP" varchar(1) NOT NULL default ' ',
  "AAAAYAPQ" varchar(1) NOT NULL default ' ',
  "AAABLJYA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYAPP","AAAAYAPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFFR" varchar(2) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFFR","AAABLUAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCWA" varchar(2) NOT NULL default ' ',
  "AAABAXDP" varchar(1) NOT NULL default ' ',
  "AAAAYARE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFVBU" decimal(5,2) NOT NULL default '0.00',
  "AAAAYAQG" decimal(5,2) NOT NULL default '0.00',
  "AAAAYAQH" varchar(2) NOT NULL default ' ',
  "AAAAYAQI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYAQJ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYAQK" decimal(5,2) NOT NULL default '0.00',
  "AAAAYAQL" varchar(2) NOT NULL default '00',
  "AAAAYCZU" varchar(2) NOT NULL default '00',
  "AAAAYCZV" varchar(4) NOT NULL default ' ',
  "AAAAYCZW" varchar(4) NOT NULL default ' ',
  "AAAAYAQP" varchar(1) NOT NULL default ' ',
  "AAAAYAQQ" varchar(2) NOT NULL default '00',
  "AAAAYCZX" varchar(1) NOT NULL default ' ',
  "AAAAYCZY" varchar(1) NOT NULL default ' ',
  "AAAAYAQT" varchar(4) NOT NULL default ' ',
  "AAAAYAQU" varchar(4) NOT NULL default ' ',
  "AAAAZNXD" varchar(2) NOT NULL default ' ',
  "AAAAYCZZ" varchar(6) NOT NULL default '000000',
  "AAAAYAQX" varchar(1) NOT NULL default ' ',
  "AAAAYAQY" varchar(1) NOT NULL default ' ',
  "AAAAYAQZ" varchar(4) NOT NULL default ' ',
  "AAAAYARA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCWA","AAABAXDP","AAAAYARE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYARG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYARP" varchar(4) NOT NULL default ' ',
  "AAAAYARQ" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYARM" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYARN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYARP","AAAAYARQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYARS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLXR" varchar(4) NOT NULL default ' ',
  "AAAAYDFX" varchar(10) NOT NULL default ' ',
  "AAAAYASI" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHCJ" varchar(4) NOT NULL default ' ',
  "AAAAYDFO" varchar(1) NOT NULL default ' ',
  "AAAAYDFP" varchar(1) NOT NULL default ' ',
  "AAAAYDJG" varchar(1) NOT NULL default ' ',
  "AAAAYASE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLXR","AAAAYDFX","AAAAYASI","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYASL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYDME" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYASS" varchar(5) NOT NULL default ' ',
  "AAAAYAST" varchar(2) NOT NULL default ' ',
  "AAAAYASU" varchar(1) NOT NULL default ' ',
  "AAAAYASV" varchar(1) NOT NULL default ' ',
  "AAAAYAZQ" varchar(4) NOT NULL default ' ',
  "AAAAYAVP" varchar(1) NOT NULL default ' ',
  "AAAAYDCS" varchar(4) NOT NULL default ' ',
  "AAAAYASZ" varchar(4) NOT NULL default ' ',
  "AAAAYATA" decimal(7,2) NOT NULL default '0.00',
  "AAAAYATB" decimal(7,2) NOT NULL default '0.00',
  "AAAAYATC" varchar(5) NOT NULL default ' ',
  "AAAAYATD" varchar(5) NOT NULL default ' ',
  "AAAAYDCH" varchar(1) NOT NULL default ' ',
  "AAAAYDCI" varchar(6) NOT NULL default '000000',
  "AAAAYATG" varchar(5) NOT NULL default ' ',
  "AAAAYATH" varchar(1) NOT NULL default ' ',
  "AAAAYATI" varchar(6) NOT NULL default '000000',
  "AAAAYCMO" varchar(4) NOT NULL default ' ',
  "AAAAYATK" varchar(4) NOT NULL default ' ',
  "AAAAYCMN" varchar(4) NOT NULL default ' ',
  "AAAAYATM" varchar(1) NOT NULL default ' ',
  "AAAAYATN" varchar(4) NOT NULL default ' ',
  "AAAAYATO" varchar(1) NOT NULL default ' ',
  "AAAAYATP" varchar(4) NOT NULL default ' ',
  "AAAAYATQ" varchar(4) NOT NULL default ' ',
  "AAAAYATR" varchar(2) NOT NULL default '00',
  "AAAAYATS" varchar(3) NOT NULL default '000',
  "AAAAYATT" varchar(1) NOT NULL default ' ',
  "AAAAYATU" varchar(4) NOT NULL default ' ',
  "AAAAYATV" varchar(1) NOT NULL default ' ',
  "AAAAYATW" varchar(1) NOT NULL default ' ',
  "AAAAYDDY" varchar(4) NOT NULL default ' ',
  "AAAAYATY" varchar(4) NOT NULL default ' ',
  "AAAAYATZ" varchar(4) NOT NULL default ' ',
  "AAAAYAUA" varchar(2) NOT NULL default '00',
  "AAAAYAUB" varchar(2) NOT NULL default '00',
  "AAAAYAUC" varchar(4) NOT NULL default ' ',
  "AAAAYAUD" varchar(4) NOT NULL default ' ',
  "AAAAYAUE" varchar(4) NOT NULL default ' ',
  "AAAAYAUF" varchar(4) NOT NULL default ' ',
  "AAAAYAUG" varchar(4) NOT NULL default ' ',
  "AAAAYAUH" varchar(4) NOT NULL default ' ',
  "AAAAYAUI" varchar(2) NOT NULL default ' ',
  "AAAAYAUJ" varchar(4) NOT NULL default ' ',
  "AAAAYCZD" varchar(1) NOT NULL default ' ',
  "AAAAYAUL" varchar(1) NOT NULL default ' ',
  "AAAAYAUM" varchar(5) NOT NULL default ' ',
  "AAAAYAUN" varchar(5) NOT NULL default ' ',
  "AAAAYAUO" varchar(1) NOT NULL default ' ',
  "AAAAYAUP" varchar(1) NOT NULL default ' ',
  "AAAAYFSL" varchar(4) NOT NULL default ' ',
  "AAAAYAUR" varchar(4) NOT NULL default ' ',
  "AAAAYAUS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDMC","AAAAYDMD","AAAAYDME","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYDME" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYAWM" varchar(4) NOT NULL default ' ',
  "AAAAYAXO" varchar(2) NOT NULL default '00',
  "AAAAYAXP" varchar(1) NOT NULL default ' ',
  "AAAAYAXQ" varchar(2) NOT NULL default '00',
  "AAAAYAXR" varchar(2) NOT NULL default '00',
  "AAAAYAXS" varchar(1) NOT NULL default ' ',
  "AAAAYAXT" varchar(2) NOT NULL default '00',
  "AAAAYAXU" varchar(2) NOT NULL default '00',
  "AAAAYAXV" varchar(1) NOT NULL default ' ',
  "AAAAYAXW" varchar(3) NOT NULL default '000',
  "AAAAYAVP" varchar(1) NOT NULL default ' ',
  "AAAAYDKZ" varchar(5) NOT NULL default ' ',
  "AAAAYAXX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDMC","AAAAYDMD","AAAAYDME","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYAWM" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAAAYAWB" decimal(5,2) NOT NULL default '0.00',
  "AAAAYAWC" decimal(5,2) NOT NULL default '0.00',
  "AAAAYAWD" decimal(5,2) NOT NULL default '0.00',
  "AAAAYAZA" varchar(5) NOT NULL default ' ',
  "AAAAYAZB" decimal(7,2) NOT NULL default '0.00',
  "AAAAYAZC" varchar(5) NOT NULL default ' ',
  "AAAAYAZD" varchar(5) NOT NULL default ' ',
  "AAAAYAZE" decimal(7,2) NOT NULL default '0.00',
  "AAAAYAZF" varchar(5) NOT NULL default ' ',
  "AAAAYAXN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYAWM","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCNI" varchar(1) NOT NULL default ' ',
  "AAAAYAXN" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAAAYAWT" varchar(2) NOT NULL default '00',
  "AAAAYAWU" varchar(2) NOT NULL default '00',
  "AAAAYAWV" varchar(2) NOT NULL default '00',
  "AAAAYAWW" varchar(2) NOT NULL default '00',
  "AAAAYDLS" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYAWY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCNI","AAAAYAXN","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYDME" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYAXK" decimal(9,2) NOT NULL default '0.00',
  "AAAAYAXL" decimal(9,2) NOT NULL default '0.00',
  "AAAAYAXM" decimal(9,2) NOT NULL default '0.00',
  "AAAAYAXN" varchar(4) NOT NULL default ' ',
  "AAAAYAXO" varchar(2) NOT NULL default '00',
  "AAAAYAXP" varchar(1) NOT NULL default ' ',
  "AAAAYAXQ" varchar(2) NOT NULL default '00',
  "AAAAYAXR" varchar(2) NOT NULL default '00',
  "AAAAYAXS" varchar(1) NOT NULL default ' ',
  "AAAAYAXT" varchar(2) NOT NULL default '00',
  "AAAAYAXU" varchar(2) NOT NULL default '00',
  "AAAAYAXV" varchar(1) NOT NULL default ' ',
  "AAAAYAXW" varchar(3) NOT NULL default '000',
  "AAAAYAXX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDMC","AAAAYDMD","AAAAYDME","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYDME" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYAYK" decimal(9,2) NOT NULL default '0.00',
  "AAAAYAYL" varchar(5) NOT NULL default ' ',
  "AAAAYAYM" decimal(7,2) NOT NULL default '0.00',
  "AAAAYAYN" varchar(30) NOT NULL default ' ',
  "AAAAYAYO" decimal(7,2) NOT NULL default '0.00',
  "AAAAYAYP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDMC","AAAAYDMD","AAAAYDME","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYAZQ" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAAAYDLS" decimal(5,2) NOT NULL default '0.00',
  "AAAAYAZA" varchar(5) NOT NULL default ' ',
  "AAAAYAZB" decimal(7,2) NOT NULL default '0.00',
  "AAAAYAZC" varchar(5) NOT NULL default ' ',
  "AAAAYAZD" varchar(5) NOT NULL default ' ',
  "AAAAYAZE" decimal(7,2) NOT NULL default '0.00',
  "AAAAYAZF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYAZQ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYAZQ" varchar(4) NOT NULL default ' ',
  "AAAAYAZN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYAZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYAZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDGV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAAAYBAA" varchar(1) NOT NULL default ' ',
  "AAAAYBAB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDGV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYBBA" varchar(1) NOT NULL default ' ',
  "AAAAYBAJ" varchar(1) NOT NULL default ' ',
  "AAAAYBAK" varchar(1) NOT NULL default ' ',
  "AAAAYBAL" varchar(1) NOT NULL default ' ',
  "AAAAYBCD" varchar(1) NOT NULL default ' ',
  "AAAAYBAN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDIX","AAAAYBBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYBBA" varchar(1) NOT NULL default ' ',
  "AAAAYBAW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDIX","AAAAYBBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXEI" varchar(4) NOT NULL default ' ',
  "AAABHOCT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABFXEI" varchar(4) NOT NULL default ' ',
  "AAAAYBBL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABFXEI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCXE" varchar(2) NOT NULL default ' ',
  "AAAAYBCD" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYBBV" decimal(5,2) NOT NULL default '0.00',
  "AAAAYBBW" varchar(1) NOT NULL default ' ',
  "AAAAYBBX" decimal(5,2) NOT NULL default '0.00',
  "AAAAYBBY" decimal(5,2) NOT NULL default '0.00',
  "AAAAYBBZ" varchar(1) NOT NULL default ' ',
  "AAAAYBCA" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYCXE","AAAAYBCD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBCK" varchar(4) NOT NULL default ' ',
  "AAAAYBCI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYBCR" varchar(1) NOT NULL default ' ',
  "AAAAYBCS" varchar(4) NOT NULL default ' ',
  "AAAAYBCT" varchar(4) NOT NULL default ' ',
  "AAAAYBCU" varchar(4) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAAAYBCW" varchar(2) NOT NULL default ' ',
  "AAAAYBCX" varchar(2) NOT NULL default ' ',
  "AAAAYBCY" varchar(1) NOT NULL default ' ',
  "AAAAYDLR" varchar(1) NOT NULL default ' ',
  "AAAAYDLS" decimal(5,2) NOT NULL default '0.00',
  "AAAAYDLV" varchar(10) NOT NULL default ' ',
  "AAAAYDLW" varchar(10) NOT NULL default ' ',
  "AAAAYBDD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAYDMD","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYBEA" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYBDQ" varchar(2) NOT NULL default ' ',
  "AAAAYBDR" varchar(8) NOT NULL default ' ',
  "AAAAYBDS" varchar(2) NOT NULL default ' ',
  "AAAAYBDT" varchar(8) NOT NULL default ' ',
  "AAAAYBDU" varchar(2) NOT NULL default ' ',
  "AAAAYBDV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMCVI","AAABBHJS","AAAAYBEA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBIC" varchar(4) NOT NULL default '0000',
  "AAABLKMN" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(16) NOT NULL default ' ',
  "AAAAYBEH" varchar(40) NOT NULL default ' ',
  "AAABBICM" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAYBIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYBIC" varchar(4) NOT NULL default '0000',
  "AAAAYBEQ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYBIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBIC" varchar(4) NOT NULL default '0000',
  "AAAAYBGY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBIC","AAAAYBGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYBIC" varchar(4) NOT NULL default '0000',
  "AAAAYBGY" varchar(4) NOT NULL default ' ',
  "AAAAYBFG" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYBIC","AAAAYBGY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBIC" varchar(4) NOT NULL default '0000',
  "AAAAYDAO" varchar(4) NOT NULL default ' ',
  "AAABBICM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBIC","AAAAYDAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBIC" varchar(4) NOT NULL default '0000',
  "AAABBQOI" varchar(8) NOT NULL default ' ',
  "AAABFNHW" varchar(30) NOT NULL default ' ',
  "AAAAYBFY" varchar(3) NOT NULL default '000',
  "AAAAYBJS" varchar(16) NOT NULL default ' ',
  "AAAAYBGY" varchar(4) NOT NULL default ' ',
  "AAABKGBL" varchar(1) NOT NULL default '0',
  "AAAAYBGC" varchar(1) NOT NULL default '0',
  "AAAAYBGD" varchar(1) NOT NULL default '0',
  "AAAAYBGE" varchar(30) NOT NULL default ' ',
  "AAAAYBGF" varchar(2) NOT NULL default ' ',
  "AAABABQC" varchar(1) NOT NULL default ' ',
  "AAAAYBGH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBIC","AAABBQOI","AAABFNHW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBIC" varchar(4) NOT NULL default '0000',
  "AAAAYBGY" varchar(4) NOT NULL default ' ',
  "AAAAYBGZ" varchar(5) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYBHB" varchar(5) NOT NULL default ' ',
  "AAABLRMF" varchar(4) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAAAYBGV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBIC","AAAAYBGY","AAAAYBGZ","AAABHVYL","AAAAYBHB","AAABLRMF","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBHU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYBHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYBHU" varchar(2) NOT NULL default '00',
  "AAAAYBHN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYBHU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBIC" varchar(4) NOT NULL default '0000',
  "AAAAYBHU" varchar(2) NOT NULL default '00',
  "AAAAYBHV" varchar(6) NOT NULL default ' ',
  "AAAAYBHW" varchar(4) NOT NULL default ' ',
  "AAAAYBHX" varchar(12) NOT NULL default ' ',
  "AAAAYBHY" tinyint(3) unsigned NOT NULL default '0',
  "AAABJARY" varchar(12) NOT NULL default ' ',
  "AAABHFFR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBIC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRMF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLRMF" varchar(4) NOT NULL default ' ',
  "AAAAYBIM" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLRMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDAO" varchar(4) NOT NULL default ' ',
  "AAABBICM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDAO" varchar(4) NOT NULL default ' ',
  "AAAAYBJA" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBJS" varchar(16) NOT NULL default ' ',
  "AAAAYBJH" varchar(1) NOT NULL default ' ',
  "AAABBICM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYBJS" varchar(16) NOT NULL default ' ',
  "AAAAYBJP" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYBJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRMF" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRMF","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAYBKU" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABEXGG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAAAYBKU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAYBKU" varchar(4) NOT NULL default ' ',
  "AAABILTS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGCHD","AAAAYBKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABEXGG" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABEXGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABEXGG" varchar(3) NOT NULL default '000',
  "AAABLPUZ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABGCHD","AAABEXGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABEXGG" varchar(3) NOT NULL default '000',
  "AAABMRCU" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAAAYBLS" varchar(2) NOT NULL default '00',
  "AAAAYBLT" varchar(2) NOT NULL default '00',
  "AAAAYBLU" varchar(1) NOT NULL default ' ',
  "AAAAYBLV" varchar(1) NOT NULL default ' ',
  "AAAAYBLW" varchar(1) NOT NULL default ' ',
  "AAAAYBLX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABEXGG","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIGMU" varchar(4) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAAAYBNW" varchar(8) NOT NULL default ' ',
  "AAABLADC" varchar(2) NOT NULL default ' ',
  "AAAAYBOZ" varchar(2) NOT NULL default ' ',
  "AAAAYBNZ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLWJT" varchar(30) NOT NULL default ' ',
  "AAABLBEI" varchar(6) NOT NULL default '000000',
  "AAAAYBMP" varchar(4) NOT NULL default ' ',
  "AAABHEQK" varchar(40) NOT NULL default ' ',
  "AAABFREQ" varchar(20) NOT NULL default ' ',
  "AAAAYBMS" varchar(20) NOT NULL default ' ',
  "AAAAYBMT" varchar(1) NOT NULL default ' ',
  "AAAAYBMU" varchar(2) NOT NULL default ' ',
  "AAAAYBMV" varchar(2) NOT NULL default ' ',
  "AAAAYBMW" varchar(1) NOT NULL default ' ',
  "AAABLQTM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIGMU","AAABJFGL","AAABMKXW","AAAAYBNW","AAABLADC","AAAAYBOZ","AAAAYBNZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAAAYBNW" varchar(8) NOT NULL default ' ',
  "AAABLADC" varchar(2) NOT NULL default ' ',
  "AAAAYBOZ" varchar(2) NOT NULL default ' ',
  "AAAAYBNZ" varchar(2) NOT NULL default ' ',
  "AAAAYBOA" varchar(25) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYBNS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABMKXW","AAAAYBNW","AAABLADC","AAAAYBOZ","AAAAYBNZ","AAAAYBOA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYCAY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCAX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJFGL","AAABHVYL","AAAAYCAY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLADC" varchar(2) NOT NULL default ' ',
  "AAAAYBOZ" varchar(2) NOT NULL default ' ',
  "AAAAYCAY" varchar(2) NOT NULL default ' ',
  "AAAAYBOW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLADC","AAAAYBOZ","AAAAYCAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYBPN" varchar(1) NOT NULL default ' ',
  "AAAAYCAY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCAX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAAAYBPN","AAAAYCAY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYBQB" varchar(6) NOT NULL default ' ',
  "AAAAYBQC" varchar(1) NOT NULL default ' ',
  "AAAAYCAY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCAX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYBQB","AAAAYBQC","AAAAYCAY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZSAM" varchar(4) NOT NULL default ' ',
  "AAABAMJC" varchar(2) NOT NULL default ' ',
  "AAAAYBQV" varchar(1) NOT NULL default ' ',
  "AAAAYBQW" varchar(1) NOT NULL default ' ',
  "AAAAYBQX" varchar(2) NOT NULL default ' ',
  "AAAAYCAY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCAX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAZSAM","AAABAMJC","AAAAYBQV","AAAAYBQW","AAAAYBQX","AAAAYCAY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLHXQ" varchar(4) NOT NULL default ' ',
  "AAABBFVS" varchar(2) NOT NULL default ' ',
  "AAAAYBRU" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYBRW" varchar(1) NOT NULL default ' ',
  "AAAAYBRX" varchar(1) NOT NULL default ' ',
  "AAAAYBRY" varchar(1) NOT NULL default ' ',
  "AAAAYBRZ" varchar(1) NOT NULL default ' ',
  "AAAAYBSA" varchar(1) NOT NULL default ' ',
  "AAAAYCAY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCAX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLHXQ","AAABBFVS","AAAAYBRU","AAABHVYL","AAAAYBRW","AAAAYBRX","AAAAYBRY","AAAAYBRZ","AAAAYBSA","AAAAYCAY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAAAZNVH" varchar(1) NOT NULL default ' ',
  "AAAAYBSP" varchar(8) NOT NULL default ' ',
  "AAAAYBSQ" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYBSL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAAAZNVH","AAAAYBSP","AAAAYBSQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBYE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYBYE" varchar(20) NOT NULL default ' ',
  "AAAAYBTB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYBYE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBXV" varchar(20) NOT NULL default ' ',
  "AAABMORS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYBXV" varchar(20) NOT NULL default ' ',
  "AAAAYCIW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYBXV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRQS" varchar(20) NOT NULL default ' ',
  "AAAAYBTW" varchar(30) NOT NULL default ' ',
  "AAAAYBXH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABKRQS" varchar(20) NOT NULL default ' ',
  "AAAAYBUE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABKRQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBXH" varchar(20) NOT NULL default ' ',
  "AAAAYBUL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYBXH" varchar(20) NOT NULL default ' ',
  "AAAAYBUS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYBXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBXK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYBXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYBXK" varchar(20) NOT NULL default ' ',
  "AAAAYBVF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYBXK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYTCA" varchar(4) NOT NULL default ' ',
  "AAAAYBXK" varchar(20) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLMAE" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAYTCA","AAAAYBXK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAEF" varchar(10) NOT NULL default ' ',
  "AAAAYBXK" varchar(20) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAEF","AAAAYBXK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYBXK" varchar(20) NOT NULL default ' ',
  "AAAAYBYE" varchar(20) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAYBWT" varchar(1) NOT NULL default ' ',
  "AAAAYBWU" varchar(1) NOT NULL default ' ',
  "AAAAYBWV" varchar(1) NOT NULL default ' ',
  "AAAAYBWW" varchar(1) NOT NULL default ' ',
  "AAAAYBWX" varchar(1) NOT NULL default ' ',
  "AAAAYBWY" varchar(1) NOT NULL default ' ',
  "AAAAYBWZ" varchar(1) NOT NULL default ' ',
  "AAAAYBXA" varchar(1) NOT NULL default ' ',
  "AAAAYBXB" varchar(1) NOT NULL default ' ',
  "AAAAYBXC" varchar(1) NOT NULL default ' ',
  "AAAAYBXD" varchar(1) NOT NULL default ' ',
  "AAAAYBXE" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYBXG" varchar(4) NOT NULL default ' ',
  "AAAAYBXH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYBXK","AAAAYBYE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBYE" varchar(20) NOT NULL default ' ',
  "AAAAYBXV" varchar(20) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYBYE","AAAAYBXV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYBYE" varchar(20) NOT NULL default ' ',
  "AAABKRQS" varchar(20) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYBYE","AAABKRQS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYCAI" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYCAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYCAI" varchar(6) NOT NULL default ' ',
  "AAAAYBYT" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAYCAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYCAJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYCAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYCAJ" varchar(6) NOT NULL default ' ',
  "AAABBBGP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAYCAJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYBZP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYCAI" varchar(6) NOT NULL default ' ',
  "AAAAYCAJ" varchar(6) NOT NULL default ' ',
  "AAABBGYO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFREQ" varchar(5) NOT NULL default ' ',
  "AAABAZEY" varchar(1) NOT NULL default ' ',
  "AAABEJKE" decimal(5,2) NOT NULL default '0.00',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAAAYCAE" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYCAI","AAAAYCAJ","AAABBGYO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCAX" varchar(3) NOT NULL default ' ',
  "AAAAYCAY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCAS" varchar(2) NOT NULL default '00',
  "AAAAYCAT" varchar(2) NOT NULL default '00',
  "AAAAYCAU" varchar(1) NOT NULL default ' ',
  "AAAAYCAV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCAX","AAAAYCAY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAABKRQS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAABKRQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCHH" varchar(4) NOT NULL default ' ',
  "AAAAYCBQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAAAYCHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAABKRQS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCCA" decimal(9,2) NOT NULL default '0.00',
  "AAAAYCCB" varchar(1) NOT NULL default ' ',
  "AAABLOQL" decimal(7,2) NOT NULL default '0.00',
  "AAAAYCCD" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(4) NOT NULL default ' ',
  "AAAAYCCF" varchar(4) NOT NULL default ' ',
  "AAAAYCGF" varchar(4) NOT NULL default ' ',
  "AAAAYCCH" varchar(2) NOT NULL default ' ',
  "AAAAYCCI" varchar(2) NOT NULL default ' ',
  "AAABBLTD" varchar(4) NOT NULL default ' ',
  "AAABDWBA" varchar(1) NOT NULL default ' ',
  "AAAAYCGX" varchar(2) NOT NULL default ' ',
  "AAAAYCCM" varchar(1) NOT NULL default ' ',
  "AAAAYCCN" varchar(1) NOT NULL default ' ',
  "AAAAYCCO" varchar(1) NOT NULL default ' ',
  "AAAAYCCP" varchar(2) NOT NULL default ' ',
  "AAAAYCCQ" varchar(2) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYCLM" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYCCU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAABKRQS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCHH" varchar(4) NOT NULL default ' ',
  "AAABKRQS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCDG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAAAYCHH","AAABKRQS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCGX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAAAYCGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCGN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAZWEP" varchar(1) NOT NULL default ' ',
  "AAAAYCGX" varchar(2) NOT NULL default ' ',
  "AAAAYCGN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAAAZWEP","AAAAYCGX","AAAAYCGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCGF" varchar(4) NOT NULL default ' ',
  "AAAAYCEN" varchar(4) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAAAYCGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCGF" varchar(4) NOT NULL default ' ',
  "AAAAYCFF" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCEZ" varchar(4) NOT NULL default ' ',
  "AAAAYCFA" varchar(2) NOT NULL default '00',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAAAYCGF","AAAAYCFF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCFU" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCFN" varchar(60) NOT NULL default ' ',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAAAYCFP" varchar(8) NOT NULL default ' ',
  "AAAAYCFQ" varchar(1) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCHZ","AAAAYCFU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCGF" varchar(4) NOT NULL default ' ',
  "AAAAYCGB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCHZ","AAAAYCGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCGN" varchar(2) NOT NULL default ' ',
  "AAAAYCGK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCGX" varchar(2) NOT NULL default ' ',
  "AAAAYCGT" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCHZ","AAAAYCGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCHH" varchar(4) NOT NULL default ' ',
  "AAAAYCHD" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCHZ","AAAAYCHH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAABKRQS" varchar(4) NOT NULL default ' ',
  "AAAAYCHN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCHZ","AAABKRQS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCHZ" varchar(4) NOT NULL default ' ',
  "AAAAYCHW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHT" varchar(1) NOT NULL default ' ',
  "AAABGCHS" varchar(2) NOT NULL default '00',
  "AAABGCHU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHT","AAABGCHS","AAABGCHU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCJX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCJX" varchar(4) NOT NULL default ' ',
  "AAAAYCIW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAEPB" varchar(2) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYCWM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAEPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAEPB" varchar(2) NOT NULL default ' ',
  "AAAAZKCC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAEPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCJX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCJU" varchar(1) NOT NULL default ' ',
  "AAAAYCJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCJX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAABBNQD" varchar(2) NOT NULL default ' ',
  "AAAAYCKV" varchar(4) NOT NULL default ' ',
  "AAAAYCLD" varchar(4) NOT NULL default ' ',
  "AAAAYHTL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDIX","AAABBNQD","AAAAYCKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAABBNQD" varchar(2) NOT NULL default ' ',
  "AAAAYCKV" varchar(4) NOT NULL default ' ',
  "AAABJFPC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDIX","AAABBNQD","AAAAYCKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCLD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCLD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYHTL" varchar(4) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYCLJ" varchar(2) NOT NULL default ' ',
  "AAAAYCLK" varchar(2) NOT NULL default '00',
  "AAAAYCLL" decimal(3,0) NOT NULL default '0',
  "AAAAYCLM" varchar(1) NOT NULL default ' ',
  "AAAAYCLN" varchar(3) NOT NULL default '000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAYDIG" varchar(2) NOT NULL default ' ',
  "AAAAYDJP" varchar(2) NOT NULL default ' ',
  "AAAAYCLR" decimal(9,2) NOT NULL default '0.00',
  "AAAAYCLS" decimal(9,2) NOT NULL default '0.00',
  "AAAAYCLT" decimal(5,2) NOT NULL default '0.00',
  "AAABEJLV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYHTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYHTL" varchar(4) NOT NULL default ' ',
  "AAAAYCMB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYHTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCMQ" varchar(4) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCML" varchar(4) NOT NULL default ' ',
  "AAAAYCMM" varchar(4) NOT NULL default ' ',
  "AAAAYCMN" varchar(4) NOT NULL default ' ',
  "AAAAYCMO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCMQ","AAAAYDIX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCNI" varchar(1) NOT NULL default ' ',
  "AAAAYCNJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCNI","AAAAYCNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCNI" varchar(1) NOT NULL default ' ',
  "AAAAYCNJ" varchar(4) NOT NULL default ' ',
  "AAAAYCNF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCNI","AAAAYCNJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCOI" varchar(4) NOT NULL default ' ',
  "AAABHFFR" varchar(2) NOT NULL default ' ',
  "AAAAYCNO" varchar(30) NOT NULL default ' ',
  "AAAAYCNP" varchar(30) NOT NULL default ' ',
  "AAAAYCNQ" varchar(30) NOT NULL default ' ',
  "AAAAYCNR" varchar(30) NOT NULL default ' ',
  "AAAAYCNS" varchar(30) NOT NULL default ' ',
  "AAAAYCNT" varchar(30) NOT NULL default ' ',
  "AAAAYCNU" varchar(10) NOT NULL default ' ',
  "AAAAYCNV" varchar(10) NOT NULL default ' ',
  "AAAAYCNW" varchar(30) NOT NULL default ' ',
  "AAAAYCNX" varchar(1) NOT NULL default ' ',
  "AAAAYCNY" varchar(1) NOT NULL default ' ',
  "AAABBOBP" varchar(20) NOT NULL default ' ',
  "AAABBOBS" varchar(20) NOT NULL default ' ',
  "AAAAYCOB" varchar(40) NOT NULL default ' ',
  "AAAAYCOC" varchar(40) NOT NULL default ' ',
  "AAAAYCOD" varchar(40) NOT NULL default ' ',
  "AAAAYCOE" varchar(40) NOT NULL default ' ',
  "AAAAYCOF" varchar(1) NOT NULL default ' ',
  "AAAAYCOG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCOI","AAABHFFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDLA" varchar(2) NOT NULL default ' ',
  "AAAAYCPG" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCOQ" varchar(1) NOT NULL default ' ',
  "AAAAYCOR" varchar(1) NOT NULL default ' ',
  "AAAAYCOS" varchar(1) NOT NULL default ' ',
  "AAAAYCOT" varchar(3) NOT NULL default '000',
  "AAAAYCOU" varchar(1) NOT NULL default ' ',
  "AAAAYCOV" varchar(1) NOT NULL default ' ',
  "AAAAYDFO" varchar(1) NOT NULL default ' ',
  "AAAAYCOX" varchar(2) NOT NULL default ' ',
  "AAAAYCOY" varchar(2) NOT NULL default ' ',
  "AAAAYCOZ" varchar(5) NOT NULL default ' ',
  "AAAAYCPA" varchar(2) NOT NULL default ' ',
  "AAAAYCPB" varchar(5) NOT NULL default ' ',
  "AAAAYCPC" varchar(5) NOT NULL default ' ',
  "AAAAYCPD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDLA","AAAAYCPG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCRG" varchar(2) NOT NULL default ' ',
  "AAAAYCPL" varchar(1) NOT NULL default ' ',
  "AAAAYCPM" varchar(1) NOT NULL default ' ',
  "AAAAYCPN" varchar(1) NOT NULL default ' ',
  "AAAAYCPO" varchar(2) NOT NULL default ' ',
  "AAAAYCPP" varchar(1) NOT NULL default ' ',
  "AAAAYCPQ" varchar(1) NOT NULL default ' ',
  "AAAAYCPR" varchar(40) NOT NULL default ' ',
  "AAABFQWU" varchar(1) NOT NULL default ' ',
  "AAAAZAOT" varchar(1) NOT NULL default ' ',
  "AAABHETP" varchar(1) NOT NULL default ' ',
  "AAAAYCPV" varchar(1) NOT NULL default ' ',
  "AAAAYCPW" varchar(1) NOT NULL default ' ',
  "AAAAYCPX" varchar(1) NOT NULL default ' ',
  "AAAAYCPY" varchar(1) NOT NULL default ' ',
  "AAAAYCPZ" varchar(40) NOT NULL default ' ',
  "AAAAYCQA" varchar(1) NOT NULL default ' ',
  "AAAAYCQB" varchar(1) NOT NULL default ' ',
  "AAAAYCQC" varchar(1) NOT NULL default ' ',
  "AAAAYCQD" varchar(1) NOT NULL default ' ',
  "AAAAYCQE" varchar(1) NOT NULL default ' ',
  "AAAAYCQF" varchar(40) NOT NULL default ' ',
  "AAAAYCQG" varchar(1) NOT NULL default ' ',
  "AAAAYCQH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCRG" varchar(2) NOT NULL default ' ',
  "AAAAYCQO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYCRG" varchar(2) NOT NULL default ' ',
  "AAAAYCRH" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCRB" varchar(1) NOT NULL default ' ',
  "AAAAYCRC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDMC","AAAAYDMD","AAAAYCRG","AAAAYCRH","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHETP" varchar(1) NOT NULL default ' ',
  "AAAAYCRY" varchar(1) NOT NULL default ' ',
  "AAAAYCRZ" varchar(10) NOT NULL default ' ',
  "AAAAYCRP" varchar(1) NOT NULL default ' ',
  "AAAAYCRQ" varchar(1) NOT NULL default ' ',
  "AAAAYCRR" varchar(1) NOT NULL default ' ',
  "AAAAYCRS" varchar(1) NOT NULL default ' ',
  "AAAAYCRT" varchar(1) NOT NULL default ' ',
  "AAAAYCRU" varchar(1) NOT NULL default ' ',
  "AAAAYCRV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHETP","AAAAYCRY","AAAAYCRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCTN" varchar(4) NOT NULL default ' ',
  "AAAAYCTE" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCSG" varchar(1) NOT NULL default ' ',
  "AAAAYCSH" varchar(2) NOT NULL default ' ',
  "AAAAYCSI" varchar(2) NOT NULL default ' ',
  "AAAAYCSJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYCSK" varchar(1) NOT NULL default ' ',
  "AAAAYCSL" varchar(1) NOT NULL default ' ',
  "AAAAYCSM" varchar(3) NOT NULL default '000',
  "AAAAYCSN" decimal(5,2) NOT NULL default '0.00',
  "AAAAYCSO" decimal(5,2) NOT NULL default '0.00',
  "AAAAYCSP" decimal(9,2) NOT NULL default '0.00',
  "AAAAYCSQ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYCSR" varchar(1) NOT NULL default ' ',
  "AAAAYCSS" varchar(2) NOT NULL default ' ',
  "AAAAYCST" varchar(2) NOT NULL default ' ',
  "AAAAYCSU" decimal(5,2) NOT NULL default '0.00',
  "AAAAYCSV" varchar(1) NOT NULL default ' ',
  "AAAAYCSW" varchar(1) NOT NULL default ' ',
  "AAAAYCSX" varchar(3) NOT NULL default '000',
  "AAAAYCSY" decimal(5,2) NOT NULL default '0.00',
  "AAAAYCSZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYCTA" decimal(9,2) NOT NULL default '0.00',
  "AAAAYCTB" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYCTN","AAAAYCTE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYCTN" varchar(4) NOT NULL default ' ',
  "AAAAYCTK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYCTN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCTY" varchar(4) NOT NULL default ' ',
  "AAABFXEI" varchar(4) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYCWM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCTW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYCTY","AAABFXEI","AAAAYDIX","AAAAYCWM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXEI" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCUI" varchar(2) NOT NULL default '00',
  "AAAAYCUJ" varchar(2) NOT NULL default '00',
  "AAAAYCUK" varchar(2) NOT NULL default '00',
  "AAAAYCVI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXEI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXEI" varchar(4) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCVV" varchar(2) NOT NULL default '00',
  "AAAAYCVW" varchar(2) NOT NULL default '00',
  "AAAAYCVX" varchar(2) NOT NULL default '00',
  "AAAAYCVY" varchar(2) NOT NULL default '00',
  "AAAAYCVZ" varchar(1) NOT NULL default ' ',
  "AAAAYCWA" varchar(2) NOT NULL default ' ',
  "AAAAYDAC" varchar(2) NOT NULL default ' ',
  "AAAAYCWC" varchar(1) NOT NULL default ' ',
  "AAAAYCWD" varchar(1) NOT NULL default ' ',
  "AAABANVG" varchar(1) NOT NULL default ' ',
  "AAAAYCWF" varchar(1) NOT NULL default ' ',
  "AAABEBCS" varchar(1) NOT NULL default ' ',
  "AAAAYCVH" varchar(1) NOT NULL default ' ',
  "AAAAYCVI" varchar(1) NOT NULL default ' ',
  "AAABJGWQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABFXEI","AAAAYDIX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXEI" varchar(4) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYCWM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCVV" varchar(2) NOT NULL default '00',
  "AAAAYCVW" varchar(2) NOT NULL default '00',
  "AAAAYCVX" varchar(2) NOT NULL default '00',
  "AAAAYCVY" varchar(2) NOT NULL default '00',
  "AAAAYCVZ" varchar(1) NOT NULL default ' ',
  "AAAAYCWA" varchar(2) NOT NULL default ' ',
  "AAAAYDAC" varchar(2) NOT NULL default ' ',
  "AAAAYCWC" varchar(1) NOT NULL default ' ',
  "AAAAYCWD" varchar(1) NOT NULL default ' ',
  "AAABANVG" varchar(1) NOT NULL default ' ',
  "AAAAYCWF" varchar(1) NOT NULL default ' ',
  "AAABEBCS" varchar(1) NOT NULL default ' ',
  "AAAAYCWH" varchar(1) NOT NULL default ' ',
  "AAAAYCWI" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABFXEI","AAAAYDIX","AAAAYCWM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCXE" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKOEQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYCWW" varchar(1) NOT NULL default ' ',
  "AAAAYCWX" varchar(4) NOT NULL default ' ',
  "AAAAYCWY" varchar(3) NOT NULL default '000',
  "AAAAYCWZ" decimal(9,2) NOT NULL default '0.00',
  "AAABLOZV" varchar(1) NOT NULL default ' ',
  "AAAAYCZZ" varchar(6) NOT NULL default '000000',
  "AAAAYCXC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYCXE","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYDME" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDMC","AAAAYDMD","AAAAYDME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYDME" varchar(2) NOT NULL default ' ',
  "AAAAYCXW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDMC","AAAAYDMD","AAAAYDME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYCYS" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYCYS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYCYS" varchar(2) NOT NULL default ' ',
  "AAAAYCYP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYCYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAABBNQD" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCYZ" varchar(4) NOT NULL default ' ',
  "AAAAYCZA" varchar(4) NOT NULL default ' ',
  "AAAAYCZB" varchar(4) NOT NULL default ' ',
  "AAABAPFX" varchar(4) NOT NULL default ' ',
  "AAAAYCZD" varchar(1) NOT NULL default ' ',
  "AAABDWBA" varchar(1) NOT NULL default ' ',
  "AAAAYCZF" varchar(2) NOT NULL default '00',
  "AAAAYCZG" decimal(3,0) NOT NULL default '0',
  "AAABIGYV" varchar(1) NOT NULL default ' ',
  "AAAAYCZI" varchar(1) NOT NULL default ' ',
  "AAAAYCZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDIX","AAABBNQD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYCZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDAC" varchar(2) NOT NULL default ' ',
  "AAABAXDP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYCZU" varchar(2) NOT NULL default '00',
  "AAAAYCZV" varchar(4) NOT NULL default ' ',
  "AAAAYCZW" varchar(4) NOT NULL default ' ',
  "AAAAYCZX" varchar(1) NOT NULL default ' ',
  "AAAAYCZY" varchar(1) NOT NULL default ' ',
  "AAAAYCZZ" varchar(6) NOT NULL default '000000',
  "AAAAYDAA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDAC","AAABAXDP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHFFR" varchar(2) NOT NULL default ' ',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  "AAABGCHL" varchar(1) NOT NULL default ' ',
  "AAABFRHR" varchar(8) NOT NULL default ' ',
  "AAAAYDBC" varchar(20) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAAAYDAN" varchar(40) NOT NULL default ' ',
  "AAAAYDAO" varchar(10) NOT NULL default ' ',
  "AAABBLIL" varchar(10) NOT NULL default ' ',
  "AAABBKHW" varchar(40) NOT NULL default ' ',
  "AAABGCHK" varchar(40) NOT NULL default ' ',
  "AAAAYDAS" varchar(40) NOT NULL default ' ',
  "AAABBMCQ" varchar(2) NOT NULL default ' ',
  "AAAAYDAU" varchar(40) NOT NULL default ' ',
  "AAAAYDAV" varchar(40) NOT NULL default ' ',
  "AAAAYXZJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHFFR","AAABBNBC","AAABGCHL","AAABFRHR","AAAAYDBC","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDBE" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAAAYDBH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLUAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDBY" varchar(1) NOT NULL default ' ',
  "AAABHFFR" varchar(2) NOT NULL default ' ',
  "AAABGCHL" varchar(1) NOT NULL default ' ',
  "AAABLOQJ" varchar(8) NOT NULL default ' ',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  "AAAAYDBR" varchar(1) NOT NULL default ' ',
  "AAAAYDBS" varchar(12) NOT NULL default ' ',
  "AAAAYDBT" varchar(1) NOT NULL default ' ',
  "AAAAYDBU" varchar(1) NOT NULL default ' ',
  "AAAAYDBV" varchar(4) NOT NULL default ' ',
  "AAAAYDBW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDBY","AAABHFFR","AAABGCHL","AAABLOQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDCS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDCH" varchar(1) NOT NULL default ' ',
  "AAAAYDCI" varchar(6) NOT NULL default '000000',
  "AAAAYDCJ" varchar(4) NOT NULL default ' ',
  "AAAAYDCK" varchar(4) NOT NULL default ' ',
  "AAAAYDCL" varchar(4) NOT NULL default ' ',
  "AAAAYDCM" varchar(4) NOT NULL default ' ',
  "AAAAYDJP" varchar(2) NOT NULL default ' ',
  "AAAAYDJK" varchar(10) NOT NULL default ' ',
  "AAAAYDCP" varchar(1) NOT NULL default ' ',
  "AAAAYDCQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYDCS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDCU" (
  "AAABLOZE" varchar(10) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAYVJG" varchar(2) NOT NULL default ' ',
  "AAAAZCJG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOZE","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDDY" varchar(4) NOT NULL default ' ',
  "AAAAYDDO" varchar(2) NOT NULL default '00',
  "AAAAYDDP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDDK" decimal(6,4) NOT NULL default '0.0000',
  "AAABHYAO" decimal(8,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAAAYDDY","AAAAYDDO","AAAAYDDP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDDY" varchar(4) NOT NULL default ' ',
  "AAAAYDDV" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDDY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWRL" varchar(4) NOT NULL default ' ',
  "AAAAYDET" varchar(2) NOT NULL default ' ',
  "AAAAYDEU" varchar(1) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDEH" varchar(1) NOT NULL default ' ',
  "AAAAYDEI" varchar(5) NOT NULL default ' ',
  "AAAAYRBZ" varchar(2) NOT NULL default ' ',
  "AAAAYDEK" varchar(10) NOT NULL default ' ',
  "AAAAYDEL" varchar(10) NOT NULL default ' ',
  "AAAAYDEM" varchar(1) NOT NULL default ' ',
  "AAAAYDEN" varchar(4) NOT NULL default ' ',
  "AAAAYDEO" decimal(5,2) NOT NULL default '0.00',
  "AAAAYDEP" varchar(1) NOT NULL default ' ',
  "AAABCHDZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWRL","AAAAYDET","AAAAYDEU","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABKWRL" varchar(4) NOT NULL default ' ',
  "AAAAYDFB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABKWRL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYDFX" varchar(10) NOT NULL default ' ',
  "AAAAYDFY" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHCJ" varchar(4) NOT NULL default ' ',
  "AAAAYDFO" varchar(1) NOT NULL default ' ',
  "AAAAYDFP" varchar(1) NOT NULL default ' ',
  "AAAAYDFQ" varchar(1) NOT NULL default ' ',
  "AAAAYDFR" varchar(1) NOT NULL default ' ',
  "AAAAYDFS" varchar(1) NOT NULL default ' ',
  "AAAAYDFT" varchar(1) NOT NULL default ' ',
  "AAABIGYV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDIX","AAAAYDFX","AAAAYDFY","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFXEI" varchar(4) NOT NULL default ' ',
  "AAAAYDGO" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDGH" varchar(2) NOT NULL default ' ',
  "AAAAYDGI" varchar(1) NOT NULL default ' ',
  "AAAAYDGJ" varchar(3) NOT NULL default '000',
  "AAAAYDGK" varchar(5) NOT NULL default ' ',
  "AAAAYDGL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFXEI","AAAAYDGO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDGY" varchar(4) NOT NULL default '0000',
  "AAAAYDGZ" varchar(3) NOT NULL default '000',
  "AAAAYDGU" varchar(4) NOT NULL default '0000',
  "AAAAYDGV" varchar(4) NOT NULL default ' ',
  "AAAAYDGW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDGY","AAAAYDGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDHA" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBMWL" varchar(2) NOT NULL default ' ',
  "AAABLSIN" varchar(6) NOT NULL default ' ',
  "AAAAYDHE" varchar(100) NOT NULL default ' ',
  "AAAAYDHF" varchar(3) NOT NULL default '000',
  "AAAAYDHG" varchar(1) NOT NULL default ' ',
  "AAAAYDHH" varchar(1) NOT NULL default ' ',
  "AAAAYDHI" varchar(1) NOT NULL default ' ',
  "AAAAYDHJ" varchar(1) NOT NULL default ' ',
  "AAAAYDHK" varchar(1) NOT NULL default '0',
  "AAAAYDHL" varchar(1) NOT NULL default '0',
  "AAAAYDHM" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLSCE","AAABBMWL","AAABLSIN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDIG" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDHV" varchar(1) NOT NULL default ' ',
  "AAAAYDHW" varchar(1) NOT NULL default ' ',
  "AAAAYDHX" varchar(1) NOT NULL default ' ',
  "AAAAYDHY" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDHZ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDIA" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDIB" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDIC" decimal(5,2) NOT NULL default '0.00',
  "AAAAYDID" decimal(5,2) NOT NULL default '0.00',
  "AAAAYDIE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDIG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYDIY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDIX","AAAAYDIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDIX" varchar(4) NOT NULL default ' ',
  "AAAAYDIY" varchar(1) NOT NULL default ' ',
  "AAAAYDIU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDIX","AAAAYDIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDJP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDJF" varchar(1) NOT NULL default ' ',
  "AAAAYDJG" varchar(1) NOT NULL default ' ',
  "AAAAYDJH" varchar(1) NOT NULL default ' ',
  "AAAAYDJI" varchar(1) NOT NULL default ' ',
  "AAAAYDJJ" varchar(1) NOT NULL default ' ',
  "AAAAYDJK" varchar(10) NOT NULL default ' ',
  "AAAAYDJL" varchar(1) NOT NULL default ' ',
  "AAAAYDJM" varchar(4) NOT NULL default ' ',
  "AAAAZLJW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYDJP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDKP" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDJY" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDJZ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDKA" decimal(4,0) NOT NULL default '0',
  "AAABASKY" varchar(2) NOT NULL default ' ',
  "AAAAYDKC" decimal(5,2) NOT NULL default '0.00',
  "AAAAYDKD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDKP","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDKP" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDRVP" varchar(1) NOT NULL default ' ',
  "AAABJHGR" varchar(2) NOT NULL default ' ',
  "AAABFPGJ" varchar(30) NOT NULL default ' ',
  "AAAAYDKV" varchar(1) NOT NULL default ' ',
  "AAAAYDKW" varchar(1) NOT NULL default ' ',
  "AAAAYDKX" varchar(1) NOT NULL default ' ',
  "AAAAYDKY" varchar(1) NOT NULL default ' ',
  "AAAAYDKZ" varchar(5) NOT NULL default ' ',
  "AAAAYDLA" varchar(2) NOT NULL default ' ',
  "AAAAYDLB" varchar(2) NOT NULL default ' ',
  "AAAAYDLC" varchar(1) NOT NULL default ' ',
  "AAAAYDLD" varchar(30) NOT NULL default ' ',
  "AAAAYDLE" varchar(30) NOT NULL default ' ',
  "AAAAYDLF" varchar(1) NOT NULL default ' ',
  "AAAAYDLG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDRVP","AAABJHGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDLK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDMC" varchar(1) NOT NULL default ' ',
  "AAAAYDMD" varchar(4) NOT NULL default ' ',
  "AAAAYDME" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDLR" varchar(1) NOT NULL default ' ',
  "AAAAYDLS" decimal(5,2) NOT NULL default '0.00',
  "AAAAYDLT" varchar(2) NOT NULL default ' ',
  "AAAAYDLU" varchar(2) NOT NULL default ' ',
  "AAAAYDLV" varchar(10) NOT NULL default ' ',
  "AAAAYDLW" varchar(10) NOT NULL default ' ',
  "AAAAYDLX" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDLY" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDLZ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDMA" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYDMC","AAAAYDMD","AAAAYDME","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYFHS" varchar(11) NOT NULL default '00000000000',
  "AAAAYDRF" varchar(3) NOT NULL default '000',
  "AAAAYDPT" varchar(3) NOT NULL default ' ',
  "AAAAYDNJ" varchar(9) NOT NULL default ' ',
  "AAAAYDYE" varchar(2) NOT NULL default '00',
  "AAAAYDOS" varchar(6) NOT NULL default ' ',
  "AAAAYDPW" varchar(3) NOT NULL default ' ',
  "AAAAYEOY" varchar(2) NOT NULL default '00',
  "AAAAYDMS" varchar(11) NOT NULL default '00000000000',
  "AAAAYDMT" varchar(11) NOT NULL default '00000000000',
  "AAAAYDMU" varchar(11) NOT NULL default '00000000000',
  "AAABIFGN" varchar(4) NOT NULL default ' ',
  "AAAAYNIS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHOA" varchar(4) NOT NULL default ' ',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAAAYDNE" varchar(6) NOT NULL default ' ',
  "AAABEGXI" varchar(15) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAYEUM" varchar(2) NOT NULL default ' ',
  "AAAAYEUN" varchar(3) NOT NULL default ' ',
  "AAAAYDNJ" varchar(9) NOT NULL default ' ',
  "AAAAYDNK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHOA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABARMH" varchar(1) NOT NULL default ' ',
  "AAAAYDPW" varchar(3) NOT NULL default ' ',
  "AAABILRJ" varchar(32) NOT NULL default ' ',
  "AAAAYDTX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABARMH","AAAAYDPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFHS" varchar(11) NOT NULL default '00000000000',
  "AAAAYDRF" varchar(3) NOT NULL default '000',
  "AAAAYESF" varchar(1) NOT NULL default ' ',
  "AAABILRJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFHS","AAAAYDRF","AAAAYESF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEEG" varchar(1) NOT NULL default ' ',
  "AAAAYDOS" varchar(6) NOT NULL default ' ',
  "AAABCUWM" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAAAYENH" varchar(15) NOT NULL default ' ',
  "AAABEGXK" varchar(15) NOT NULL default ' ',
  "AAABEGXI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYEEG","AAAAYDOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFHS" varchar(11) NOT NULL default '00000000000',
  "AAAAYDPT" varchar(3) NOT NULL default '000',
  "AAAAYDPU" varchar(1) NOT NULL default ' ',
  "AAAAYDPV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFHS" varchar(11) NOT NULL default '00000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAYDPH" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDPI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDPJ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYDPK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFHS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFHS" varchar(11) NOT NULL default '00000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDPT" varchar(3) NOT NULL default '000',
  "AAAAYDPU" varchar(1) NOT NULL default ' ',
  "AAAAYDPV" varchar(1) NOT NULL default ' ',
  "AAAAYDPW" varchar(3) NOT NULL default ' ',
  "AAAAYDPX" varchar(1) NOT NULL default ' ',
  "AAAAYDPY" varchar(1) NOT NULL default ' ',
  "AAAAYDPZ" varchar(1) NOT NULL default ' ',
  "AAAAYDQA" varchar(20) NOT NULL default ' ',
  "AAAAYFLP" varchar(3) NOT NULL default '000',
  "AAAAYDQC" varchar(11) NOT NULL default '00000000000',
  PRIMARY KEY ("AAABMRIL","AAAAYFHS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDUL" varchar(7) NOT NULL default '0000000',
  "AAAAYDQO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDQU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIFGN" varchar(4) NOT NULL default ' ',
  "AAAAYDQX" varchar(11) NOT NULL default '00000000000',
  PRIMARY KEY ("AAABMRIL","AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFHS" varchar(11) NOT NULL default '00000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDRF" varchar(3) NOT NULL default '000',
  "AAABIFGN" varchar(4) NOT NULL default ' ',
  "AAAAYDRH" varchar(2) NOT NULL default '00',
  "AAABKUVE" varchar(2) NOT NULL default '00',
  "AAAAYDRJ" varchar(2) NOT NULL default ' ',
  "AAAAYDRK" varchar(1) NOT NULL default ' ',
  "AAAAYDRL" varchar(1) NOT NULL default ' ',
  "AAAAYDRM" varchar(55) NOT NULL default ' ',
  "AAAAYDRN" varchar(34) NOT NULL default ' ',
  "AAAAYDRO" varchar(12) NOT NULL default ' ',
  "AAABIWXH" varchar(12) NOT NULL default ' ',
  "AAAAYDRQ" varchar(32) NOT NULL default ' ',
  "AAAAYDRR" varchar(5) NOT NULL default '00000',
  "AAAAYDRS" varchar(32) NOT NULL default ' ',
  "AAAAYDRT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFHS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABIFGN" varchar(4) NOT NULL default ' ',
  "AAABFOVQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABIFGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNIS" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFDV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYNIS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYDZG" varchar(3) NOT NULL default ' ',
  "AAAAYGKO" varchar(1) NOT NULL default ' ',
  "AAABILRJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYDZG","AAAAYGKO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAAAYESF" varchar(1) NOT NULL default ' ',
  "AAAAYDTD" varchar(1) NOT NULL default ' ',
  "AAAAYDTE" varchar(1) NOT NULL default ' ',
  "AAAAYDTS" varchar(3) NOT NULL default ' ',
  "AAAAYDTT" varchar(2) NOT NULL default '00',
  "AAABKRCX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX","AAAAYESF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDTS" varchar(3) NOT NULL default ' ',
  "AAAAYDTT" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDTS","AAAAYDTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDTU" (
  "AAAAYDTX" varchar(3) NOT NULL default ' ',
  "AAABILRJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYDTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAYDUB" varchar(3) NOT NULL default ' ',
  "AAAAYDUC" varchar(3) NOT NULL default ' ',
  "AAAAYDUD" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDUL" varchar(7) NOT NULL default '0000000',
  "AAABBARY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDUL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDUM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDUS" varchar(1) NOT NULL default ' ',
  "AAAAYDUT" varchar(1) NOT NULL default ' ',
  "AAAAYDUU" varchar(1) NOT NULL default ' ',
  "AAAAYDUV" varchar(1) NOT NULL default ' ',
  "AAAAYDUW" varchar(1) NOT NULL default ' ',
  "AAAAYDUX" varchar(1) NOT NULL default ' ',
  "AAAAYDUY" varchar(1) NOT NULL default ' ',
  "AAAAYDUZ" varchar(1) NOT NULL default ' ',
  "AAAAYEYY" decimal(5,2) NOT NULL default '0.00',
  "AAAAYDVB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEOB" varchar(2) NOT NULL default '00',
  "AAAAYDVJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYEOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDVR" varchar(2) NOT NULL default ' ',
  "AAAAYDVP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDVR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYECI" varchar(2) NOT NULL default '00',
  "AAABILRJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYECI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEZP" varchar(2) NOT NULL default '00',
  "AAAAYDXQ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYDXS" varchar(9) NOT NULL default '000000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDWH" varchar(2) NOT NULL default '00',
  "AAAAYDWI" varchar(2) NOT NULL default '00',
  "AAAAYDWJ" varchar(2) NOT NULL default '00',
  "AAAAYDWK" varchar(2) NOT NULL default '00',
  "AAAAYDWL" varchar(2) NOT NULL default '00',
  "AAAAYDWM" varchar(2) NOT NULL default '00',
  "AAAAYDWN" varchar(2) NOT NULL default '00',
  "AAAAYDWO" varchar(2) NOT NULL default '00',
  "AAAAYDWP" varchar(2) NOT NULL default '00',
  "AAAAYDWQ" varchar(2) NOT NULL default '00',
  "AAAAYDWR" varchar(2) NOT NULL default '00',
  "AAAAYDWS" varchar(2) NOT NULL default '00',
  "AAAAYDWT" varchar(2) NOT NULL default '00',
  "AAAAYDWU" varchar(2) NOT NULL default '00',
  "AAAAYDWV" varchar(2) NOT NULL default '00',
  "AAAAYDWW" varchar(2) NOT NULL default '00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAYDWY" varchar(2) NOT NULL default '00',
  "AAAAYDWZ" varchar(2) NOT NULL default '00',
  "AAAAYDXA" varchar(2) NOT NULL default '00',
  "AAAAYDXB" varchar(2) NOT NULL default '00',
  "AAAAYDXC" varchar(2) NOT NULL default '00',
  "AAAAYDXD" varchar(2) NOT NULL default '00',
  "AAAAYDXE" varchar(2) NOT NULL default '00',
  "AAAAYDXF" varchar(2) NOT NULL default '00',
  "AAAAYDXG" varchar(2) NOT NULL default '00',
  "AAAAYDXH" varchar(2) NOT NULL default '00',
  "AAAAYDXI" varchar(2) NOT NULL default '00',
  "AAAAYDXJ" varchar(2) NOT NULL default '00',
  "AAAAYDXK" varchar(2) NOT NULL default '00',
  "AAAAYDXL" varchar(2) NOT NULL default '00',
  "AAAAYDXM" varchar(2) NOT NULL default '00',
  "AAAAYDXN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYEZP","AAAAYDXQ","AAABKFCZ","AAAAYDXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDYE" varchar(2) NOT NULL default '00',
  "AAAAYDYF" varchar(1) NOT NULL default ' ',
  "AAAAYFBH" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDYA" varchar(2) NOT NULL default ' ',
  "AAAAYDYB" varchar(1) NOT NULL default ' ',
  "AAABKZAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDYE","AAAAYDYF","AAAAYFBH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYDZG" varchar(3) NOT NULL default ' ',
  "AAAAYGKO" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYDYO" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYDYP" varchar(3) NOT NULL default '000',
  "AAAAYDYQ" varchar(4) NOT NULL default '0000',
  "AAAAYEIA" varchar(4) NOT NULL default '0000',
  "AAAAYEIB" varchar(8) NOT NULL default '00000000',
  "AAAAYEIC" varchar(4) NOT NULL default '0000',
  "AAAAYEID" varchar(8) NOT NULL default '00000000',
  "AAAAYDYV" decimal(3,0) NOT NULL default '0',
  "AAAAYDYW" varchar(1) NOT NULL default '0',
  "AAAAYEFJ" varchar(1) NOT NULL default ' ',
  "AAAAYEFM" varchar(1) NOT NULL default ' ',
  "AAAAYDYZ" varchar(2) NOT NULL default '00',
  "AAAAYDZA" varchar(3) NOT NULL default ' ',
  "AAAAYDZB" varchar(3) NOT NULL default ' ',
  "AAAAYDZC" varchar(1) NOT NULL default ' ',
  "AAAAYDZD" varchar(1) NOT NULL default ' ',
  "AAAAYDZE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYDZG","AAAAYGKO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYECI" varchar(2) NOT NULL default '00',
  "AAAAYEAH" varchar(1) NOT NULL default '0',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEAC" decimal(9,2) NOT NULL default '0.00',
  "AAAAYEAD" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYECI","AAAAYEAH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYDZW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEAG" varchar(2) NOT NULL default '00',
  "AAAAYEAH" varchar(1) NOT NULL default '0',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEAC" decimal(9,2) NOT NULL default '0.00',
  "AAAAYEAD" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYEAG","AAAAYEAH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEEZ" varchar(2) NOT NULL default '00',
  "AAAAYFLP" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZCYO" decimal(4,2) NOT NULL default '0.00',
  "AAAAYEAQ" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYEEZ","AAAAYFLP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEMZ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEBA" decimal(5,3) NOT NULL default '0.000',
  "AAAAYEBB" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYEMZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEBF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEOY" varchar(2) NOT NULL default '00',
  "AAAAYECI" varchar(2) NOT NULL default '00',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYEOY","AAAAYECI","AAABJGBU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYEEG" varchar(1) NOT NULL default ' ',
  "AAABILRJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYEEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEBZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYECI" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYECE" decimal(9,2) NOT NULL default '0.00',
  "AAAAYECF" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYECI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYECK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYECR" decimal(9,2) NOT NULL default '0.00',
  "AAAAYECS" decimal(9,2) NOT NULL default '0.00',
  "AAAAYECT" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYECZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEDQ" varchar(2) NOT NULL default ' ',
  "AAAAYEDC" varchar(1) NOT NULL default ' ',
  "AAAAYEDD" varchar(1) NOT NULL default ' ',
  "AAAAYEDE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYEDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEDQ" varchar(2) NOT NULL default ' ',
  "AAAAYEDR" varchar(2) NOT NULL default ' ',
  "AAAAZEGG" decimal(9,2) NOT NULL default '0.00',
  "AAAAYEDT" decimal(5,3) NOT NULL default '0.000',
  "AAABARMH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEEG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYEEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEEP" varchar(4) NOT NULL default ' ',
  "AAABAPRQ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYEEP","AAABAPRQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYEEZ" varchar(2) NOT NULL default '00',
  "AAABILRJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYEEZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPBN" varchar(5) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEFJ" varchar(1) NOT NULL default ' ',
  "AAAAYGKO" varchar(2) NOT NULL default ' ',
  "AAAAYXIA" varchar(4) NOT NULL default ' ',
  "AAAAYEFM" varchar(1) NOT NULL default ' ',
  "AAABIFOV" varchar(4) NOT NULL default ' ',
  "AAAAYEFO" varchar(4) NOT NULL default ' ',
  "AAAAYFKX" varchar(3) NOT NULL default ' ',
  "AAAAYEFQ" varchar(2) NOT NULL default ' ',
  "AAAAYEFR" varchar(4) NOT NULL default ' ',
  "AAAAYEFS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPBN","AAABMCVI","AAABBHJS","AAABBEON","AAABBEOO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPBN" varchar(5) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYESF" varchar(1) NOT NULL default ' ',
  "AAAAYGKN" varchar(3) NOT NULL default '000',
  "AAAAYEHD" varchar(1) NOT NULL default '0',
  "AAAAYEGM" varchar(5) NOT NULL default ' ',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPBN","AAABMCVI","AAABBHJS","AAABBEON","AAABBEOO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFKX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEHB" varchar(3) NOT NULL default '000',
  "AAAAYEHC" varchar(3) NOT NULL default '000',
  "AAAAYEHD" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAYFKX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFKX" varchar(3) NOT NULL default ' ',
  "AAAAYESF" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEHN" decimal(5,2) NOT NULL default '0.00',
  "AAAAYEHO" varchar(1) NOT NULL default '0',
  "AAAAYEQQ" decimal(3,0) NOT NULL default '0',
  "AAAAYEHQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAYFKX","AAAAYESF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFKX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEIA" varchar(4) NOT NULL default '0000',
  "AAAAYEIB" varchar(8) NOT NULL default '00000000',
  "AAAAYEIC" varchar(4) NOT NULL default '0000',
  "AAAAYEID" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYFKX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFKX" varchar(3) NOT NULL default ' ',
  "AAAAYESF" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEIN" varchar(5) NOT NULL default ' ',
  "AAABFLSG" varchar(8) NOT NULL default ' ',
  "AAAAYEIP" varchar(5) NOT NULL default ' ',
  "AAAAYHNX" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFKX","AAAAYESF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFKX" varchar(3) NOT NULL default ' ',
  "AAAAYEJF" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZHSJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFKX","AAAAYEJF","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZHSJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZHSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXIA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYXIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZHSJ" varchar(5) NOT NULL default ' ',
  "AAAAYEKJ" varchar(4) NOT NULL default ' ',
  "AAAAYFKY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZHSJ","AAAAYEKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZHSJ" varchar(5) NOT NULL default ' ',
  "AAAAYEKJ" varchar(4) NOT NULL default ' ',
  "AAAAYEKE" varchar(4) NOT NULL default ' ',
  "AAAAYNLU" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYEKG" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAZHSJ","AAAAYEKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFKY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  "AAAAYEKQ" varchar(4) NOT NULL default ' ',
  "AAAAYEKR" varchar(40) NOT NULL default ' ',
  "AAABMPMH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFKY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAPBN" varchar(5) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABAPBN","AAABMCVI","AAABBHJS","AAABBEON","AAABBEOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYELM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYFKX" varchar(3) NOT NULL default ' ',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYFKX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYELU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYXIA" varchar(4) NOT NULL default ' ',
  "AAABILRJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYXIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYFKY" varchar(4) NOT NULL default ' ',
  "AAABILRJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYFKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZHSJ" varchar(5) NOT NULL default ' ',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZHSJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYEMZ" varchar(3) NOT NULL default '000',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYEMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYENA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYENT" varchar(10) NOT NULL default '0000000000',
  "AAAAYEND" varchar(1) NOT NULL default ' ',
  "AAABCUWM" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAAAYENH" varchar(15) NOT NULL default ' ',
  "AAABEGXK" varchar(15) NOT NULL default ' ',
  "AAABEGXI" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYENT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYENM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYENT" varchar(10) NOT NULL default '0000000000',
  "AAABILRJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYENT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYENU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYEOB" varchar(2) NOT NULL default '00',
  "AAAAYENY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYEOB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEOC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYEOY" varchar(2) NOT NULL default '00',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  "AAABKZAW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYEOY","AAABJGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEOY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYEOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYEOY" varchar(2) NOT NULL default '00',
  "AAABKZAW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYEOY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYEPD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEPV" varchar(8) NOT NULL default ' ',
  "AAAAYESE" varchar(2) NOT NULL default '00',
  "AAAAYERV" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAYEPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYEPV" varchar(8) NOT NULL default ' ',
  "AAAAYEPS" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYEPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYESE" varchar(2) NOT NULL default '00',
  "AAAAYEPZ" varchar(1) NOT NULL default ' ',
  "AAAAYEQA" varchar(1) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYESE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYESE" varchar(2) NOT NULL default '00',
  "AAAAYEQI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYESE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYESB" varchar(2) NOT NULL default ' ',
  "AAAAYEQP" varchar(2) NOT NULL default '00',
  "AAAAYEQQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAYESB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYESB" varchar(2) NOT NULL default ' ',
  "AAABHFFQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYESB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYERB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYERV" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAYERV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYERG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYERV" varchar(3) NOT NULL default '000',
  "AAAAYERK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYERV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYERO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYERV" varchar(3) NOT NULL default '000',
  "AAAAYESB" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYERT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYERV","AAAAYESB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYERX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYESE" varchar(2) NOT NULL default '00',
  "AAAAYESF" varchar(1) NOT NULL default ' ',
  "AAAAYESB" varchar(2) NOT NULL default ' ',
  "AAABAGKE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYESE","AAAAYESF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYESG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYESM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYESR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQBS" varchar(4) NOT NULL default ' ',
  "AAABILTS" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYESX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYETI" varchar(2) NOT NULL default ' ',
  "AAAAYETJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYETD" varchar(40) NOT NULL default ' ',
  "AAAAYETE" varchar(5) NOT NULL default ' ',
  "AAAAYETF" varchar(9) NOT NULL default ' ',
  "AAAAYETG" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYETI","AAAAYETJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYETL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZTYI" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYETX" varchar(1) NOT NULL default '0',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYETR" varchar(3) NOT NULL default '000',
  "AAAAYETS" varchar(1) NOT NULL default ' ',
  "AAAAYETT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZTYI","AAABKFCZ","AAAAYETX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYETY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEUM" varchar(2) NOT NULL default ' ',
  "AAAAYEUN" varchar(3) NOT NULL default ' ',
  "AAABMJVD" varchar(20) NOT NULL default ' ',
  "AAABMJVE" varchar(20) NOT NULL default ' ',
  "AAAAYFOQ" varchar(2) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABBCDL" varchar(6) NOT NULL default '000000',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABEGXI" varchar(15) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYEUM","AAAAYEUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEUX" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYEUZ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKOEQ" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAYEUX","AAABHVYL","AAAAYEUZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCJL" varchar(6) NOT NULL default ' ',
  "AAABBCJM" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBCJH" varchar(1) NOT NULL default ' ',
  "AAAAYTKK" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBCJL","AAABBCJM","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTMJ" varchar(6) NOT NULL default ' ',
  "AAAAYTKS" varchar(1) NOT NULL default ' ',
  "AAAAYTKT" varchar(1) NOT NULL default ' ',
  "AAAAYTKU" varchar(1) NOT NULL default ' ',
  "AAAAYTKV" varchar(1) NOT NULL default ' ',
  "AAAAYTKW" varchar(20) NOT NULL default ' ',
  "AAAAYTKX" varchar(20) NOT NULL default ' ',
  "AAAAYTKY" varchar(20) NOT NULL default ' ',
  "AAAAYTKZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYTMJ" varchar(6) NOT NULL default ' ',
  "AAAAZAZL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYTMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTMJ" varchar(6) NOT NULL default ' ',
  "AAAAYTLW" varchar(2) NOT NULL default '00',
  "AAAAYTLO" varchar(1) NOT NULL default ' ',
  "AAAAYTLP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYTLR" varchar(2) NOT NULL default ' ',
  "AAAAYTLS" decimal(9,2) NOT NULL default '0.00',
  "AAAAYTLT" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTMJ","AAAAYTLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTMM" varchar(6) NOT NULL default ' ',
  "AAAAYTMA" varchar(1) NOT NULL default ' ',
  "AAAAYTMB" varchar(1) NOT NULL default ' ',
  "AAAAYTMC" decimal(5,2) NOT NULL default '0.00',
  "AAAAYTMD" varchar(1) NOT NULL default ' ',
  "AAAAYTME" decimal(9,2) NOT NULL default '0.00',
  "AAAAYTMF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYTMG" varchar(2) NOT NULL default '00',
  "AAAAYTMH" varchar(1) NOT NULL default ' ',
  "AAAAYTMI" varchar(1) NOT NULL default ' ',
  "AAAAYTMJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTMV" varchar(6) NOT NULL default ' ',
  "AAAAYXIN" varchar(3) NOT NULL default '000',
  "AAAAYTMR" varchar(1) NOT NULL default ' ',
  "AAABAEST" varchar(1) NOT NULL default ' ',
  "AAAAYTMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCJT" varchar(2) NOT NULL default ' ',
  "AAABBCJR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQNV" varchar(4) NOT NULL default ' ',
  "AAABBCJA" varchar(1) NOT NULL default ' ',
  "AAAAYTNQ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCJB" varchar(2) NOT NULL default ' ',
  "AAAAYTNK" decimal(9,2) NOT NULL default '0.00',
  "AAAAYTNL" varchar(1) NOT NULL default ' ',
  "AAAAYTNM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQNV","AAABBCJA","AAAAYTNQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFDV" varchar(2) NOT NULL default '00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEYV" decimal(9,2) NOT NULL default '0.00',
  "AAAAYEYW" decimal(9,2) NOT NULL default '0.00',
  "AAAAYEYX" decimal(9,2) NOT NULL default '0.00',
  "AAAAYEYY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYFDV","AAABMPWL","AAABKFCZ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYEZP" varchar(2) NOT NULL default '00',
  "AAABGCYS" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYEZL" decimal(9,2) NOT NULL default '0.00',
  "AAAAYEZM" decimal(9,2) NOT NULL default '0.00',
  "AAAAYEZN" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYEZP","AAABGCYS","AAABMPWL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYEZT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFDV" varchar(2) NOT NULL default '00',
  "AAABBAEF" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLXZQ" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYFDV","AAABBAEF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFDV" varchar(2) NOT NULL default '00',
  "AAABBAEF" varchar(6) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYFDV","AAABBAEF","AAABMPWL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFDV" varchar(2) NOT NULL default '00',
  "AAAAYFDB" varchar(1) NOT NULL default ' ',
  "AAAAYFBH" varchar(2) NOT NULL default '00',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYFBJ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGPY" varchar(1) NOT NULL default ' ',
  "AAAAYFBB" varchar(1) NOT NULL default ' ',
  "AAAAYFBC" decimal(5,2) NOT NULL default '0.00',
  "AAAAYFBD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFDV","AAAAYFDB","AAAAYFBH","AAABHVYL","AAAAYFBJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAXBU" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAAAYFCC" varchar(5) NOT NULL default ' ',
  "AAAAYFCA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAXBU","AAABBGCX","AAABAWGV","AAAAYFCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFCC" varchar(5) NOT NULL default ' ',
  "AAAAYFCA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFCD" (
  "AAAAYFDV" varchar(2) NOT NULL default '00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFCJ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYFCK" decimal(5,2) NOT NULL default '0.00',
  "AAAAYFCL" decimal(5,2) NOT NULL default '0.00',
  "AAAAYFCM" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYFDV","AAABMPWL","AAABKFCZ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFDV" varchar(2) NOT NULL default '00',
  "AAAAYFDB" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFCX" varchar(1) NOT NULL default ' ',
  "AAABKZAW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFDV","AAAAYFDB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYFDV" varchar(2) NOT NULL default '00',
  "AAABBAEF" varchar(6) NOT NULL default ' ',
  "AAABILRJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYFDV","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYFDV" varchar(2) NOT NULL default '00',
  "AAABBAEF" varchar(6) NOT NULL default ' ',
  "AAABILRJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYFDV","AAABBAEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAAAYFEX" varchar(10) NOT NULL default '0000000000',
  "AAAAYFFA" varchar(1) NOT NULL default ' ',
  "AAAAYFFJ" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(20) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABIITH" varchar(8) NOT NULL default '00000000',
  "AAABIITI" varchar(6) NOT NULL default '000000',
  "AAAAYFEH" varchar(1) NOT NULL default ' ',
  "AAAAYFEI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMYU","AAAAYFEX","AAAAYFFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFFJ" varchar(4) NOT NULL default '0000',
  "AAAAYFKL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMYU" varchar(12) NOT NULL default ' ',
  "AAAAYFFA" varchar(1) NOT NULL default ' ',
  "AAAAYFEX" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABKMYU","AAAAYFFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHIOL" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFFH" varchar(30) NOT NULL default ' ',
  "AAAAYFFI" varchar(30) NOT NULL default ' ',
  "AAAAYFFJ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABHIOL","AAABMKXW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYFGE" varchar(2) NOT NULL default ' ',
  "AAAAYFGF" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMMCU","AAABHVYP","AAAAYFGE","AAAAYFGF","AAABBEOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(4) NOT NULL default ' ',
  "AAABBNBC" varchar(16) NOT NULL default ' ',
  "AAAAYFGM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(4) NOT NULL default ' ',
  "AAABILRJ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLOZE","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFHS" varchar(11) NOT NULL default '00000000000',
  "AAAAYFHT" varchar(8) NOT NULL default ' ',
  "AAAAYFHL" varchar(4) NOT NULL default '0000',
  "AAAAYFHM" varchar(6) NOT NULL default '000000',
  "AAAAYFHN" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYFHP" varchar(1) NOT NULL default ' ',
  "AAAAYFHQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFHS","AAAAYFHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFIA" varchar(8) NOT NULL default ' ',
  "AAAAYFIB" varchar(8) NOT NULL default '00000000',
  "AAAAYFHY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFIA","AAAAYFIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFII" varchar(1) NOT NULL default ' ',
  "AAAAYFIJ" varchar(1) NOT NULL default '0',
  "AAAAYFIK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFJU" varchar(4) NOT NULL default ' ',
  "AAAAYFJN" varchar(3) NOT NULL default ' ',
  "AAAAYFIT" decimal(3,0) NOT NULL default '0',
  "AAAAYFIU" varchar(1) NOT NULL default '0',
  "AAAAYFIV" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFIW" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFIX" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFIY" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFIZ" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJA" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJB" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJC" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJD" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJE" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJF" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJG" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJH" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJI" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJJ" decimal(8,4) NOT NULL default '0.0000',
  "AAAAYFJK" decimal(8,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAYFJU","AAAAYFJN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFKX" varchar(3) NOT NULL default ' ',
  "AAAAYFKY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFJU" varchar(4) NOT NULL default ' ',
  "AAAAYFKN" varchar(5) NOT NULL default '00000',
  "AAAAYFJW" varchar(2) NOT NULL default '00',
  "AAAAYFJX" varchar(4) NOT NULL default ' ',
  "AAAAYFJY" varchar(3) NOT NULL default ' ',
  "AAAAYFJZ" varchar(1) NOT NULL default ' ',
  "AAAAYFKA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFKX","AAAAYFKY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFKF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFKN" varchar(5) NOT NULL default '00000',
  "AAAAYFKI" varchar(2) NOT NULL default '00',
  "AAAAYFKJ" varchar(8) NOT NULL default '00000000',
  "AAAAYFKK" varchar(17) NOT NULL default ' ',
  "AAAAYFKL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYFKX" varchar(3) NOT NULL default ' ',
  "AAAAYFKY" varchar(4) NOT NULL default ' ',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  "AAABLPWW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYFKX","AAAAYFKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFKZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFLP" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYFLP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFLH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYFLP" varchar(3) NOT NULL default '000',
  "AAABILRJ" varchar(60) NOT NULL default ' ',
  "AAABLPWW" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYFLP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFMJ" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYFLV" varchar(1) NOT NULL default ' ',
  "AAAAYFLW" decimal(5,2) NOT NULL default '0.00',
  "AAABDWBA" varchar(2) NOT NULL default '00',
  "AAAAYFLY" varchar(1) NOT NULL default ' ',
  "AAAAYFLZ" decimal(5,2) NOT NULL default '0.00',
  "AAABBDDT" varchar(2) NOT NULL default '00',
  "AAABBDDU" varchar(1) NOT NULL default ' ',
  "AAAAYFMC" varchar(1) NOT NULL default ' ',
  "AAAAYFMD" decimal(5,2) NOT NULL default '0.00',
  "AAABABOS" varchar(2) NOT NULL default '00',
  "AAABABOT" varchar(2) NOT NULL default ' ',
  "AAAAYFMG" varchar(1) NOT NULL default ' ',
  "AAAAYFMH" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYFMJ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
