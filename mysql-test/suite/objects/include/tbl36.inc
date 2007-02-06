# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl36.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAABDZSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAFY" varchar(1) NOT NULL default ' ',
  "AAABDZSU" varchar(1) NOT NULL default ' ',
  "AAABDZSV" varchar(1) NOT NULL default ' ',
  "AAABDZSW" varchar(1) NOT NULL default ' ',
  "AAABDZSX" varchar(1) NOT NULL default ' ',
  "AAABDZSY" varchar(1) NOT NULL default ' ',
  "AAABDZSZ" varchar(1) NOT NULL default ' ',
  "AAABDZTA" varchar(1) NOT NULL default ' ',
  "AAABDZTB" varchar(1) NOT NULL default ' ',
  "AAABDZTC" varchar(1) NOT NULL default ' ',
  "AAABDZTD" varchar(1) NOT NULL default ' ',
  "AAABESGO" varchar(30) NOT NULL default ' ',
  "AAABDZTF" varchar(1) NOT NULL default ' ',
  "AAABDZTG" varchar(1) NOT NULL default ' ',
  "AAABDZTH" varchar(1) NOT NULL default ' ',
  "AAABDZTI" varchar(1) NOT NULL default ' ',
  "AAABDZTJ" varchar(1) NOT NULL default ' ',
  "AAABDZTK" varchar(1) NOT NULL default ' ',
  "AAABDZTL" varchar(1) NOT NULL default ' ',
  "AAABDZTM" varchar(1) NOT NULL default ' ',
  "AAABDZTN" varchar(1) NOT NULL default ' ',
  "AAABDZTO" varchar(1) NOT NULL default ' ',
  "AAABDZTP" varchar(1) NOT NULL default ' ',
  "AAABDZTQ" varchar(1) NOT NULL default ' ',
  "AAABDZTR" varchar(1) NOT NULL default ' ',
  "AAABDZTS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAFD" varchar(10) NOT NULL default ' ',
  "AAABLBDX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABEAFD" varchar(10) NOT NULL default ' ',
  "AAABDZUF" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABEAFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABDZUT" varchar(1) NOT NULL default ' ',
  "AAABDZUO" varchar(1) NOT NULL default ' ',
  "AAABDZUP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXX","AAABESGI","AAABDZUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABDZUY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERXX","AAABESGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGI","AAABESHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGI","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABESGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABESGF" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABDZWY" varchar(4) NOT NULL default ' ',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABLKLI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESGI","AAABDZWY","AAABDZWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESGI" varchar(2) NOT NULL default '00',
  "AAABDZWY" varchar(4) NOT NULL default ' ',
  "AAABDZWZ" varchar(8) NOT NULL default '00000000',
  "AAABDZWU" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESGI","AAABDZWY","AAABDZWZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABDZXW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABDZYF" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABDZXW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABDZYF" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABESHA","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZYW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDZYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABDZYW" varchar(3) NOT NULL default ' ',
  "AAABDZYT" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABDZYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGFYY" varchar(20) NOT NULL default ' ',
  "AAABDZZE" varchar(3) NOT NULL default ' ',
  "AAABDZZB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGFYY","AAABDZZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZZF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABDZZM" varchar(4) NOT NULL default '0000',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABDZZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABDZZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDZZY" varchar(1) NOT NULL default ' ',
  "AAABDZZZ" varchar(30) NOT NULL default ' ',
  "AAABEAAA" varchar(30) NOT NULL default ' ',
  "AAABMAAS" varchar(1) NOT NULL default ' ',
  "AAABDZZT" varchar(30) NOT NULL default ' ',
  "AAABDZZU" varchar(30) NOT NULL default ' ',
  "AAABDZZV" varchar(1) NOT NULL default ' ',
  "AAABKXRS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDZZY","AAABDZZZ","AAABEAAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKMQV" varchar(4) NOT NULL default ' ',
  "AAABERXO" varchar(3) NOT NULL default '000',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABEAAG" varchar(4) NOT NULL default ' ',
  "AAABEAAH" varchar(4) NOT NULL default ' ',
  "AAABEAAI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKMQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABKMQV" varchar(4) NOT NULL default ' ',
  "AAABEAAP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABKMQV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAAV" varchar(30) NOT NULL default ' ',
  "AAABEAAW" varchar(1) NOT NULL default ' ',
  "AAABEAAX" varchar(1) NOT NULL default ' ',
  "AAABEAAY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEABA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABEACI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEABG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEADL" varchar(5) NOT NULL default '00000',
  "AAABEABJ" varchar(30) NOT NULL default ' ',
  "AAABEABK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABEADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEABN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABEADL" varchar(5) NOT NULL default '00000',
  "AAABEABR" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABEADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEABV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEADL" varchar(5) NOT NULL default '00000',
  "AAABEACD" varchar(2) NOT NULL default '00',
  "AAABERXX" varchar(10) NOT NULL default ' ',
  "AAABEACA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEADL","AAABEACD","AAABERXX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEACF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABEACI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEACL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAEL" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABEAEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEACQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABEAEL" varchar(5) NOT NULL default '00000',
  "AAABEACU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABEAEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEACY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAEL" varchar(5) NOT NULL default '00000',
  "AAABEADL" varchar(5) NOT NULL default '00000',
  "AAABEADC" varchar(2) NOT NULL default '00',
  "AAABEADD" varchar(1) NOT NULL default ' ',
  "AAABEADE" varchar(20) NOT NULL default ' ',
  "AAABEADF" varchar(3) NOT NULL default ' ',
  "AAABEADG" varchar(1) NOT NULL default ' ',
  "AAABEADH" varchar(1) NOT NULL default ' ',
  "AAABEADI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAEL","AAABEADL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEADM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABEAFY" varchar(1) NOT NULL default ' ',
  "AAABEADV" varchar(4) NOT NULL default ' ',
  "AAABEAEL" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABEAFY","AAABEADV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEADW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABEAFY" varchar(1) NOT NULL default ' ',
  "AAABLZFC" varchar(4) NOT NULL default ' ',
  "AAABEAEL" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABEAFY","AAABLZFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABEAFY" varchar(1) NOT NULL default ' ',
  "AAABMLHQ" varchar(4) NOT NULL default ' ',
  "AAABEAEL" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABEAFY","AAABMLHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABESGO" varchar(30) NOT NULL default ' ',
  "AAABEAFB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAFD" varchar(10) NOT NULL default ' ',
  "AAABESGO" varchar(30) NOT NULL default ' ',
  "AAABEAFB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYFE" varchar(3) NOT NULL default ' ',
  "AAABEAFH" varchar(1) NOT NULL default ' ',
  "AAABEAFI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABKYFE" varchar(3) NOT NULL default ' ',
  "AAABEAFP" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABKYFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAFY" varchar(1) NOT NULL default ' ',
  "AAABEAFW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAFY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAIZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABEAHF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABIDHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABLZUS","AAABIDHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQII" varchar(4) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABIDHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQII","AAABLZUS","AAABIDHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABEAHF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZFD" varchar(4) NOT NULL default ' ',
  "AAABLZUS" varchar(1) NOT NULL default ' ',
  "AAABIDHQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZFD","AAABLZUS","AAABIDHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABEAIZ" varchar(4) NOT NULL default ' ',
  "AAABEAHV" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABEAIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAIZ" varchar(4) NOT NULL default ' ',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAIZ","AAABESHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAIY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHP" varchar(1) NOT NULL default ' ',
  "AAABEAIY" varchar(4) NOT NULL default ' ',
  "AAABEAIP" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHP","AAABEAIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAIY" varchar(4) NOT NULL default ' ',
  "AAABEAIZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAIY","AAABEAIZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABESHA" varchar(12) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEAJE" varchar(1) NOT NULL default ' ',
  "AAABEAJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABESHA","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAJJ" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEAJL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAJN" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEAJP" varchar(30) NOT NULL default ' ',
  "AAABEAJQ" varchar(30) NOT NULL default ' ',
  "AAABEAJR" varchar(30) NOT NULL default ' ',
  "AAABEAJS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAJU" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEAJW" varchar(30) NOT NULL default ' ',
  "AAABEAJX" varchar(30) NOT NULL default ' ',
  "AAABEAJY" varchar(30) NOT NULL default ' ',
  "AAABEAJZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABEAKX" varchar(3) NOT NULL default ' ',
  "AAABEAKY" varchar(3) NOT NULL default ' ',
  "AAABEAKG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLYRW","AAABEAKX","AAABEAKY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYRW" varchar(6) NOT NULL default ' ',
  "AAABEAKX" varchar(3) NOT NULL default ' ',
  "AAABEAKY" varchar(3) NOT NULL default ' ',
  "AAABEAKZ" varchar(3) NOT NULL default ' ',
  "AAABEALA" varchar(2) NOT NULL default '00',
  "AAABEAKS" varchar(2) NOT NULL default ' ',
  "AAABEAKT" varchar(2) NOT NULL default ' ',
  "AAABEAKU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLYRW","AAABEAKX","AAABEAKY","AAABEAKZ","AAABEALA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEALB" (
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABHEIG" varchar(30) NOT NULL default ' ',
  "AAABEALG" varchar(30) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABLTXI","AAABLEST"),
  KEY "AAABEALR" ("AAABHEIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEALT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAABEALW" varchar(1) NOT NULL default ' ',
  "AAABEANH" varchar(1) NOT NULL default ' ',
  "AAABEANG" varchar(1) NOT NULL default ' ',
  "AAABEALZ" varchar(1) NOT NULL default ' ',
  "AAABJZVA" varchar(4) NOT NULL default ' ',
  "AAABJGGK" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  "AAABEAMJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEAOJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAMZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEAMZ" varchar(3) NOT NULL default ' ',
  "AAABLLLP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEAMZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEANA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAPE" varchar(3) NOT NULL default ' ',
  "AAABEAND" varchar(3) NOT NULL default ' ',
  "AAABEAPC" varchar(1) NOT NULL default ' ',
  "AAABEANF" varchar(1) NOT NULL default ' ',
  "AAABEANG" varchar(1) NOT NULL default ' ',
  "AAABEANH" varchar(1) NOT NULL default ' ',
  "AAABEANI" varchar(1) NOT NULL default ' ',
  "AAABEANJ" varchar(255) NOT NULL default ' ',
  "AAABEANK" varchar(255) NOT NULL default ' ',
  "AAABEANL" varchar(16) NOT NULL default ' ',
  "AAABEANM" varchar(16) NOT NULL default ' ',
  "AAABEANN" varchar(2) NOT NULL default ' ',
  "AAABEANO" varchar(12) NOT NULL default ' ',
  "AAABEANP" varchar(1) NOT NULL default ' ',
  "AAABEANQ" varchar(1) NOT NULL default ' ',
  "AAABLHAH" varchar(255) NOT NULL default ' ',
  "AAABEANS" varchar(3) NOT NULL default ' ',
  "AAABEANT" varchar(32) NOT NULL default ' ',
  "AAABEANU" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEANX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEAPE" varchar(3) NOT NULL default ' ',
  "AAABLLLP" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEAPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLPJD" varchar(10) NOT NULL default ' ',
  "AAABMPMH" varchar(32) NOT NULL default ' ',
  "AAABEAOJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLPJD","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAPE" varchar(3) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAPN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEAPE" varchar(3) NOT NULL default ' ',
  "AAABEAPN" varchar(10) NOT NULL default ' ',
  "AAABEAPC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEAPE","AAABEAPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEAPN" varchar(10) NOT NULL default ' ',
  "AAABLLLP" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEAPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAPO" (
  "AAABLAAA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLAAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAPR" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLAAA" varchar(2) NOT NULL default '00',
  "AAABEAPU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLAAA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAPX" (
  "AAABGAMQ" varchar(11) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABEAQB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABGAMQ","AAABMREL","AAABJEMU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGEXP" varchar(4) NOT NULL default ' ',
  "AAABJGGK" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGEXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJRGC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJRGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABLCCR" varchar(6) NOT NULL default ' ',
  "AAABJRGC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCAK","AAABLCCR","AAABJRGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEARA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJRGC" varchar(2) NOT NULL default ' ',
  "AAABEARE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJRGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEARI" (
  "AAABJATT" varchar(3) NOT NULL default ' ',
  "AAABIDRZ" varchar(3) NOT NULL default ' ',
  "AAABMMRH" varchar(3) NOT NULL default '000',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABJATT","AAABIDRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEARQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEARV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEASE" (
  "AAABJATT" varchar(3) NOT NULL default ' ',
  "AAABMMRH" varchar(3) NOT NULL default ' ',
  "AAABMLZS" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABEASJ" varchar(1) NOT NULL default ' ',
  "AAABEASK" varchar(1) NOT NULL default ' ',
  "AAABEASL" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABJATT","AAABMMRH","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEASP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJPBG" varchar(3) NOT NULL default '000',
  "AAABJOWT" varchar(3) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJPBG","AAABJOWT","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEATA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJWRR" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEATI" (
  "AAABJOWT" varchar(3) NOT NULL default ' ',
  "AAABEATK" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABJOWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEATM" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJOWT" varchar(3) NOT NULL default ' ',
  "AAABEATP" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABJOWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEATS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABLCCR" varchar(6) NOT NULL default ' ',
  "AAABLCCC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK","AAABLCCR","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABECQG" varchar(3) NOT NULL default ' ',
  "AAABECQD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABECQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAUJ" (
  "AAABECQH" varchar(4) NOT NULL default ' ',
  "AAABECQG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABECQH","AAABECQG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAUO" (
  "AAABLCOG" varchar(1) NOT NULL default ' ',
  "AAABECQH" varchar(4) NOT NULL default ' ',
  "AAABEAUR" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCOG","AAABECQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAUU" (
  "AAABMCMG" varchar(6) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLGCM" varchar(42) NOT NULL default ' ',
  "AAABEAUY" varchar(2) NOT NULL default ' ',
  "AAABEAUZ" varchar(2) NOT NULL default ' ',
  "AAABEAVA" varchar(2) NOT NULL default ' ',
  "AAABEAVB" varchar(2) NOT NULL default ' ',
  "AAABLXGN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMCMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAVE" (
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABLTXI","AAABLEST","AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAVU" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABMOHE" varchar(10) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEPKB","AAABLTXI","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAWM" (
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABEAWQ" decimal(7,0) NOT NULL default '0',
  "AAABEAWR" decimal(7,0) NOT NULL default '0',
  "AAABEAWS" decimal(7,0) NOT NULL default '0',
  "AAABEAWT" decimal(7,0) NOT NULL default '0',
  "AAABKNGX" varchar(10) NOT NULL default ' ',
  "AAABMQIX" varchar(1) NOT NULL default ' ',
  "AAABEAWW" varchar(20) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  "AAABEAXE" varchar(6) NOT NULL default ' ',
  "AAABIGHT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABLTXI","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAXJ" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEBIF" varchar(10) NOT NULL default ' ',
  "AAABEBIG" varchar(2) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEBIF","AAABEBIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAXY" (
  "AAABEBIF" varchar(10) NOT NULL default ' ',
  "AAABEBIG" varchar(2) NOT NULL default ' ',
  "AAABKNGX" varchar(10) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBIF","AAABEBIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAYL" (
  "AAABEBIF" varchar(10) NOT NULL default ' ',
  "AAABEBIG" varchar(2) NOT NULL default ' ',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABEAYR" varchar(2) NOT NULL default '00',
  "AAABEAYS" varchar(3) NOT NULL default '000',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  "AAABEAZA" varchar(2) NOT NULL default '00',
  "AAABEAZB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABEBIF","AAABEBIG","AAABEPKB","AAABLTXI","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEAZH" (
  "AAABEBGA" varchar(4) NOT NULL default ' ',
  "AAABEBGB" varchar(2) NOT NULL default ' ',
  "AAABEBGC" varchar(2) NOT NULL default ' ',
  "AAABEBGD" varchar(4) NOT NULL default ' ',
  "AAABEBGE" varchar(2) NOT NULL default ' ',
  "AAABEBGF" varchar(2) NOT NULL default ' ',
  "AAABEBAI" varchar(1) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABEAZR" varchar(1) NOT NULL default ' ',
  "AAABEAZS" varchar(1) NOT NULL default ' ',
  "AAABEAZT" varchar(2) NOT NULL default ' ',
  "AAABEAZU" varchar(2) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBGA","AAABEBGB","AAABEBGC","AAABEBGD","AAABEBGE","AAABEBGF","AAABEBAI","AAABLTXI","AAABLEST"),
  KEY "AAABEBAL" ("AAABEBGA","AAABEBGB","AAABEBGC"),
  KEY "AAABEBAP" ("AAABEBGD","AAABEBGE","AAABEBGF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBAT" (
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABEBDH" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABEBDJ" varchar(30) NOT NULL default ' ',
  "AAABEBDK" varchar(2) NOT NULL default ' ',
  "AAABEBAZ" varchar(2) NOT NULL default ' ',
  "AAABEBBA" varchar(1) NOT NULL default ' ',
  "AAABEBCS" varchar(4) NOT NULL default ' ',
  "AAABEBCY" varchar(2) NOT NULL default '00',
  "AAABEBBD" varchar(10) NOT NULL default ' ',
  "AAABEBBE" varchar(2) NOT NULL default ' ',
  "AAABEBBF" varchar(2) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  "AAABEBBN" varchar(4) NOT NULL default ' ',
  "AAABEBBO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABEPKB","AAABEBDH","AAABLEST","AAABEBDJ","AAABEBDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBBU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABEBCC" varchar(35) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRHS","AAABEPKB","AAABEBCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBCD" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABEBCL" varchar(35) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRHS","AAABEPKB","AAABEBCL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBCM" (
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABEBDH" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABEBDJ" varchar(30) NOT NULL default ' ',
  "AAABEBDK" varchar(2) NOT NULL default ' ',
  "AAABEBCS" varchar(4) NOT NULL default ' ',
  "AAABEBCT" varchar(1) NOT NULL default ' ',
  "AAABEBCU" varchar(1) NOT NULL default ' ',
  "AAABEBCV" varchar(1) NOT NULL default ' ',
  "AAABEBCW" varchar(1) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  "AAABEBCY" varchar(2) NOT NULL default '00',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABEBDH","AAABLEST","AAABEBDJ","AAABEBDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBDL" (
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABMDEP" varchar(4) NOT NULL default ' ',
  "AAABKNBQ" varchar(1) NOT NULL default ' ',
  "AAABKMLX" varchar(12) NOT NULL default ' ',
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAABEBDU" varchar(1) NOT NULL default ' ',
  "AAABKMMB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABLTXI","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBDZ" (
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABEBGG" varchar(4) NOT NULL default ' ',
  "AAABKNGX" varchar(10) NOT NULL default ' ',
  "AAABLILS" varchar(1) NOT NULL default ' ',
  "AAABEBEG" varchar(1) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPKB","AAABLTXI","AAABLEST","AAABEBGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBES" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABEBGG" varchar(4) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEPKB","AAABLTXI","AAABLEST","AAABEBGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBFL" (
  "AAABEBGA" varchar(4) NOT NULL default ' ',
  "AAABEBGB" varchar(2) NOT NULL default ' ',
  "AAABEBGC" varchar(2) NOT NULL default ' ',
  "AAABEBGD" varchar(4) NOT NULL default ' ',
  "AAABEBGE" varchar(2) NOT NULL default ' ',
  "AAABEBGF" varchar(2) NOT NULL default ' ',
  "AAABEBGG" varchar(4) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBGA","AAABEBGB","AAABEBGC","AAABEBGD","AAABEBGE","AAABEBGF","AAABEBGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBGH" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEBIH" varchar(10) NOT NULL default ' ',
  "AAABEBII" varchar(2) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEBIH","AAABEBII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBGW" (
  "AAABEBIH" varchar(10) NOT NULL default ' ',
  "AAABEBII" varchar(2) NOT NULL default ' ',
  "AAABKNGX" varchar(10) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  "AAABHYMV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBIH","AAABEBII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBHK" (
  "AAABEBIH" varchar(10) NOT NULL default ' ',
  "AAABEBII" varchar(2) NOT NULL default ' ',
  "AAABEBIF" varchar(10) NOT NULL default ' ',
  "AAABEBIG" varchar(2) NOT NULL default ' ',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBIH","AAABEBII","AAABEBIF","AAABEBIG","AAABEPKB","AAABLTXI"),
  KEY "AAABEBIE" ("AAABEBIF","AAABEBIG","AAABEBIH","AAABEBII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBIJ" (
  "AAABEBIL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBIM" (
  "AAABKNGX" varchar(10) NOT NULL default ' ',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABLZCR" varchar(30) NOT NULL default ' ',
  "AAABLVHG" varchar(30) NOT NULL default ' ',
  "AAABLVHH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABKNGX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMDE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJMDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMDE" varchar(1) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABEBJL" varchar(2) NOT NULL default ' ',
  "AAABEBJM" varchar(2) NOT NULL default '00',
  "AAABEBJN" varchar(3) NOT NULL default '000',
  "AAABEBJO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBJS" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABKGYQ" varchar(40) NOT NULL default ' ',
  "AAABKGYP" varchar(20) NOT NULL default ' ',
  "AAABKGYO" varchar(10) NOT NULL default ' ',
  "AAABLAUS" varchar(55) NOT NULL default ' ',
  "AAABHUGE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREL","AAABLDBT","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBKN" (
  "AAABEBKS" varchar(20) NOT NULL default ' ',
  "AAABIVVF" varchar(10) NOT NULL default ' ',
  "AAABEBKQ" varchar(5) NOT NULL default ' ',
  "AAABEBKR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABEBKS","AAABIVVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBKU" (
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBKX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJQKL" varchar(1) NOT NULL default ' ',
  "AAABEQOQ" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJQKL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJQWQ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJQWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJQWQ" varchar(10) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABJQWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPRX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJPRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJPRX" varchar(2) NOT NULL default '00',
  "AAABKTYZ" varchar(30) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJPRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPRY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJPRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBMO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJPRY" varchar(2) NOT NULL default '00',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJPRY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPRU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABJPRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJPRU" varchar(2) NOT NULL default '00',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJPRU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPSH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJPSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJPSH" varchar(2) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJPSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEBOM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEBOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBOE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEBOM" varchar(1) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEBOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBON" (
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
  KEY "AAABEBQD" ("AAABJAAI"),
  KEY "AAABEBQF" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBQI" (
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
CREATE TABLE "AAABEBRE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBRS" (
  "AAABLUER" varchar(2) NOT NULL default '00',
  "AAABEBRU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBRW" (
  "AAABLCOG" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(2) NOT NULL default '00',
  "AAABEBRZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCOG","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBSC" (
  "AAABLAPR" varchar(10) NOT NULL default ' ',
  "AAABEBSG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLAPR","AAABEBSG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLUER" varchar(10) NOT NULL default ' ',
  "AAABLCXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBSP" (
  "AAABLSDL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBSS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJQHP" varchar(6) NOT NULL default '000000',
  "AAABEBTB" varchar(1) NOT NULL default ' ',
  "AAABEBTC" varchar(1) NOT NULL default ' ',
  "AAABEBTD" varchar(1) NOT NULL default ' ',
  "AAABEBTE" varchar(1) NOT NULL default ' ',
  "AAABEBTF" varchar(30) NOT NULL default ' ',
  "AAABEBTG" varchar(30) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default ' ',
  "AAABEBTI" varchar(20) NOT NULL default ' ',
  "AAABEBTJ" varchar(20) NOT NULL default ' ',
  "AAABEBTK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJQHP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAAS" varchar(1) NOT NULL default ' ',
  "AAABMNLW" varchar(4) NOT NULL default ' ',
  "AAABEBTR" varchar(40) NOT NULL default ' ',
  "AAABEBTS" varchar(1) NOT NULL default ' ',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  "AAABEBTU" varchar(1) NOT NULL default ' ',
  "AAABEBTV" varchar(2) NOT NULL default ' ',
  "AAABKVBV" varchar(1) NOT NULL default ' ',
  "AAABEBTX" varchar(20) NOT NULL default ' ',
  "AAABEBTY" varchar(40) NOT NULL default ' ',
  "AAABKUXH" varchar(1) NOT NULL default ' ',
  "AAABHVKC" varchar(1) NOT NULL default ' ',
  "AAABEBUB" varchar(1) NOT NULL default ' ',
  "AAABEBUC" varchar(1) NOT NULL default ' ',
  "AAABEBUD" varchar(1) NOT NULL default ' ',
  "AAABEBUE" varchar(1) NOT NULL default ' ',
  "AAABEBUF" varchar(1) NOT NULL default ' ',
  "AAABEBUG" varchar(1) NOT NULL default ' ',
  "AAABEBUH" varchar(10) NOT NULL default ' ',
  "AAABEBUI" varchar(15) NOT NULL default ' ',
  "AAABEBUJ" varchar(1) NOT NULL default ' ',
  "AAABEBUK" varchar(10) NOT NULL default ' ',
  "AAABEBUL" varchar(15) NOT NULL default ' ',
  "AAABEBUM" varchar(1) NOT NULL default ' ',
  "AAABEBUN" varchar(10) NOT NULL default ' ',
  "AAABEBUO" varchar(15) NOT NULL default ' ',
  "AAABEBUP" varchar(1) NOT NULL default ' ',
  "AAABEBUQ" varchar(10) NOT NULL default ' ',
  "AAABEBUR" varchar(15) NOT NULL default ' ',
  "AAABEBUS" varchar(1) NOT NULL default ' ',
  "AAABEBUT" varchar(10) NOT NULL default ' ',
  "AAABEBUU" varchar(15) NOT NULL default ' ',
  "AAABEBUV" varchar(1) NOT NULL default ' ',
  "AAABEBUW" varchar(1) NOT NULL default ' ',
  "AAABEBUX" varchar(1) NOT NULL default ' ',
  "AAABEBUY" varchar(1) NOT NULL default ' ',
  "AAABKAMJ" varchar(1) NOT NULL default ' ',
  "AAABEBVA" varchar(1) NOT NULL default ' ',
  "AAABEBVB" varchar(1) NOT NULL default ' ',
  "AAABIGRE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAAS","AAABMNLW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAAS" varchar(1) NOT NULL default ' ',
  "AAABLCCR" varchar(6) NOT NULL default ' ',
  "AAABHAQM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMAAS" varchar(1) NOT NULL default ' ',
  "AAABKTYZ" varchar(15) NOT NULL default ' ',
  "AAABKTZA" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMAAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBWC" (
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABJEMU" varchar(30) NOT NULL default ' ',
  "AAABEBWG" varchar(1) NOT NULL default ' ',
  "AAABEBWH" varchar(1) NOT NULL default ' ',
  "AAABEPCU" varchar(1) NOT NULL default ' ',
  "AAABEBWJ" varchar(1) NOT NULL default ' ',
  "AAABEBWK" varchar(1) NOT NULL default ' ',
  "AAABEBWL" varchar(1) NOT NULL default ' ',
  "AAABEBWM" varchar(1) NOT NULL default '0',
  "AAABEBWN" varchar(1) NOT NULL default '0',
  "AAABEBWO" varchar(1) NOT NULL default '0',
  "AAABEBWP" varchar(1) NOT NULL default '0',
  "AAABEBWQ" varchar(1) NOT NULL default ' ',
  "AAABEBWR" varchar(1) NOT NULL default ' ',
  "AAABFSSB" varchar(16) NOT NULL default ' ',
  "AAABEBWT" varchar(1) NOT NULL default ' ',
  "AAABEBWU" varchar(30) NOT NULL default ' ',
  "AAABEBWV" varchar(30) NOT NULL default ' ',
  "AAABEDJD" varchar(1) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAABEBYP" varchar(8) NOT NULL default ' ',
  "AAABEBYQ" varchar(3) NOT NULL default '000',
  "AAABEBYR" varchar(40) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBXE" (
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABLEEK" varchar(4) NOT NULL default ' ',
  "AAABEPGB" varchar(4) NOT NULL default ' ',
  "AAABEBXI" varchar(1) NOT NULL default ' ',
  "AAABEBXJ" varchar(30) NOT NULL default ' ',
  "AAABEDJD" varchar(1) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAABEBYP" varchar(8) NOT NULL default ' ',
  "AAABEBYQ" varchar(3) NOT NULL default '000',
  "AAABEBYR" varchar(40) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBXS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABEPDQ" varchar(50) NOT NULL default ' ',
  "AAABEDJD" varchar(1) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAABEBYP" varchar(8) NOT NULL default ' ',
  "AAABEBYQ" varchar(3) NOT NULL default '000',
  "AAABEBYR" varchar(40) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABEPDW" varchar(16) NOT NULL default ' ',
  "AAABEPDX" varchar(16) NOT NULL default ' ',
  "AAABEPDY" varchar(16) NOT NULL default ' ',
  "AAABEPDZ" varchar(16) NOT NULL default ' ',
  "AAABEDJD" varchar(1) NOT NULL default ' ',
  "AAABKWET" varchar(8) NOT NULL default '00000000',
  "AAABGCZV" varchar(6) NOT NULL default '000000',
  "AAABEBYP" varchar(8) NOT NULL default ' ',
  "AAABEBYQ" varchar(3) NOT NULL default '000',
  "AAABEBYR" varchar(40) NOT NULL default ' ',
  "AAABEBYS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBYV" (
  "AAABLEEK" varchar(4) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLEEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBYZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLEEK" varchar(4) NOT NULL default ' ',
  "AAABEPEJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLEEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBZF" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBZI" (
  "AAABILWI" varchar(20) NOT NULL default ' ',
  "AAABLGCM" varchar(30) NOT NULL default ' ',
  "AAABEBZL" varchar(8) NOT NULL default '00000000',
  "AAABEBZM" varchar(6) NOT NULL default '000000',
  "AAABEBZN" varchar(3) NOT NULL default ' ',
  "AAABEBZO" varchar(1) NOT NULL default ' ',
  "AAABFLVY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABILWI","AAABLGCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPFD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBZW" (
  "AAABEPGB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEBZZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPGB" varchar(4) NOT NULL default ' ',
  "AAABEPFZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEPGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABECAI" varchar(10) NOT NULL default ' ',
  "AAABEPGB" varchar(4) NOT NULL default ' ',
  "AAABECAK" varchar(3) NOT NULL default ' ',
  "AAABLUKI" varchar(4) NOT NULL default ' ',
  "AAABECAM" varchar(12) NOT NULL default ' ',
  "AAABECAN" varchar(8) NOT NULL default '00000000',
  "AAABECAO" varchar(6) NOT NULL default '000000',
  "AAABECAP" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABECAW" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABECBF" varchar(2) NOT NULL default ' ',
  "AAABECBG" varchar(1) NOT NULL default ' ',
  "AAABLARI" varchar(8) NOT NULL default '00000000',
  "AAABJDPL" varchar(8) NOT NULL default '00000000',
  "AAABLTLO" varchar(12) NOT NULL default ' ',
  "AAABLTLD" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABMGSS" varchar(12) NOT NULL default ' ',
  "AAABMGST" varchar(8) NOT NULL default '00000000',
  "AAABMGSU" varchar(6) NOT NULL default '000000',
  "AAABECBP" varchar(12) NOT NULL default ' ',
  "AAABECBQ" varchar(8) NOT NULL default '00000000',
  "AAABECBR" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECBW" (
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEPGB" varchar(4) NOT NULL default ' ',
  "AAABMDBL" varchar(2) NOT NULL default ' ',
  "AAABECCA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABECCJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABECCV" varchar(30) NOT NULL default ' ',
  "AAABECCW" varchar(1) NOT NULL default ' ',
  "AAABECCX" varchar(1) NOT NULL default ' ',
  "AAABFSSB" varchar(16) NOT NULL default ' ',
  "AAABECCZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYMV" varchar(18) NOT NULL default ' ',
  "AAABLTXI" varchar(4) NOT NULL default '0000',
  "AAABECDQ" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABECDS" varchar(2) NOT NULL default '00',
  "AAABECDM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYMV","AAABLTXI","AAABECDQ","AAABEPFA","AAABECDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(4) NOT NULL default ' ',
  "AAABECDW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(4) NOT NULL default ' ',
  "AAABGNFY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABECFM" varchar(1) NOT NULL default ' ',
  "AAABKKRR" varchar(3) NOT NULL default ' ',
  "AAABKHNV" varchar(3) NOT NULL default ' ',
  "AAABJRBK" varchar(5) NOT NULL default ' ',
  "AAABECEN" int(11) NOT NULL default '0',
  "AAABECEO" int(11) NOT NULL default '0',
  "AAABECEP" varchar(16) NOT NULL default ' ',
  "AAABECEQ" decimal(3,0) NOT NULL default '0',
  "AAABEVEE" varchar(1) NOT NULL default ' ',
  "AAABEVEH" varchar(10) NOT NULL default ' ',
  "AAABECET" decimal(5,3) NOT NULL default '0.000',
  "AAABECEU" varchar(1) NOT NULL default ' ',
  "AAABECEV" decimal(5,3) NOT NULL default '0.000',
  "AAABECEW" varchar(1) NOT NULL default ' ',
  "AAABECPF" varchar(8) NOT NULL default '00000000',
  "AAABECEY" varchar(1) NOT NULL default ' ',
  "AAABECEZ" varchar(1) NOT NULL default ' ',
  "AAABECFA" varchar(1) NOT NULL default ' ',
  "AAABECFB" varchar(1) NOT NULL default ' ',
  "AAABECFC" varchar(1) NOT NULL default ' ',
  "AAABECFD" varchar(1) NOT NULL default ' ',
  "AAABECFE" varchar(1) NOT NULL default ' ',
  "AAABECFF" varchar(1) NOT NULL default ' ',
  "AAABECFG" varchar(1) NOT NULL default ' ',
  "AAABECFH" varchar(1) NOT NULL default ' ',
  "AAABECFI" varchar(1) NOT NULL default ' ',
  "AAABECFJ" varchar(1) NOT NULL default ' ',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABECFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJPBG" varchar(3) NOT NULL default '000',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  "AAABECFR" varchar(1) NOT NULL default ' ',
  "AAABMEAG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABJPBG","AAABLDBT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECFW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABECKC" varchar(1) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABECGB" varchar(1) NOT NULL default ' ',
  "AAABECGC" varchar(1) NOT NULL default ' ',
  "AAABLWIT" varchar(1) NOT NULL default ' ',
  "AAABLWIV" varchar(1) NOT NULL default ' ',
  "AAABLWIX" varchar(1) NOT NULL default ' ',
  "AAABLWIZ" varchar(1) NOT NULL default ' ',
  "AAABECGH" varchar(1) NOT NULL default ' ',
  "AAABECGI" varchar(1) NOT NULL default ' ',
  "AAABECGJ" varchar(1) NOT NULL default ' ',
  "AAABECGK" varchar(1) NOT NULL default ' ',
  "AAABECGL" varchar(1) NOT NULL default ' ',
  "AAABECGM" varchar(1) NOT NULL default ' ',
  "AAABECGN" varchar(1) NOT NULL default ' ',
  "AAABECGO" varchar(1) NOT NULL default ' ',
  "AAABLDQW" varchar(20) NOT NULL default ' ',
  "AAABECGQ" varchar(2) NOT NULL default '00',
  "AAABECGR" varchar(4) NOT NULL default ' ',
  "AAABECGS" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABECKC","AAABLCAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJPBG" varchar(3) NOT NULL default '000',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAABLDBT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJPBG","AAABLAXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJQWF" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABJQWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQWF" varchar(3) NOT NULL default '000',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJQWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJQWE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJQWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJQWE" varchar(3) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJQWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABECKC" varchar(1) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABECKE" varchar(2) NOT NULL default ' ',
  "AAABECIN" varchar(4) NOT NULL default ' ',
  "AAABJQTY" varchar(1) NOT NULL default ' ',
  "AAABJQTZ" varchar(1) NOT NULL default ' ',
  "AAABECIQ" varchar(1) NOT NULL default ' ',
  "AAABECIR" varchar(1) NOT NULL default ' ',
  "AAABJQVD" varchar(1) NOT NULL default ' ',
  "AAABJQTV" varchar(1) NOT NULL default ' ',
  "AAABECIU" varchar(1) NOT NULL default ' ',
  "AAABECIV" varchar(1) NOT NULL default ' ',
  "AAABMEFR" varchar(1) NOT NULL default ' ',
  "AAABECIX" varchar(1) NOT NULL default ' ',
  "AAABMEFL" varchar(2) NOT NULL default ' ',
  "AAABJQVE" varchar(1) NOT NULL default ' ',
  "AAABJQVN" varchar(1) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABLCXR" varchar(1) NOT NULL default ' ',
  "AAABJQVV" varchar(1) NOT NULL default ' ',
  "AAABLCWK" varchar(2) NOT NULL default '00',
  "AAABLCWJ" varchar(2) NOT NULL default '00',
  "AAABECJG" decimal(3,0) NOT NULL default '0',
  "AAABECJH" varchar(1) NOT NULL default ' ',
  "AAABECJI" varchar(2) NOT NULL default ' ',
  "AAABJQWI" varchar(1) NOT NULL default ' ',
  "AAABJQVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABECKC","AAABLCAK","AAABECKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABECKC" varchar(1) NOT NULL default ' ',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABECKE" varchar(2) NOT NULL default ' ',
  "AAABJQTW" varchar(40) NOT NULL default ' ',
  "AAABECJY" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMREX","AAABECKC","AAABLCAK","AAABECKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECKF" (
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABECKQ" varchar(3) NOT NULL default ' ',
  "AAABECKL" varchar(6) NOT NULL default ' ',
  "AAABECKM" varchar(6) NOT NULL default ' ',
  "AAABECKN" varchar(1) NOT NULL default ' ',
  "AAABLVFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABECKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECKR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABECLA" varchar(7) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABMCQA" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABECLA","AAABMEFT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMCC" varchar(4) NOT NULL default ' ',
  "AAABLAXF" varchar(8) NOT NULL default ' ',
  "AAABECLG" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJMCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(2) NOT NULL default '00',
  "AAABECLM" varchar(2) NOT NULL default ' ',
  "AAABECLN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCRW" varchar(1) NOT NULL default ' ',
  "AAABECLU" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCRW" varchar(1) NOT NULL default ' ',
  "AAABJPMO" varchar(1) NOT NULL default '0',
  "AAABECMC" varchar(3) NOT NULL default '000',
  "AAABECMD" varchar(3) NOT NULL default '000',
  "AAABLTUX" varchar(1) NOT NULL default ' ',
  "AAABECMF" decimal(5,0) NOT NULL default '0',
  "AAABMQRD" decimal(10,7) NOT NULL default '0.0000000',
  PRIMARY KEY ("AAABMRIL","AAABLCRW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECMJ" (
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABHJFS" varchar(30) NOT NULL default ' ',
  "AAABGGIZ" varchar(30) NOT NULL default ' ',
  "AAABECMN" varchar(1) NOT NULL default ' ',
  "AAABECMO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLMHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECMQ" (
  "AAABECND" varchar(10) NOT NULL default ' ',
  "AAABECOF" varchar(10) NOT NULL default ' ',
  "AAABECMT" varchar(1) NOT NULL default ' ',
  "AAABECMU" varchar(1) NOT NULL default ' ',
  "AAABHJJH" varchar(1) NOT NULL default ' ',
  "AAABHJJI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABECND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECMY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABECND" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABECND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECNE" (
  "AAABECNZ" varchar(4) NOT NULL default ' ',
  "AAABECOD" varchar(1) NOT NULL default ' ',
  "AAABECOE" varchar(1) NOT NULL default ' ',
  "AAABECNI" varchar(1) NOT NULL default ' ',
  "AAABECNJ" varchar(1) NOT NULL default ' ',
  "AAABECNK" varchar(1) NOT NULL default ' ',
  "AAABLTXO" varchar(4) NOT NULL default ' ',
  "AAABHJLH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABECNZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECNO" (
  "AAABECOF" varchar(10) NOT NULL default ' ',
  "AAABECNT" varchar(4) NOT NULL default ' ',
  "AAABECNU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABECOF","AAABECNT","AAABECNU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECNV" (
  "AAABECNZ" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABECNZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECOB" (
  "AAABECOF" varchar(10) NOT NULL default ' ',
  "AAABECOD" varchar(1) NOT NULL default ' ',
  "AAABECOE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABECOF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECOG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMCC" varchar(4) NOT NULL default ' ',
  "AAABECOK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCCC" varchar(4) NOT NULL default ' ',
  "AAABKVKH" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLCCC" varchar(4) NOT NULL default ' ',
  "AAABGNFY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLCCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABECPF" varchar(8) NOT NULL default '00000000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLUZZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECPK" (
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAABMEBC" varchar(4) NOT NULL default ' ',
  "AAABJMEB" varchar(1) NOT NULL default ' ',
  "AAABLAXJ" varchar(2) NOT NULL default '00',
  "AAABJNZY" varchar(4) NOT NULL default ' ',
  "AAABECPR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLCPX","AAABLAXH","AAABMEBC","AAABJMEB","AAABLAXJ","AAABJNZY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLCAK" varchar(3) NOT NULL default ' ',
  "AAABECQG" varchar(3) NOT NULL default ' ',
  "AAABECQH" varchar(4) NOT NULL default ' ',
  "AAABECQD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLCAK","AAABECQG","AAABECQH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECQI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJYKA" varchar(250) NOT NULL default ' ',
  "AAABECQL" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECQN" (
  "AAABGQXP" varchar(40) NOT NULL default ' ',
  "AAABECQP" varchar(252) NOT NULL default ' ',
  "AAABECQQ" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAABGQXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABECQV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLW","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNLW" varchar(20) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNLW","AAABMCFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABLCCR" varchar(6) NOT NULL default ' ',
  "AAABLCCK" varchar(1) NOT NULL default ' ',
  "AAABECRT" varchar(1) NOT NULL default '0',
  "AAABLCCL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLCCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABLCCR" varchar(6) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABECSD" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLECD","AAABLCCR","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABECSM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP","AAABMCTG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABECST" varchar(20) NOT NULL default ' ',
  "AAABECSU" varchar(1) NOT NULL default ' ',
  "AAABECSV" varchar(1) NOT NULL default ' ',
  "AAABECSW" varchar(1) NOT NULL default ' ',
  "AAABECSX" varchar(1) NOT NULL default ' ',
  "AAABECSY" varchar(1) NOT NULL default ' ',
  "AAABECSZ" varchar(2) NOT NULL default ' ',
  "AAABECTA" varchar(2) NOT NULL default ' ',
  "AAABECTB" varchar(1) NOT NULL default ' ',
  "AAABECTC" varchar(1) NOT NULL default ' ',
  "AAABECTD" varchar(4) NOT NULL default '0000',
  "AAABECTE" varchar(40) NOT NULL default ' ',
  "AAABECTF" varchar(40) NOT NULL default ' ',
  "AAABECTG" varchar(1) NOT NULL default ' ',
  "AAABECTH" varchar(10) NOT NULL default '0000000000',
  "AAABECTI" varchar(18) NOT NULL default ' ',
  "AAABECTJ" varchar(3) NOT NULL default ' ',
  "AAABECTK" varchar(1) NOT NULL default ' ',
  "AAABECTL" varchar(1) NOT NULL default ' ',
  "AAABECTM" varchar(1) NOT NULL default ' ',
  "AAABECTN" varchar(1) NOT NULL default ' ',
  "AAABECTO" varchar(3) NOT NULL default ' ',
  "AAABECTP" varchar(3) NOT NULL default ' ',
  "AAABECTQ" varchar(1) NOT NULL default ' ',
  "AAABECTR" varchar(1) NOT NULL default ' ',
  "AAABECUQ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLCPX" varchar(1) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLCPX","AAABLDCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDBR" varchar(1) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABECUI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDBR","AAABIMRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECUM" (
  "AAABECUQ" varchar(5) NOT NULL default ' ',
  "AAABECUR" varchar(4) NOT NULL default '0000',
  "AAABIMRG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABECUQ","AAABECUR"),
  KEY "AAABECUS" ("AAABIMRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECUU" (
  "AAABECXL" varchar(10) NOT NULL default ' ',
  "AAABHTPK" varchar(2) NOT NULL default ' ',
  "AAABECUX" varchar(20) NOT NULL default ' ',
  "AAABECYZ" varchar(40) NOT NULL default ' ',
  "AAABECUZ" varchar(70) NOT NULL default ' ',
  "AAABECVA" varchar(255) NOT NULL default ' ',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABECXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECVD" (
  "AAABHTPK" varchar(2) NOT NULL default ' ',
  "AAABECVF" varchar(1) NOT NULL default ' ',
  "AAABECVG" varchar(1) NOT NULL default ' ',
  "AAABECVH" varchar(1) NOT NULL default ' ',
  "AAABECVI" varchar(3) NOT NULL default ' ',
  "AAABECVJ" varchar(10) NOT NULL default ' ',
  "AAABECYZ" varchar(40) NOT NULL default ' ',
  "AAABLEJR" varchar(70) NOT NULL default ' ',
  "AAABECVM" varchar(1) NOT NULL default ' ',
  "AAABECVN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHTPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABECXD" varchar(10) NOT NULL default ' ',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABECVT" varchar(1) NOT NULL default ' ',
  "AAABECVU" varchar(1) NOT NULL default ' ',
  "AAABECVV" varchar(1) NOT NULL default ' ',
  "AAABECVW" varchar(1) NOT NULL default ' ',
  "AAABECVX" varchar(1) NOT NULL default ' ',
  "AAABECVY" varchar(1) NOT NULL default ' ',
  "AAABECVZ" varchar(1) NOT NULL default ' ',
  "AAABECWA" varchar(1) NOT NULL default ' ',
  "AAABECWB" varchar(1) NOT NULL default ' ',
  "AAABECWC" varchar(1) NOT NULL default ' ',
  "AAABECWD" varchar(1) NOT NULL default ' ',
  "AAABECWE" varchar(1) NOT NULL default ' ',
  "AAABECWF" varchar(1) NOT NULL default ' ',
  "AAABECWG" varchar(1) NOT NULL default ' ',
  "AAABECWH" varchar(1) NOT NULL default ' ',
  "AAABECWI" varchar(1) NOT NULL default ' ',
  "AAABECWJ" varchar(1) NOT NULL default ' ',
  "AAABECWK" varchar(1) NOT NULL default ' ',
  "AAABECWL" varchar(12) NOT NULL default ' ',
  "AAABECWM" varchar(3) NOT NULL default ' ',
  "AAABECWN" varchar(2) NOT NULL default ' ',
  "AAABECWO" varchar(3) NOT NULL default ' ',
  "AAABECWP" varchar(2) NOT NULL default ' ',
  "AAABECWQ" varchar(10) NOT NULL default ' ',
  "AAABECWR" varchar(12) NOT NULL default ' ',
  "AAABECWS" varchar(3) NOT NULL default ' ',
  "AAABECWT" varchar(3) NOT NULL default ' ',
  "AAABECWU" varchar(3) NOT NULL default ' ',
  "AAABECWV" varchar(3) NOT NULL default ' ',
  "AAABECWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABECXD","AAABIMRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABECXD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECXG" (
  "AAABHTPK" varchar(2) NOT NULL default ' ',
  "AAABECXL" varchar(10) NOT NULL default ' ',
  "AAABECXJ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABHTPK","AAABECXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABECYT" varchar(10) NOT NULL default ' ',
  "AAABECXQ" varchar(1) NOT NULL default ' ',
  "AAABECXR" varchar(4) NOT NULL default '0000',
  "AAABECXS" varchar(30) NOT NULL default ' ',
  "AAABECXT" varchar(1) NOT NULL default ' ',
  "AAABECXU" varchar(1) NOT NULL default ' ',
  "AAABECXV" varchar(1) NOT NULL default ' ',
  "AAABECXW" varchar(1) NOT NULL default ' ',
  "AAABECXX" varchar(1) NOT NULL default ' ',
  "AAABECXY" varchar(1) NOT NULL default ' ',
  "AAABECXZ" varchar(1) NOT NULL default ' ',
  "AAABECYA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABECYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABECYT" varchar(10) NOT NULL default ' ',
  "AAABHFQD" varchar(4) NOT NULL default '0000',
  "AAABECYI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABECYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABECYT" varchar(10) NOT NULL default ' ',
  "AAABECYP" varchar(20) NOT NULL default ' ',
  "AAABECYQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABECYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECYU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABECYX" varchar(1) NOT NULL default ' ',
  "AAABECYY" varchar(2) NOT NULL default ' ',
  "AAABECYZ" varchar(40) NOT NULL default ' ',
  "AAABECZA" varchar(3) NOT NULL default ' ',
  "AAABECZB" varchar(30) NOT NULL default ' ',
  "AAABECZC" varchar(1) NOT NULL default ' ',
  "AAABECZD" varchar(1) NOT NULL default ' ',
  "AAABKERC" varchar(128) NOT NULL default ' ',
  "AAABECZF" varchar(1) NOT NULL default ' ',
  "AAABECZG" varchar(1) NOT NULL default ' ',
  "AAABECZH" varchar(1) NOT NULL default ' ',
  "AAABECZI" varchar(1) NOT NULL default ' ',
  "AAABECZJ" varchar(1) NOT NULL default ' ',
  "AAABECZK" varchar(1) NOT NULL default ' ',
  "AAABECZL" varchar(4) NOT NULL default ' ',
  "AAABECZM" varchar(10) NOT NULL default ' ',
  "AAABECZN" varchar(32) NOT NULL default ' ',
  "AAABECZO" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABECZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIMRD" varchar(3) NOT NULL default ' ',
  "AAABEDBS" varchar(2) NOT NULL default ' ',
  "AAABECZV" varchar(1) NOT NULL default ' ',
  "AAABECZW" varchar(1) NOT NULL default ' ',
  "AAABECZX" varchar(1) NOT NULL default ' ',
  "AAABECZY" varchar(1) NOT NULL default ' ',
  "AAABECZZ" varchar(2) NOT NULL default ' ',
  "AAABEDAA" varchar(2) NOT NULL default ' ',
  "AAABEDAB" varchar(2) NOT NULL default ' ',
  "AAABEDAC" varchar(2) NOT NULL default ' ',
  "AAABEDAD" varchar(2) NOT NULL default ' ',
  "AAABEDAE" varchar(2) NOT NULL default ' ',
  "AAABEDAF" varchar(1) NOT NULL default ' ',
  "AAABEDAG" varchar(1) NOT NULL default ' ',
  "AAABEDAH" varchar(14) NOT NULL default ' ',
  "AAABEDAI" varchar(40) NOT NULL default ' ',
  "AAABEDAJ" varchar(30) NOT NULL default ' ',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAABEDAM" varchar(1) NOT NULL default ' ',
  "AAABHJJK" varchar(1) NOT NULL default ' ',
  "AAABEDAO" varchar(8) NOT NULL default ' ',
  "AAABEDAP" varchar(1) NOT NULL default ' ',
  "AAABEDAQ" varchar(1) NOT NULL default ' ',
  "AAABEDAR" varchar(1) NOT NULL default ' ',
  "AAABEDAS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIMRD","AAABEDBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLELF" varchar(30) NOT NULL default ' ',
  "AAABLFOX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLELF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDBR" varchar(1) NOT NULL default ' ',
  "AAABLELF" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDBR","AAABLELF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDBK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDBR" varchar(1) NOT NULL default ' ',
  "AAABEDBS" varchar(2) NOT NULL default ' ',
  "AAABEDBO" varchar(2) NOT NULL default ' ',
  "AAABEDBP" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDBR","AAABEDBS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTPK" varchar(2) NOT NULL default ' ',
  "AAABHVMV" varchar(3) NOT NULL default ' ',
  "AAABEDCG" varchar(1) NOT NULL default ' ',
  "AAABEDCT" varchar(70) NOT NULL default ' ',
  "AAABEDBZ" varchar(70) NOT NULL default ' ',
  "AAABEDCA" varchar(1) NOT NULL default ' ',
  "AAABEDCB" varchar(255) NOT NULL default ' ',
  "AAABEDCC" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTPK","AAABHVMV","AAABEDCG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDCT" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEDCT" varchar(70) NOT NULL default ' ',
  "AAABLPSU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEDCT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDCU" (
  "AAABMIRY" varchar(10) NOT NULL default ' ',
  "AAABEDDS" varchar(3) NOT NULL default ' ',
  "AAABEDDT" varchar(4) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEDDA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMIRY","AAABEDDS","AAABEDDT","AAABLESS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDDF" (
  "AAABEDDS" varchar(3) NOT NULL default ' ',
  "AAABEDDT" varchar(4) NOT NULL default ' ',
  "AAABEDDI" varchar(1) NOT NULL default ' ',
  "AAABEDDJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABEDDS","AAABEDDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDDM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEDDS" varchar(3) NOT NULL default ' ',
  "AAABEDDT" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABEDDS","AAABEDDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDDU" (
  "AAABKMUQ" varchar(20) NOT NULL default ' ',
  "AAABHFCG" varchar(12) NOT NULL default ' ',
  "AAABFMVJ" varchar(12) NOT NULL default ' ',
  "AAABLKMO" varchar(8) NOT NULL default '00000000',
  "AAABLKMP" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABKMUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDEM" varchar(1) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABKOTY" varchar(10) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABKPUA" varchar(6) NOT NULL default ' ',
  "AAABFUKS" varchar(6) NOT NULL default ' ',
  "AAABLXOT" varchar(8) NOT NULL default '00000000',
  "AAABLRMN" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDEM","AAABMCRJ","AAABKOTY","AAABMCRK","AAABKPUA","AAABFUKS","AAABLXOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJEGO" varchar(2) NOT NULL default ' ',
  "AAABEQUD" varchar(12) NOT NULL default ' ',
  "AAABEDGW" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(32) NOT NULL default ' ',
  "AAABEDEZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJEGO","AAABEQUD","AAABEDGW","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDFF" (
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAABEDFH" varchar(1) NOT NULL default ' ',
  "AAABEDFI" varchar(1) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABEDFM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAABEDFY" int(11) NOT NULL default '0',
  "AAABEDFZ" varchar(1) NOT NULL default ' ',
  "AAABLUWF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDGD" (
  "AAABMLPV" varchar(8) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLPV","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJEGO" varchar(2) NOT NULL default ' ',
  "AAABEQUD" varchar(12) NOT NULL default ' ',
  "AAABEDGW" varchar(12) NOT NULL default ' ',
  "AAABMDEN" varchar(32) NOT NULL default ' ',
  "AAABEDHZ" varchar(3) NOT NULL default ' ',
  "AAABGRXB" varchar(1) NOT NULL default ' ',
  "AAABEDGR" varchar(1) NOT NULL default ' ',
  "AAABEDGS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJEGO","AAABEQUD","AAABEDGW","AAABMDEN","AAABEDHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDHZ" varchar(3) NOT NULL default ' ',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABEDHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLUTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUTZ" varchar(2) NOT NULL default '00',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUTZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDHS" (
  "AAABEDHZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDHV" (
  "AAABEDHZ" varchar(3) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDHZ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDJP" varchar(20) NOT NULL default ' ',
  "AAABEDJQ" varchar(40) NOT NULL default ' ',
  "AAABEDIF" varchar(12) NOT NULL default ' ',
  "AAABEDIG" varchar(8) NOT NULL default '00000000',
  "AAABEDIH" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABEDJP","AAABEDJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDJP" varchar(20) NOT NULL default ' ',
  "AAABEDJQ" varchar(40) NOT NULL default ' ',
  "AAABHBWG" varchar(10) NOT NULL default ' ',
  "AAABEDJB" varchar(2) NOT NULL default ' ',
  "AAABEDJC" varchar(10) NOT NULL default ' ',
  "AAABLPNX" varchar(20) NOT NULL default ' ',
  "AAABJOUB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDJP","AAABEDJQ","AAABHBWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBWG" varchar(10) NOT NULL default ' ',
  "AAABEDJB" varchar(2) NOT NULL default ' ',
  "AAABEDJC" varchar(10) NOT NULL default ' ',
  "AAABEDJD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBWG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDJP" varchar(20) NOT NULL default ' ',
  "AAABEDJQ" varchar(40) NOT NULL default ' ',
  "AAABEDJK" varchar(8) NOT NULL default '00000000',
  "AAABIWVY" varchar(16) NOT NULL default '0000000000000000',
  "AAABEDJM" varchar(10) NOT NULL default ' ',
  "AAABEDJN" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDJP","AAABEDJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDJR" (
  "AAABLTQJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABLTQJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDJX" (
  "AAABMNJW" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(5) NOT NULL default ' ',
  "AAABIJSB" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABMNJW","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDKD" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMLPV" varchar(20) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMLPV","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDKM" (
  "AAABLTQJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABIGTC" VARBINARY(255),
  PRIMARY KEY ("AAABLTQJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDKT" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABEDLQ" varchar(50) NOT NULL default ' ',
  "AAABLUZL" varchar(8) NOT NULL default ' ',
  "AAABEDLS" varchar(1) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABIGTC" VARBINARY(255),
  PRIMARY KEY ("AAABLZKF","AAABEDLQ","AAABLUZL","AAABEDLS","AAABLTQJ","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDLI" (
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABEDLQ" varchar(50) NOT NULL default ' ',
  "AAABLUZL" varchar(8) NOT NULL default ' ',
  "AAABEDLS" varchar(1) NOT NULL default ' ',
  "AAABHHRZ" varchar(5) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABLZKF","AAABEDLQ","AAABLUZL","AAABEDLS","AAABHHRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDLU" (
  "AAABHHRZ" varchar(5) NOT NULL default ' ',
  "AAABMRCK" longtext,
  PRIMARY KEY ("AAABHHRZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDLY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXVN" varchar(12) NOT NULL default ' ',
  "AAABEDMG" varchar(8) NOT NULL default '00000000',
  "AAABKRQD" varchar(12) NOT NULL default ' ',
  "AAABEDMD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXVN","AAABEDMG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABEDMK" varchar(3) NOT NULL default ' ',
  "AAABEDML" varchar(1) NOT NULL default ' ',
  "AAABEDMM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXVN" varchar(12) NOT NULL default ' ',
  "AAABLVJK" varchar(8) NOT NULL default '00000000',
  "AAABKMHA" varchar(12) NOT NULL default ' ',
  "AAABEDMU" varchar(8) NOT NULL default '00000000',
  "AAABEDMV" varchar(8) NOT NULL default '00000000',
  "AAABEDMW" varchar(1) NOT NULL default ' ',
  "AAABEDMX" varchar(18) NOT NULL default ' ',
  "AAABEDMY" varchar(18) NOT NULL default ' ',
  "AAABEDMZ" varchar(18) NOT NULL default ' ',
  "AAABEDNA" varchar(22) NOT NULL default ' ',
  "AAABEDNB" varchar(18) NOT NULL default ' ',
  "AAABEDNC" varchar(3) NOT NULL default ' ',
  "AAABEDND" varchar(4) NOT NULL default ' ',
  "AAABEDNE" varchar(30) NOT NULL default ' ',
  "AAABEDNF" varchar(22) NOT NULL default ' ',
  "AAABEDNG" varchar(1) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABEDNI" varchar(50) NOT NULL default ' ',
  "AAABEDNP" varchar(12) NOT NULL default ' ',
  "AAABEDNK" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXVN"),
  KEY "AAABEDNN" ("AAABMRIL","AAABEDNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXVN" varchar(12) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHIOX" varchar(30) NOT NULL default ' ',
  "AAABEDNV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXVN","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGBMM" varchar(30) NOT NULL default ' ',
  "AAABEDOZ" varchar(10) NOT NULL default ' ',
  "AAABHPQD" varchar(30) NOT NULL default ' ',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABEDPL" varchar(1) NOT NULL default ' ',
  "AAABEDPA" varchar(14) NOT NULL default ' ',
  "AAABEDPB" varchar(12) NOT NULL default ' ',
  "AAABFPZD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGBMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGBMM" varchar(30) NOT NULL default ' ',
  "AAABEDOZ" varchar(10) NOT NULL default ' ',
  "AAABEDPA" varchar(14) NOT NULL default ' ',
  "AAABEDOQ" varchar(1) NOT NULL default ' ',
  "AAABEDPB" varchar(12) NOT NULL default ' ',
  "AAABEDOS" varchar(40) NOT NULL default ' ',
  "AAABEDPM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGBMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDQP" varchar(4) NOT NULL default ' ',
  "AAABEDOZ" varchar(10) NOT NULL default ' ',
  "AAABEDPA" varchar(14) NOT NULL default ' ',
  "AAABEDPB" varchar(12) NOT NULL default ' ',
  "AAABFPZD" varchar(1) NOT NULL default ' ',
  "AAABEDPM" varchar(30) NOT NULL default ' ',
  "AAABIOAH" varchar(1) NOT NULL default ' ',
  "AAABKUYQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDPP" varchar(30) NOT NULL default ' ',
  "AAABEDPL" varchar(1) NOT NULL default ' ',
  "AAABEDPM" varchar(30) NOT NULL default ' ',
  "AAABIOAH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDPP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDPQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDQH" varchar(4) NOT NULL default ' ',
  "AAABMLPO" varchar(4) NOT NULL default '0000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDQH","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDQH" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLMHB" varchar(30) NOT NULL default ' ',
  "AAABLUFA" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDQH","AAABMRFQ","AAABMLES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDQK" (
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABHPQD" varchar(30) NOT NULL default ' ',
  "AAABLSCQ" varchar(1) NOT NULL default ' ',
  "AAABKNBS" varchar(1) NOT NULL default ' ',
  "AAABEDQP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDQR" (
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABEDRA" varchar(1) NOT NULL default ' ',
  "AAABJFIE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO","AAABEDRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDQX" (
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABEDQZ" varchar(1) NOT NULL default ' ',
  "AAABEDRA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDRC" (
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABMMVH" varchar(30) NOT NULL default ' ',
  "AAABEDRF" varchar(30) NOT NULL default ' ',
  "AAABEDRG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDRI" (
  "AAABEDRM" varchar(2) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFIE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDRM","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCTJ" varchar(32) NOT NULL default ' ',
  "AAABFKPK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCTJ","AAABFKPK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDTR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDTY" varchar(4) NOT NULL default ' ',
  "AAABEDSD" varchar(8) NOT NULL default ' ',
  "AAABEDSE" varchar(30) NOT NULL default ' ',
  "AAABEDSF" varchar(8) NOT NULL default ' ',
  "AAABEDSG" varchar(30) NOT NULL default ' ',
  "AAABEDSH" varchar(8) NOT NULL default ' ',
  "AAABEDSI" varchar(30) NOT NULL default ' ',
  "AAABEDSJ" varchar(8) NOT NULL default ' ',
  "AAABEDSK" varchar(30) NOT NULL default ' ',
  "AAABEDSL" varchar(8) NOT NULL default ' ',
  "AAABEDSM" varchar(30) NOT NULL default ' ',
  "AAABEDSN" varchar(8) NOT NULL default ' ',
  "AAABEDSO" varchar(30) NOT NULL default ' ',
  "AAABEDSP" varchar(8) NOT NULL default ' ',
  "AAABEDSQ" varchar(30) NOT NULL default ' ',
  "AAABEDSR" varchar(8) NOT NULL default ' ',
  "AAABEDSS" varchar(30) NOT NULL default ' ',
  "AAABLTXO" varchar(30) NOT NULL default ' ',
  "AAABEDSU" varchar(30) NOT NULL default ' ',
  "AAABEDSV" varchar(30) NOT NULL default ' ',
  "AAABEDSW" varchar(30) NOT NULL default ' ',
  "AAABEDSX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDTY" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEDTE" varchar(30) NOT NULL default ' ',
  "AAABIFON" varchar(30) NOT NULL default ' ',
  "AAABEDTG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDTY","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEDTR" varchar(4) NOT NULL default ' ',
  "AAABEDTO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEDTR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDTX" varchar(4) NOT NULL default ' ',
  "AAABEDTY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDTX","AAABEDTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDUW" varchar(2) NOT NULL default ' ',
  "AAABEDUC" varchar(1) NOT NULL default ' ',
  "AAABEDUD" varchar(4) NOT NULL default ' ',
  "AAABEDUT" varchar(1) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDUW" varchar(2) NOT NULL default ' ',
  "AAABMCFC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEDUW" varchar(2) NOT NULL default ' ',
  "AAABEDUR" varchar(1) NOT NULL default ' ',
  "AAABEDUS" varchar(4) NOT NULL default ' ',
  "AAABEDUT" varchar(1) NOT NULL default ' ',
  "AAABLUFX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEDUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDUX" (
  "AAABLUSV" int(11) NOT NULL default '0',
  "AAABEDUZ" varchar(60) NOT NULL default ' ',
  "AAABEDVA" varchar(8) NOT NULL default '00000000',
  "AAABEDVB" varchar(6) NOT NULL default '000000',
  "AAABGBVJ" varchar(8) NOT NULL default '00000000',
  "AAABGBVK" varchar(6) NOT NULL default '000000',
  "AAABLRQK" varchar(12) NOT NULL default ' ',
  "AAABEDVF" varchar(20) NOT NULL default ' ',
  "AAABEDVG" tinyint(3) unsigned NOT NULL default '0',
  "AAABGIKK" tinyint(3) unsigned NOT NULL default '0',
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEDVJ" varchar(8) NOT NULL default '00000000',
  "AAABEDVK" varchar(6) NOT NULL default '000000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDVN" (
  "AAABLXIY" varchar(12) NOT NULL default ' ',
  "AAABLKMM" varchar(8) NOT NULL default '00000000',
  "AAABEDVQ" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABLXIY"),
  KEY "AAABEDVS" ("AAABLKMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDVU" (
  "AAABJYKA" varchar(1) NOT NULL default ' ',
  "AAABEDVW" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABJYKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDVY" (
  "AAABHEGP" varchar(10) NOT NULL default ' ',
  "AAABGKLB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABHEGP","AAABGKLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDWD" (
  "AAABEDWL" varchar(13) NOT NULL default ' ',
  "AAABEDWF" varchar(32) NOT NULL default ' ',
  "AAABEDWG" varchar(1) NOT NULL default ' ',
  "AAABEDWH" varchar(1) NOT NULL default ' ',
  "AAABEDWI" varchar(1) NOT NULL default ' ',
  "AAABEDWJ" varchar(1) NOT NULL default ' ',
  "AAABEDWK" int(11) NOT NULL default '0',
  PRIMARY KEY ("AAABEDWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDWM" (
  "AAABEDWX" varchar(13) NOT NULL default ' ',
  "AAABEDYI" varchar(13) NOT NULL default ' ',
  "AAABEDYJ" varchar(7) NOT NULL default ' ',
  "AAABEDWQ" varchar(8) NOT NULL default '00000000',
  "AAABEDWR" varchar(6) NOT NULL default '000000',
  "AAABEDXI" varchar(8) NOT NULL default '00000000',
  "AAABEDXJ" varchar(6) NOT NULL default '000000',
  "AAABEDXM" varchar(12) NOT NULL default ' ',
  "AAABEDWV" varchar(4) NOT NULL default '0000',
  "AAABEDWW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDWX","AAABEDYI","AAABEDYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDXA" (
  "AAABEDYI" varchar(13) NOT NULL default ' ',
  "AAABEDYJ" varchar(7) NOT NULL default ' ',
  "AAABEDXW" varchar(3) NOT NULL default ' ',
  "AAABEDXX" varchar(20) NOT NULL default ' ',
  "AAABEDXY" varchar(80) NOT NULL default ' ',
  "AAABEDXZ" varchar(255) NOT NULL default ' ',
  "AAABLEMH" int(11) NOT NULL default '0',
  "AAABEDXI" varchar(8) NOT NULL default '00000000',
  "AAABEDXJ" varchar(6) NOT NULL default '000000',
  "AAABEDXK" varchar(1) NOT NULL default ' ',
  "AAABEDXL" varchar(13) NOT NULL default ' ',
  "AAABEDXM" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDYI","AAABEDYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDXP" (
  "AAABEDYI" varchar(13) NOT NULL default ' ',
  "AAABEDYJ" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDYI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDXT" (
  "AAABEDYI" varchar(13) NOT NULL default ' ',
  "AAABEDYJ" varchar(7) NOT NULL default ' ',
  "AAABEDXW" varchar(3) NOT NULL default ' ',
  "AAABEDXX" varchar(20) NOT NULL default ' ',
  "AAABEDXY" varchar(80) NOT NULL default ' ',
  "AAABEDXZ" varchar(255) NOT NULL default ' ',
  "AAABLEMH" int(11) NOT NULL default '0',
  "AAABEDYB" varchar(8) NOT NULL default '00000000',
  "AAABEDYC" varchar(6) NOT NULL default '000000',
  "AAABEDYD" varchar(8) NOT NULL default '00000000',
  "AAABEDYE" varchar(6) NOT NULL default '000000',
  "AAABEDYF" varchar(8) NOT NULL default '00000000',
  "AAABEDYG" varchar(12) NOT NULL default ' ',
  "AAABEDYH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDYI","AAABEDYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDYK" (
  "AAABEDYT" varchar(30) NOT NULL default ' ',
  "AAABEDYM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEDYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDYO" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEDYT" varchar(30) NOT NULL default ' ',
  "AAABJDMJ" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABEDYT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDYU" (
  "AAABEDYX" varchar(35) NOT NULL default ' ',
  "AAABEDZZ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABEDYX","AAABEDZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDYZ" (
  "AAABEEFC" varchar(60) NOT NULL default ' ',
  "AAABEEFQ" varchar(5) NOT NULL default '00000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABEEFC","AAABEEFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDZG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEDZL" varchar(15) NOT NULL default ' ',
  "AAABEDZM" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEDZL","AAABEDZM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDZN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEEFC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEEFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDZS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEEFC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEEFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEDZX" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABEDZZ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABIRUN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEAB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHBRF" varchar(60) NOT NULL default ' ',
  "AAABHJRW" varchar(5) NOT NULL default '00000',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMRHS","AAABHBRF","AAABHJRW","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEAN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHBRF" varchar(60) NOT NULL default ' ',
  "AAABKNGX" varchar(1) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMRHS","AAABHBRF","AAABKNGX","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEAZ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHBRF" varchar(60) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMRHS","AAABHBRF","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEERB" varchar(2) NOT NULL default '00',
  "AAABEEBM" varchar(8) NOT NULL default ' ',
  "AAABEEBN" varchar(8) NOT NULL default ' ',
  "AAABEEBO" varchar(8) NOT NULL default ' ',
  "AAABEEBP" varchar(8) NOT NULL default ' ',
  "AAABEEBQ" varchar(8) NOT NULL default ' ',
  "AAABEEBR" varchar(8) NOT NULL default ' ',
  "AAABIDTM" varchar(1) NOT NULL default '0',
  "AAABEEBT" varchar(8) NOT NULL default ' ',
  "AAABEEBU" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEERB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEECI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEECE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLXDN" varchar(4) NOT NULL default ' ',
  "AAABEECI" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLXDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABHBRF" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(80) NOT NULL default ' ',
  "AAABLUMP" tinyint(3) unsigned NOT NULL default '0',
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
CREATE TABLE "AAABEEDC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEEFQ" varchar(5) NOT NULL default '00000',
  "AAABEEDF" varchar(12) NOT NULL default ' ',
  "AAABEEDG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEEFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEDJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEEFC" varchar(60) NOT NULL default ' ',
  "AAABEEEJ" varchar(12) NOT NULL default ' ',
  "AAABEEEK" varchar(8) NOT NULL default '00000000',
  "AAABGOGT" varchar(12) NOT NULL default ' ',
  "AAABEUQX" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRHS","AAABEEFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEDS" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEEFC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEEFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEDX" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEEEB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEEEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEEC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEEFQ" varchar(5) NOT NULL default '00000',
  "AAABEEFC" varchar(60) NOT NULL default ' ',
  "AAABMMZL" varchar(1) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABEEEW" varchar(10) NOT NULL default '0000000000',
  "AAABEEEJ" varchar(12) NOT NULL default ' ',
  "AAABEEEK" varchar(8) NOT NULL default '00000000',
  "AAABGOGT" varchar(12) NOT NULL default ' ',
  "AAABEUQX" varchar(8) NOT NULL default '00000000',
  "AAABEEEN" varchar(1) NOT NULL default ' ',
  "AAABEEEO" varchar(1) NOT NULL default ' ',
  "AAABEEEP" varchar(4) NOT NULL default ' ',
  "AAABEEEQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEEFQ","AAABEEFC"),
  KEY "AAABEEEU" ("AAABEEFQ","AAABEEEW","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEEY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEEFI" varchar(10) NOT NULL default '0000000000',
  "AAABEEFJ" varchar(5) NOT NULL default '00000',
  "AAABEEFC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEEFI","AAABEEFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEFG" (
  "AAABEEFJ" varchar(5) NOT NULL default '00000',
  "AAABEEFI" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABEEFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEFK" (
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABEEFQ" varchar(5) NOT NULL default '00000',
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEEFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMDEN","AAABEEFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLTJH" varchar(32) NOT NULL default ' ',
  "AAABMLPO" varchar(10) NOT NULL default '0000000000',
  "AAABJSYW" smallint(6) NOT NULL default '0',
  "AAABLSBY" text,
  PRIMARY KEY ("AAABMRIL","AAABLTJH","AAABMLPO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZRG" varchar(13) NOT NULL default ' ',
  "AAABMCXC" varchar(10) NOT NULL default ' ',
  "AAABEERB" varchar(2) NOT NULL default '00',
  "AAABEEOW" varchar(13) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLAYB" decimal(15,5) NOT NULL default '0.00000',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABEEPE" varchar(1) NOT NULL default '0',
  "AAABEEPF" varchar(1) NOT NULL default '0',
  "AAABEEPG" varchar(1) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABLAGK" varchar(13) NOT NULL default ' ',
  "AAABLAGL" varchar(13) NOT NULL default ' ',
  "AAABEEPR" varchar(10) NOT NULL default ' ',
  "AAABEEPS" decimal(15,6) NOT NULL default '0.000000',
  "AAABEEHI" decimal(15,6) NOT NULL default '0.000000',
  "AAABEEHJ" decimal(15,6) NOT NULL default '0.000000',
  "AAABEEHK" decimal(15,6) NOT NULL default '0.000000',
  "AAABEEHL" varchar(5) NOT NULL default ' ',
  "AAABEEHM" varchar(5) NOT NULL default ' ',
  "AAABEEHN" varchar(5) NOT NULL default ' ',
  "AAABEEPZ" decimal(9,5) NOT NULL default '0.00000',
  "AAABEEQA" decimal(9,5) NOT NULL default '0.00000',
  "AAABEEQB" decimal(9,5) NOT NULL default '0.00000',
  "AAABEEQC" varchar(5) NOT NULL default ' ',
  "AAABEEQD" varchar(5) NOT NULL default ' ',
  "AAABEEQE" varchar(5) NOT NULL default ' ',
  "AAABEEQF" varchar(5) NOT NULL default ' ',
  "AAABEEQG" varchar(5) NOT NULL default ' ',
  "AAABEEQH" varchar(5) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZRG","AAABMCXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABLAYL" varchar(10) NOT NULL default ' ',
  "AAABKZRG" varchar(13) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABMEAW" varchar(4) NOT NULL default '0000',
  "AAABKZSY" varchar(1) NOT NULL default ' ',
  "AAABKZSZ" varchar(1) NOT NULL default ' ',
  "AAABLAPD" varchar(1) NOT NULL default ' ',
  "AAABLAPE" varchar(2) NOT NULL default ' ',
  "AAABLABT" varchar(18) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABLDCD" varchar(4) NOT NULL default ' ',
  "AAABLAXH" varchar(4) NOT NULL default ' ',
  "AAABLAAD" varchar(1) NOT NULL default ' ',
  "AAABLAXJ" varchar(2) NOT NULL default '00',
  "AAABMDIJ" varchar(8) NOT NULL default '00000000',
  "AAABLAAV" varchar(8) NOT NULL default '00000000',
  "AAABMDZT" varchar(8) NOT NULL default '00000000',
  "AAABMEAA" varchar(8) NOT NULL default '00000000',
  "AAABLAYB" decimal(15,5) NOT NULL default '0.00000',
  "AAABMDHS" decimal(13,2) NOT NULL default '0.00',
  "AAABLAYD" varchar(5) NOT NULL default ' ',
  "AAABLAAO" decimal(13,2) NOT NULL default '0.00',
  "AAABLAAI" varchar(5) NOT NULL default ' ',
  "AAABMDHR" decimal(13,2) NOT NULL default '0.00',
  "AAABLBYH" varchar(5) NOT NULL default ' ',
  "AAABLBXX" decimal(13,2) NOT NULL default '0.00',
  "AAABKZUA" varchar(5) NOT NULL default ' ',
  "AAABLCXB" decimal(9,5) NOT NULL default '0.00000',
  "AAABMCNY" decimal(9,5) NOT NULL default '0.00000',
  "AAABKZUD" varchar(10) NOT NULL default ' ',
  "AAABKZUE" varchar(10) NOT NULL default ' ',
  "AAABKZUF" varchar(10) NOT NULL default ' ',
  "AAABKZUG" varchar(10) NOT NULL default ' ',
  "AAABLAAW" varchar(8) NOT NULL default '00000000',
  "AAABKZUI" varchar(8) NOT NULL default '00000000',
  "AAABKZZC" varchar(1) NOT NULL default '0',
  "AAABKZUK" varchar(8) NOT NULL default '00000000',
  "AAABKZUL" varchar(8) NOT NULL default '00000000',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABKZZD" varchar(1) NOT NULL default ' ',
  "AAABKZUO" varchar(8) NOT NULL default '00000000',
  "AAABLACY" varchar(5) NOT NULL default ' ',
  "AAABLACZ" varchar(5) NOT NULL default ' ',
  "AAABEEKJ" double NOT NULL default '0',
  "AAABEEKK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABLAYL","AAABKZRG","AAABLCIO","AAABLCIP","AAABMEAW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEEOW" varchar(13) NOT NULL default ' ',
  "AAABEERB" varchar(2) NOT NULL default '00',
  "AAABEEMG" varchar(1) NOT NULL default ' ',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLEQN" varchar(8) NOT NULL default '00000000',
  "AAABEEKZ" varchar(1) NOT NULL default '0',
  "AAABEEMH" varchar(13) NOT NULL default ' ',
  "AAABEELB" varchar(13) NOT NULL default ' ',
  "AAABEEPF" varchar(1) NOT NULL default '0',
  "AAABEEPE" varchar(1) NOT NULL default '0',
  "AAABFYUL" decimal(13,2) NOT NULL default '0.00',
  "AAABMJUP" varchar(5) NOT NULL default ' ',
  "AAABEELG" varchar(10) NOT NULL default ' ',
  "AAABEELH" decimal(8,3) NOT NULL default '0.000',
  "AAABEELI" decimal(13,2) NOT NULL default '0.00',
  "AAABMJUR" varchar(5) NOT NULL default ' ',
  "AAABEELK" decimal(10,7) NOT NULL default '0.0000000',
  "AAABEELL" decimal(9,4) NOT NULL default '0.0000',
  "AAABEELM" decimal(9,4) NOT NULL default '0.0000',
  "AAABLFZO" varchar(40) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEEOW"),
  KEY "AAABEELY" ("AAABMRIL","AAABEEOW","AAABEERB","AAABEEMG"),
  KEY "AAABEEMD" ("AAABMRIL","AAABEEOW","AAABEEMG","AAABEEMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEMI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEEOW" varchar(13) NOT NULL default ' ',
  "AAABMLTD" varchar(8) NOT NULL default '00000000',
  "AAABKZYR" varchar(13) NOT NULL default ' ',
  "AAABMEFT" varchar(13) NOT NULL default ' ',
  "AAABKZYV" varchar(1) NOT NULL default ' ',
  "AAABEEMP" decimal(17,5) NOT NULL default '0.00000',
  "AAABEEMQ" decimal(17,5) NOT NULL default '0.00000',
  "AAABEEMR" decimal(17,5) NOT NULL default '0.00000',
  "AAABEEMS" decimal(17,5) NOT NULL default '0.00000',
  "AAABMNSG" varchar(5) NOT NULL default ' ',
  "AAABKZYZ" decimal(15,5) NOT NULL default '0.00000',
  "AAABEEMV" decimal(15,5) NOT NULL default '0.00000',
  "AAABKZZB" decimal(13,2) NOT NULL default '0.00',
  "AAABKZZD" varchar(1) NOT NULL default ' ',
  "AAABEEMY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEEOW","AAABMLTD"),
  KEY "AAABEENC" ("AAABMRIL","AAABMLTD","AAABKZYR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEENG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZRG" varchar(13) NOT NULL default ' ',
  "AAABKSXJ" int(11) NOT NULL default '0',
  "AAABETTM" varchar(2) NOT NULL default ' ',
  "AAABETTC" varchar(8) NOT NULL default ' ',
  "AAABGVPH" double NOT NULL default '0',
  "AAABGZIY" varchar(18) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGRGT" varchar(12) NOT NULL default ' ',
  "AAABGZEP" varchar(5) NOT NULL default ' ',
  "AAABGYZF" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEW" varchar(8) NOT NULL default '00000000',
  "AAABGZES" decimal(21,2) NOT NULL default '0.00',
  "AAABGZER" varchar(5) NOT NULL default ' ',
  "AAABGZEU" varchar(5) NOT NULL default ' ',
  "AAABGZEV" varchar(5) NOT NULL default ' ',
  "AAABGZKF" varchar(8) NOT NULL default '00000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZEL" varchar(5) NOT NULL default ' ',
  "AAABGZEM" decimal(21,2) NOT NULL default '0.00',
  "AAABJQXA" varchar(8) NOT NULL default '00000000',
  "AAABGVPT" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKZRG","AAABKSXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKZRG" varchar(13) NOT NULL default ' ',
  "AAABJGWD" varchar(4) NOT NULL default ' ',
  "AAABJGWF" varchar(13) NOT NULL default ' ',
  "AAABJGWE" varchar(10) NOT NULL default ' ',
  "AAABJMEG" varchar(4) NOT NULL default '0000',
  "AAABLIMM" varchar(10) NOT NULL default ' ',
  "AAABGRGT" varchar(12) NOT NULL default ' ',
  "AAABEERB" varchar(2) NOT NULL default '00',
  "AAABEEOW" varchar(13) NOT NULL default ' ',
  "AAABEEOX" varchar(8) NOT NULL default '00000000',
  "AAABLAXK" VARBINARY(255),
  "AAABJOTX" varchar(8) NOT NULL default '00000000',
  "AAABGXSI" varchar(6) NOT NULL default '000000',
  "AAABGZKC" decimal(22,6) NOT NULL default '0.000000',
  "AAABGZKE" decimal(21,2) NOT NULL default '0.00',
  "AAABGZKD" varchar(5) NOT NULL default ' ',
  "AAABEEPE" varchar(1) NOT NULL default '0',
  "AAABEEPF" varchar(1) NOT NULL default '0',
  "AAABEEPG" varchar(1) NOT NULL default '0',
  "AAABMRFO" varchar(1) NOT NULL default '0',
  "AAABGZIZ" varchar(8) NOT NULL default '00000000',
  "AAABEEPJ" varchar(8) NOT NULL default '00000000',
  "AAABEEPK" varchar(8) NOT NULL default '00000000',
  "AAABGWBS" varchar(2) NOT NULL default '00',
  "AAABGWBU" varchar(2) NOT NULL default '00',
  "AAABGSOH" varchar(2) NOT NULL default ' ',
  "AAABEEPO" varchar(18) NOT NULL default ' ',
  "AAABEEPP" varchar(13) NOT NULL default ' ',
  "AAABEEPQ" varchar(13) NOT NULL default ' ',
  "AAABEEPR" varchar(10) NOT NULL default ' ',
  "AAABEEPS" decimal(15,6) NOT NULL default '0.000000',
  "AAABEEPT" decimal(15,6) NOT NULL default '0.000000',
  "AAABEEPU" decimal(15,6) NOT NULL default '0.000000',
  "AAABEEPV" decimal(15,6) NOT NULL default '0.000000',
  "AAABEEPW" varchar(5) NOT NULL default ' ',
  "AAABEEPX" varchar(5) NOT NULL default ' ',
  "AAABEEPY" varchar(5) NOT NULL default ' ',
  "AAABEEPZ" decimal(9,5) NOT NULL default '0.00000',
  "AAABEEQA" decimal(9,5) NOT NULL default '0.00000',
  "AAABEEQB" decimal(9,5) NOT NULL default '0.00000',
  "AAABEEQC" varchar(5) NOT NULL default ' ',
  "AAABEEQD" varchar(5) NOT NULL default ' ',
  "AAABEEQE" varchar(5) NOT NULL default ' ',
  "AAABEEQF" varchar(5) NOT NULL default ' ',
  "AAABEEQG" varchar(5) NOT NULL default ' ',
  "AAABEEQH" varchar(5) NOT NULL default ' ',
  "AAABEEQI" varchar(1) NOT NULL default ' ',
  "AAABLZDP" varchar(12) NOT NULL default ' ',
  "AAABLCIO" varchar(8) NOT NULL default '00000000',
  "AAABLCIP" varchar(6) NOT NULL default '000000',
  "AAABLCIQ" varchar(10) NOT NULL default ' ',
  "AAABLCIS" varchar(12) NOT NULL default ' ',
  "AAABLCIT" varchar(8) NOT NULL default '00000000',
  "AAABLCIU" varchar(6) NOT NULL default '000000',
  "AAABLCIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKZRG"),
  KEY "AAABEEQT" ("AAABMRIL","AAABLAXK"(255)),
  KEY "AAABEEQW" ("AAABMRIL","AAABJGWD","AAABJGWF","AAABJGWE","AAABEERB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEERC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTU" varchar(4) NOT NULL default ' ',
  "AAABKXWS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTU","AAABKXWS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEERJ" (
  "AAABEERP" varchar(30) NOT NULL default ' ',
  "AAABLUSK" smallint(6) NOT NULL default '0',
  "AAABLUSG" varchar(14) NOT NULL default ' ',
  "AAABLUSH" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABEERP","AAABLUSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEERR" (
  "AAABMREL" varchar(10) NOT NULL default ' ',
  "AAABLUSR" varchar(110) NOT NULL default ' ',
  "AAABLUSO" smallint(6) NOT NULL default '0',
  "AAABLUSP" blob,
  PRIMARY KEY ("AAABMREL","AAABLUSR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEERY" (
  "AAABJTIO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJTIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEESB" (
  "AAABJTIO" varchar(1) NOT NULL default ' ',
  "AAABEESD" decimal(1,0) NOT NULL default '0',
  "AAABEESE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJTIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEESG" (
  "AAABJTIO" varchar(1) NOT NULL default ' ',
  "AAABEESI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJTIO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEESK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
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
  PRIMARY KEY ("AAABMRIL","AAABHZDJ","AAABHZJH"),
  KEY "AAABEETA" ("AAABMRIL","AAABHZDJ","AAABMRFO","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEETF" (
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABMMJW" varchar(30) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  "AAABMLTT" varchar(30) NOT NULL default ' ',
  "AAABEETX" varchar(1) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEETN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMQIX" varchar(6) NOT NULL default '000000',
  "AAABLTXI" varchar(4) NOT NULL default ' ',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABLUNI","AAABMMJW","AAABMLPV","AAABMLTT","AAABEETX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEETZ" (
  "AAABEEUD" varchar(1) NOT NULL default ' ',
  "AAABJTIP" varchar(1) NOT NULL default ' ',
  "AAABJTIO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEEUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEUE" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEEVC" VARBINARY(255) NOT NULL,
  "AAABEEVE" varchar(30) NOT NULL default ' ',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABEEUL" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABEEVA" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLUXQ","AAABEEVC"(255),"AAABEEVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEUR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABEEVC" VARBINARY(255) NOT NULL,
  "AAABJDPL" decimal(15,0) NOT NULL default '0',
  "AAABEEVE" varchar(30) NOT NULL default ' ',
  "AAABLARI" decimal(15,0) NOT NULL default '0',
  "AAABEEUX" varchar(12) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABGQZW" varchar(1) NOT NULL default ' ',
  "AAABEEVA" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABLUXQ","AAABEEVC"(255),"AAABJDPL","AAABEEVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEVF" (
  "AAABEEWL" varchar(4) NOT NULL default ' ',
  "AAABEEVH" varchar(1) NOT NULL default ' ',
  "AAABEEVI" varchar(1) NOT NULL default '0',
  "AAABEEVJ" varchar(4) NOT NULL default ' ',
  "AAABMGHE" varchar(4) NOT NULL default ' ',
  "AAABLMLR" varchar(2) NOT NULL default '00',
  "AAABFNHE" varchar(1) NOT NULL default ' ',
  "AAABJESP" varchar(2) NOT NULL default '00',
  "AAABHVRQ" varchar(30) NOT NULL default ' ',
  "AAABEEVP" varchar(3) NOT NULL default '000',
  "AAABEEVQ" varchar(1) NOT NULL default ' ',
  "AAABEEVR" varchar(1) NOT NULL default ' ',
  "AAABEEVS" varchar(1) NOT NULL default ' ',
  "AAABEEVT" varchar(1) NOT NULL default ' ',
  "AAABEEVU" varchar(30) NOT NULL default ' ',
  "AAABEEVV" varchar(30) NOT NULL default ' ',
  "AAABEEVW" varchar(12) NOT NULL default ' ',
  "AAABHMRA" varchar(1) NOT NULL default ' ',
  "AAABEEVY" varchar(1) NOT NULL default ' ',
  "AAABEEVZ" varchar(1) NOT NULL default ' ',
  "AAABEEWA" varchar(10) NOT NULL default '0000000000',
  "AAABEEWB" varchar(2) NOT NULL default '00',
  "AAABEEWC" varchar(1) NOT NULL default ' ',
  "AAABEEWD" varchar(4) NOT NULL default ' ',
  "AAABEEWE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEEWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEWG" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEEWL" varchar(4) NOT NULL default ' ',
  "AAABEEWJ" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABEEWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABIAXA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCTJ" varchar(32) NOT NULL default ' ',
  "AAABFKPK" varchar(10) NOT NULL default ' ',
  "AAABEEYA" varchar(8) NOT NULL default ' ',
  "AAABFMVJ" varchar(12) NOT NULL default ' ',
  "AAABFWHE" varchar(10) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEEXB" varchar(3) NOT NULL default ' ',
  "AAABEEXC" varchar(30) NOT NULL default ' ',
  "AAABEEXD" varchar(2) NOT NULL default ' ',
  "AAABEEXE" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABGCTJ","AAABFKPK","AAABEEYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFKPK" varchar(10) NOT NULL default ' ',
  "AAABEEYA" varchar(8) NOT NULL default ' ',
  "AAABLENG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFKPK","AAABEEYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEXR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABFKPK" varchar(10) NOT NULL default ' ',
  "AAABEEYA" varchar(8) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABFKPK","AAABEEYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEYB" (
  "AAABJYTM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABJYTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJYTM" varchar(40) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJYTM","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEEYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLCR" varchar(12) NOT NULL default '000000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABGLCX" varchar(3) NOT NULL default ' ',
  "AAABGLCY" varchar(4) NOT NULL default ' ',
  "AAABGLCZ" varchar(4) NOT NULL default ' ',
  "AAABEFZK" varchar(4) NOT NULL default ' ',
  "AAABKRZV" varchar(3) NOT NULL default ' ',
  "AAABEFZM" decimal(7,2) NOT NULL default '0.00',
  "AAABEFZN" decimal(9,2) NOT NULL default '0.00',
  "AAABGLDA" varchar(1) NOT NULL default ' ',
  "AAABGLDB" varchar(1) NOT NULL default ' ',
  "AAABGLEV" varchar(2) NOT NULL default ' ',
  "AAABGLEW" varchar(8) NOT NULL default '00000000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABGLBT" varchar(8) NOT NULL default '00000000',
  "AAABGLEX" varchar(20) NOT NULL default ' ',
  "AAABKOUH" varchar(40) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABKKEU" varchar(1) NOT NULL default ' ',
  "AAABEEZP" varchar(1) NOT NULL default ' ',
  "AAABEEZQ" varchar(1) NOT NULL default ' ',
  "AAABKQQE" varchar(10) NOT NULL default '0000000000',
  "AAABKQHD" tinyint(3) unsigned NOT NULL default '0',
  "AAABGLFS" varchar(3) NOT NULL default ' ',
  "AAABEEZU" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABGLBW" varchar(2) NOT NULL default ' ',
  "AAABGLBX" varchar(4) NOT NULL default '0000',
  "AAABGLBY" varchar(1) NOT NULL default ' ',
  "AAABGLBZ" decimal(13,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABGLDC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGLCR"),
  KEY "AAABEFAF" ("AAABMRIL","AAABMCRV","AAABLYZH","AAABLNYV"),
  KEY "AAABEFAK" ("AAABMRIL","AAABKQQE","AAABKQHD","AAABGLFS","AAABKKEU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFAQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLCR" varchar(12) NOT NULL default '000000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABGLDC" varchar(8) NOT NULL default '00000000',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABGLCX" varchar(3) NOT NULL default ' ',
  "AAABGLCY" varchar(4) NOT NULL default ' ',
  "AAABGLCZ" varchar(4) NOT NULL default ' ',
  "AAABGLDA" varchar(1) NOT NULL default ' ',
  "AAABGLDB" varchar(1) NOT NULL default ' ',
  "AAABGLEV" varchar(2) NOT NULL default ' ',
  "AAABGLEW" varchar(8) NOT NULL default '00000000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABGLBT" varchar(8) NOT NULL default '00000000',
  "AAABGLEX" varchar(20) NOT NULL default ' ',
  "AAABKOUH" varchar(40) NOT NULL default ' ',
  "AAABGLEZ" varchar(1) NOT NULL default ' ',
  "AAABKQQE" varchar(10) NOT NULL default '0000000000',
  "AAABKQQF" varchar(8) NOT NULL default '00000000',
  "AAABKKEU" varchar(1) NOT NULL default ' ',
  "AAABGLFD" varchar(8) NOT NULL default '00000000',
  "AAABGLFE" varchar(8) NOT NULL default '00000000',
  "AAABGLFF" varchar(20) NOT NULL default ' ',
  "AAABMQBT" varchar(8) NOT NULL default '00000000',
  "AAABGLFH" varchar(2) NOT NULL default ' ',
  "AAABGLFI" varchar(3) NOT NULL default '000',
  "AAABGLFJ" varchar(2) NOT NULL default ' ',
  "AAABHEZC" varchar(10) NOT NULL default ' ',
  "AAABGLFL" varchar(2) NOT NULL default '00',
  "AAABGLFM" varchar(4) NOT NULL default ' ',
  "AAABMCRM" varchar(12) NOT NULL default ' ',
  "AAABMGOZ" varchar(6) NOT NULL default ' ',
  "AAABLKML" varchar(4) NOT NULL default ' ',
  "AAABGLFQ" varchar(4) NOT NULL default ' ',
  "AAABKQHD" tinyint(3) unsigned NOT NULL default '0',
  "AAABGLFS" varchar(3) NOT NULL default ' ',
  "AAABGLFT" varchar(4) NOT NULL default ' ',
  "AAABHHST" varchar(8) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  "AAABGLFZ" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABMRFE" varchar(3) NOT NULL default ' ',
  "AAABKQJY" decimal(13,3) NOT NULL default '0.000',
  "AAABKQJZ" decimal(13,3) NOT NULL default '0.000',
  "AAABLZMW" varchar(4) NOT NULL default ' ',
  "AAABKQJX" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIS" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIT" varchar(3) NOT NULL default ' ',
  "AAABGLGI" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGJ" varchar(3) NOT NULL default ' ',
  "AAABGLGK" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGL" varchar(3) NOT NULL default ' ',
  "AAABGLGM" varchar(6) NOT NULL default '000000',
  "AAABGLGN" varchar(6) NOT NULL default '000000',
  "AAABKQIU" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIV" varchar(3) NOT NULL default ' ',
  "AAABGLGQ" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGR" varchar(3) NOT NULL default ' ',
  "AAABGLGS" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGT" varchar(3) NOT NULL default ' ',
  "AAABGLGU" varchar(6) NOT NULL default '000000',
  "AAABGLGV" varchar(6) NOT NULL default '000000',
  "AAABKQIW" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIX" varchar(3) NOT NULL default ' ',
  "AAABGLGY" decimal(13,3) NOT NULL default '0.000',
  "AAABGLGZ" varchar(3) NOT NULL default ' ',
  "AAABGLHA" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHB" varchar(3) NOT NULL default ' ',
  "AAABGLHC" varchar(6) NOT NULL default '000000',
  "AAABGLHD" varchar(6) NOT NULL default '000000',
  "AAABKQIY" decimal(13,3) NOT NULL default '0.000',
  "AAABKQIZ" varchar(3) NOT NULL default ' ',
  "AAABGLHG" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHH" varchar(3) NOT NULL default ' ',
  "AAABGLHI" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHJ" varchar(3) NOT NULL default ' ',
  "AAABGLHK" varchar(6) NOT NULL default '000000',
  "AAABGLHL" varchar(6) NOT NULL default '000000',
  "AAABKQJA" decimal(13,3) NOT NULL default '0.000',
  "AAABKQJB" varchar(3) NOT NULL default ' ',
  "AAABGLHO" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHP" varchar(3) NOT NULL default ' ',
  "AAABGLHQ" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHR" varchar(3) NOT NULL default ' ',
  "AAABGLHS" varchar(6) NOT NULL default '000000',
  "AAABGLHT" varchar(6) NOT NULL default '000000',
  "AAABKQJC" decimal(13,3) NOT NULL default '0.000',
  "AAABKQJD" varchar(3) NOT NULL default ' ',
  "AAABGLHW" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHX" varchar(3) NOT NULL default ' ',
  "AAABGLHY" decimal(13,3) NOT NULL default '0.000',
  "AAABGLHZ" varchar(3) NOT NULL default ' ',
  "AAABGLIA" varchar(6) NOT NULL default '000000',
  "AAABGLIB" varchar(6) NOT NULL default '000000',
  "AAABGLDX" varchar(6) NOT NULL default ' ',
  "AAABMCXI" varchar(12) NOT NULL default ' ',
  "AAABKRJY" varchar(24) NOT NULL default ' ',
  "AAABMCXJ" varchar(12) NOT NULL default ' ',
  "AAABKROQ" varchar(10) NOT NULL default ' ',
  "AAABKROR" varchar(6) NOT NULL default '000000',
  "AAABKPUB" varchar(12) NOT NULL default ' ',
  "AAABMCZF" varchar(16) NOT NULL default ' ',
  "AAABMCZG" varchar(10) NOT NULL default ' ',
  "AAABGLEG" varchar(10) NOT NULL default ' ',
  "AAABGLEH" varchar(4) NOT NULL default ' ',
  "AAABGLEI" varchar(4) NOT NULL default ' ',
  "AAABMQBQ" varchar(50) NOT NULL default ' ',
  "AAABGLEK" varchar(2) NOT NULL default ' ',
  "AAABGLEL" varchar(2) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABKRUO" varchar(20) NOT NULL default ' ',
  "AAABMCXH" varchar(14) NOT NULL default ' ',
  "AAABKRUI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLCR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABLJJO" varchar(2) NOT NULL default ' ',
  "AAABEFRD" varchar(2) NOT NULL default ' ',
  "AAABEFIN" varchar(2) NOT NULL default ' ',
  "AAABEFIO" varchar(2) NOT NULL default ' ',
  "AAABEVEY" varchar(1) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABEFGI" varchar(30) NOT NULL default ' ',
  "AAABEFGJ" varchar(1) NOT NULL default ' ',
  "AAABEFGK" varchar(1) NOT NULL default ' ',
  "AAABEFGL" varchar(1) NOT NULL default ' ',
  "AAABEFGM" varchar(1) NOT NULL default ' ',
  "AAABEFGN" varchar(1) NOT NULL default ' ',
  "AAABEFGO" varchar(1) NOT NULL default ' ',
  "AAABEFGP" varchar(1) NOT NULL default ' ',
  "AAABEFGQ" varchar(1) NOT NULL default ' ',
  "AAABEFGR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFGU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABEFIN" varchar(2) NOT NULL default ' ',
  "AAABEFIO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEFII" varchar(2) NOT NULL default ' ',
  "AAABEFHY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEFII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEFII" varchar(2) NOT NULL default ' ',
  "AAABEFIF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEFII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABEFIN" varchar(2) NOT NULL default ' ',
  "AAABEFIO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFIS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABEFJG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFJK" (
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABIZHF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABIZHF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABIZHF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLUER","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABEFTZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABEFTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABEFTZ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJBH","AAABEFTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLJJO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLHTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJBH","AAABLHTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABEFNM" varchar(4) NOT NULL default '0000',
  "AAABEFNF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLILW","AAABLTLR","AAABLHTL","AAABLHTE","AAABEFNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABEFNM" varchar(4) NOT NULL default '0000',
  "AAABEFNF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLILW","AAABLTLR","AAABLHTL","AAABLHTE","AAABEFNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABEFSK" varchar(2) NOT NULL default ' ',
  "AAABEFSL" varchar(4) NOT NULL default '0000',
  "AAABEFRU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLILW","AAABLTLR","AAABLHTL","AAABLHTE","AAABEFSK","AAABEFSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABEFSK" varchar(2) NOT NULL default ' ',
  "AAABEFSL" varchar(4) NOT NULL default '0000',
  "AAABEFSG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLILW","AAABLTLR","AAABLHTL","AAABLHTE","AAABEFSK","AAABEFSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJBH","AAABLILW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABEFUY" varchar(3) NOT NULL default '000',
  "AAABEFUS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR","AAABLHTL","AAABLHTE","AAABEFUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABEFQG" varchar(2) NOT NULL default ' ',
  "AAABEFQH" varchar(1) NOT NULL default ' ',
  "AAABEFQI" varchar(1) NOT NULL default ' ',
  "AAABLHSY" varchar(4) NOT NULL default ' ',
  "AAABEFQK" varchar(1) NOT NULL default ' ',
  "AAABFWGL" varchar(4) NOT NULL default ' ',
  "AAABLILC" varchar(1) NOT NULL default ' ',
  "AAABLILT" varchar(1) NOT NULL default ' ',
  "AAABEFQO" varchar(1) NOT NULL default ' ',
  "AAABEFQP" varchar(1) NOT NULL default ' ',
  "AAABEFQQ" varchar(1) NOT NULL default ' ',
  "AAABEFQR" varchar(1) NOT NULL default ' ',
  "AAABEFQS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABEFRD" varchar(2) NOT NULL default ' ',
  "AAABEFSU" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  "AAABEFRG" varchar(1) NOT NULL default ' ',
  "AAABEFRH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLILW","AAABLTLR","AAABLHTL","AAABLHTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABEFSK" varchar(2) NOT NULL default ' ',
  "AAABEFSL" varchar(4) NOT NULL default '0000',
  "AAABEFRU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR","AAABEFSK","AAABEFSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABEFSK" varchar(2) NOT NULL default ' ',
  "AAABEFSL" varchar(4) NOT NULL default '0000',
  "AAABEFSG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR","AAABEFSK","AAABEFSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABEVEY" varchar(1) NOT NULL default ' ',
  "AAABEFST" varchar(1) NOT NULL default ' ',
  "AAABEFSU" varchar(30) NOT NULL default ' ',
  "AAABEFSV" varchar(1) NOT NULL default ' ',
  "AAABEFSW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR","AAABLHTL","AAABLHTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLILW" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEFTK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLILW","AAABLTLR","AAABLHTL","AAABLHTE","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABEFTZ" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABEFTZ","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJBH","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default ' ',
  "AAABLHTL" varchar(4) NOT NULL default ' ',
  "AAABLHTE" varchar(4) NOT NULL default ' ',
  "AAABEFUY" varchar(3) NOT NULL default '000',
  "AAABEFUS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH","AAABLTLR","AAABLHTL","AAABLHTE","AAABEFUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEFVS" varchar(4) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABEFVD" varchar(4) NOT NULL default ' ',
  "AAABEFVE" varchar(4) NOT NULL default ' ',
  "AAABEFVF" varchar(4) NOT NULL default ' ',
  "AAABEFVG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEFVS","AAABLJBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEFVS" varchar(4) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEFVS","AAABLJBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLJBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLJBH" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLJBH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABLOZE","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUER" varchar(30) NOT NULL default ' ',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  "AAABMDEP" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUER","AAABLOZE","AAABMDEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFXA" (
  "AAABEFXP" varchar(30) NOT NULL default ' ',
  "AAABEFYT" varchar(30) NOT NULL default ' ',
  "AAABHUHO" varchar(30) NOT NULL default ' ',
  "AAABEFYV" varchar(30) NOT NULL default ' ',
  "AAABMNRG" smallint(6) NOT NULL default '0',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEFXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFXI" (
  "AAABEFXP" varchar(30) NOT NULL default ' ',
  "AAABEFYT" varchar(30) NOT NULL default ' ',
  "AAABHUHO" varchar(30) NOT NULL default ' ',
  "AAABEFYV" varchar(30) NOT NULL default ' ',
  "AAABMNRG" smallint(6) NOT NULL default '0',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEFXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFXQ" (
  "AAABGJPR" varchar(30) NOT NULL default ' ',
  "AAABEFYT" varchar(30) NOT NULL default ' ',
  "AAABHUHO" varchar(30) NOT NULL default ' ',
  "AAABEFYV" varchar(30) NOT NULL default ' ',
  "AAABMNRG" smallint(6) NOT NULL default '0',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGJPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFXY" (
  "AAABGJPR" varchar(30) NOT NULL default ' ',
  "AAABEFYT" varchar(30) NOT NULL default ' ',
  "AAABHUHO" varchar(30) NOT NULL default ' ',
  "AAABEFYV" varchar(30) NOT NULL default ' ',
  "AAABMNRG" smallint(6) NOT NULL default '0',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABGJPR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFYG" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEFYT" varchar(30) NOT NULL default ' ',
  "AAABHUHO" varchar(30) NOT NULL default ' ',
  "AAABEFYV" varchar(30) NOT NULL default ' ',
  "AAABMNRG" smallint(6) NOT NULL default '0',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFYQ" (
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEFYT" varchar(30) NOT NULL default ' ',
  "AAABHUHO" varchar(30) NOT NULL default ' ',
  "AAABEFYV" varchar(30) NOT NULL default ' ',
  "AAABMNRG" smallint(6) NOT NULL default '0',
  "AAABLOZE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEFZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLCR" varchar(12) NOT NULL default '000000000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABGLDC" varchar(8) NOT NULL default '00000000',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAABLNYV" varchar(6) NOT NULL default '000000',
  "AAABMLER" varchar(8) NOT NULL default '00000000',
  "AAABGLCW" varchar(6) NOT NULL default '000000',
  "AAABGLCY" varchar(4) NOT NULL default ' ',
  "AAABEFZK" varchar(4) NOT NULL default ' ',
  "AAABKRZV" varchar(3) NOT NULL default ' ',
  "AAABEFZM" decimal(7,2) NOT NULL default '0.00',
  "AAABEFZN" decimal(9,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEFZP" varchar(12) NOT NULL default ' ',
  "AAABLCOZ" varchar(8) NOT NULL default '00000000',
  "AAABEFZR" varchar(6) NOT NULL default '000000',
  "AAABEFZS" varchar(40) NOT NULL default ' ',
  "AAABEFZT" varchar(12) NOT NULL default ' ',
  "AAABLCPA" varchar(8) NOT NULL default '00000000',
  "AAABEFZV" varchar(6) NOT NULL default '000000',
  "AAABEFZW" varchar(40) NOT NULL default ' ',
  "AAABEFZX" varchar(1) NOT NULL default ' ',
  "AAABEFZY" varchar(12) NOT NULL default ' ',
  "AAABEFZZ" varchar(8) NOT NULL default '00000000',
  "AAABEGAA" varchar(6) NOT NULL default '000000',
  "AAABEGAB" varchar(40) NOT NULL default ' ',
  "AAABEGAO" varchar(1) NOT NULL default ' ',
  "AAABEGAD" varchar(12) NOT NULL default ' ',
  "AAABEGAE" varchar(8) NOT NULL default '00000000',
  "AAABEGAF" varchar(6) NOT NULL default '000000',
  "AAABEGAG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLCR"),
  KEY "AAABEGAJ" ("AAABMRIL","AAABMCRV"),
  KEY "AAABEGAM" ("AAABMRIL","AAABEGAO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGAP" (
  "AAABLFYC" varchar(4) NOT NULL default ' ',
  "AAABLFYD" varchar(40) NOT NULL default ' ',
  "AAABKMVW" varchar(4) NOT NULL default ' ',
  "AAABKMVX" varchar(120) NOT NULL default ' ',
  "AAABIIDV" varchar(1) NOT NULL default ' ',
  "AAABLRQP" varchar(40) NOT NULL default ' ',
  "AAABLHPU" varchar(1) NOT NULL default ' ',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABINIL" varchar(12) NOT NULL default ' ',
  "AAABLFZK" varchar(8) NOT NULL default ' ',
  "AAABLVCU" varchar(1) NOT NULL default ' ',
  "AAABEGBC" varchar(1) NOT NULL default ' ',
  "AAABJHCT" varchar(20) NOT NULL default ' ',
  "AAABFWOA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLFYC","AAABLFYD","AAABKMVW","AAABKMVX","AAABIIDV","AAABLRQP","AAABLHPU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGBM" (
  "AAABEGBQ" varchar(12) NOT NULL default ' ',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABGWQV" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABEGBQ","AAABLSCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQVJ" varchar(1) NOT NULL default '0',
  "AAABEGBV" varchar(1) NOT NULL default ' ',
  "AAABEGBW" varchar(1) NOT NULL default ' ',
  "AAABEGBX" varchar(1) NOT NULL default ' ',
  "AAABEGBY" varchar(1) NOT NULL default ' ',
  "AAABEGBZ" varchar(1) NOT NULL default ' ',
  "AAABEGCA" varchar(1) NOT NULL default '0',
  "AAABEGCB" varchar(2) NOT NULL default '00',
  "AAABEGCC" varchar(1) NOT NULL default ' ',
  "AAABEGCD" varchar(4) NOT NULL default ' ',
  "AAABEGCE" varchar(1) NOT NULL default ' ',
  "AAABEGCF" varchar(1) NOT NULL default ' ',
  "AAABEGCG" varchar(1) NOT NULL default ' ',
  "AAABEGCH" varchar(2) NOT NULL default ' ',
  "AAABEGCI" varchar(1) NOT NULL default ' ',
  "AAABEGCJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABFLEJ" varchar(34) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKIQ","AAABFLEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGCW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABFLEJ" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABLIMF","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGDH" (
  "AAABMMVH" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABEGDL" varchar(3) NOT NULL default ' ',
  "AAABEJEO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGDO" (
  "AAABMMVH" varchar(30) NOT NULL default ' ',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABEGDR" varchar(8) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMNLA" varchar(2) NOT NULL default ' ',
  "AAABEGDU" varchar(1) NOT NULL default ' ',
  "AAABHTLA" varchar(8) NOT NULL default ' ',
  "AAABEGDW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVH","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGDZ" (
  "AAABIGEG" varchar(30) NOT NULL default ' ',
  "AAABEGEB" varchar(1) NOT NULL default ' ',
  "AAABEGEC" varchar(1) NOT NULL default ' ',
  "AAABLQRH" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  "AAABEGEF" varchar(30) NOT NULL default ' ',
  "AAABEGEG" varchar(6) NOT NULL default '000000',
  "AAABEGEH" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIGEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGEJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEGEN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(36) NOT NULL default ' ',
  "AAABLFWB" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREF","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABGBTL" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABEGFM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABMQHV","AAABMQHX","AAABMQHY","AAABEPWA","AAABEKCN","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGFW" (
  "AAABEGGD" varchar(3) NOT NULL default ' ',
  "AAABEGGE" varchar(30) NOT NULL default ' ',
  "AAABMKIQ" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABEGGB" varchar(60) NOT NULL default ' ',
  "AAABEGGC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEGGD","AAABEGGE","AAABMKIQ","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABEGGL" varchar(1) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABEGGN" varchar(1) NOT NULL default ' ',
  "AAABEGJL" varchar(4) NOT NULL default ' ',
  "AAABEIWW" varchar(1) NOT NULL default ' ',
  "AAABEGGQ" varchar(1) NOT NULL default ' ',
  "AAABEGGR" varchar(1) NOT NULL default ' ',
  "AAABEGGS" varchar(1) NOT NULL default ' ',
  "AAABEGGT" varchar(1) NOT NULL default ' ',
  "AAABEGGU" varchar(1) NOT NULL default ' ',
  "AAABEGGV" varchar(1) NOT NULL default ' ',
  "AAABEGGW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABLSIN" varchar(8) NOT NULL default ' ',
  "AAABIWTN" varchar(15) NOT NULL default ' ',
  "AAABEHMZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPXB","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGHM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(1) NOT NULL default '0',
  "AAABIWTP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPXB","AAABLIMF","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGHY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEGJL" varchar(4) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEGID" varchar(1) NOT NULL default ' ',
  "AAABEGIE" varchar(1) NOT NULL default ' ',
  "AAABEGIF" varchar(1) NOT NULL default ' ',
  "AAABEGIG" varchar(1) NOT NULL default ' ',
  "AAABEGIH" varchar(1) NOT NULL default ' ',
  "AAABEGII" varchar(34) NOT NULL default ' ',
  "AAABEGIJ" varchar(10) NOT NULL default ' ',
  "AAABEGIK" varchar(34) NOT NULL default ' ',
  "AAABEGIL" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABEGJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEGJL" varchar(4) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABFNWR" varchar(1) NOT NULL default '0',
  "AAABMMJT" varchar(10) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  "AAABEGIX" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABEGJL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEGJL" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPXB","AAABEGJL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGJM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIWW" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEGKC" varchar(1) NOT NULL default ' ',
  "AAABEGKD" varchar(1) NOT NULL default ' ',
  "AAABEGLT" varchar(1) NOT NULL default ' ',
  "AAABEGKF" varchar(1) NOT NULL default ' ',
  "AAABEGKG" varchar(1) NOT NULL default ' ',
  "AAABEGKH" varchar(1) NOT NULL default ' ',
  "AAABEGKI" varchar(1) NOT NULL default '0',
  "AAABEGKJ" varchar(2) NOT NULL default '00',
  "AAABEGKK" varchar(2) NOT NULL default '00',
  "AAABEGKL" varchar(2) NOT NULL default '00',
  "AAABEGKM" varchar(2) NOT NULL default '00',
  "AAABEGKN" varchar(2) NOT NULL default '00',
  "AAABEGKO" varchar(2) NOT NULL default '00',
  "AAABEGKP" varchar(2) NOT NULL default '00',
  "AAABEGKQ" varchar(2) NOT NULL default '00',
  "AAABEGKR" varchar(2) NOT NULL default '00',
  "AAABEGKS" varchar(10) NOT NULL default ' ',
  "AAABEGKT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABFJCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  "AAABEGLB" varchar(15) NOT NULL default ' ',
  "AAABEGLC" varchar(15) NOT NULL default ' ',
  "AAABEGLD" varchar(15) NOT NULL default ' ',
  "AAABEGLE" varchar(15) NOT NULL default ' ',
  "AAABEGLF" varchar(15) NOT NULL default ' ',
  "AAABEGLG" varchar(15) NOT NULL default ' ',
  "AAABEGLH" varchar(15) NOT NULL default ' ',
  "AAABEGLI" varchar(15) NOT NULL default ' ',
  "AAABEGLJ" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFJCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEPVK" varchar(10) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABEGLS" varchar(10) NOT NULL default ' ',
  "AAABEGLT" varchar(1) NOT NULL default ' ',
  "AAABEGLU" varchar(1) NOT NULL default ' ',
  "AAABEGLV" varchar(1) NOT NULL default ' ',
  "AAABEGLW" varchar(1) NOT NULL default ' ',
  "AAABEGLX" varchar(10) NOT NULL default ' ',
  "AAABEGLY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJCO","AAABEPVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEPVK" varchar(10) NOT NULL default ' ',
  "AAABIWTN" varchar(15) NOT NULL default ' ',
  "AAABEHMZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFJCO","AAABEPVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEPVK" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(1) NOT NULL default '0',
  "AAABIWTP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABFJCO","AAABEPVK","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEGNC" varchar(2) NOT NULL default '00',
  "AAABJSBP" varchar(1) NOT NULL default ' ',
  "AAABEGNE" varchar(1) NOT NULL default ' ',
  "AAABEGTG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEPXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEGOG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABEGOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEGOG" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPXB","AAABEGOG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEGOL" varchar(10) NOT NULL default ' ',
  "AAABEGOM" varchar(3) NOT NULL default ' ',
  "AAABEGON" varchar(10) NOT NULL default ' ',
  "AAABEGOO" varchar(10) NOT NULL default ' ',
  "AAABEGOP" varchar(3) NOT NULL default ' ',
  "AAABEGOQ" varchar(10) NOT NULL default ' ',
  "AAABEGOR" varchar(10) NOT NULL default ' ',
  "AAABEGOS" varchar(3) NOT NULL default ' ',
  "AAABEGOT" varchar(10) NOT NULL default ' ',
  "AAABEGOU" varchar(10) NOT NULL default ' ',
  "AAABEGOV" varchar(3) NOT NULL default ' ',
  "AAABEGOW" varchar(10) NOT NULL default ' ',
  "AAABEGOX" varchar(10) NOT NULL default ' ',
  "AAABEGOY" varchar(3) NOT NULL default ' ',
  "AAABEGOZ" varchar(10) NOT NULL default ' ',
  "AAABEGPA" varchar(10) NOT NULL default ' ',
  "AAABEGPB" varchar(3) NOT NULL default ' ',
  "AAABEGPC" varchar(10) NOT NULL default ' ',
  "AAABEGPD" varchar(10) NOT NULL default ' ',
  "AAABEGPE" varchar(3) NOT NULL default ' ',
  "AAABEGPF" varchar(10) NOT NULL default ' ',
  "AAABEGPG" varchar(10) NOT NULL default ' ',
  "AAABEGPH" varchar(3) NOT NULL default ' ',
  "AAABEGPI" varchar(10) NOT NULL default ' ',
  "AAABEGPJ" varchar(10) NOT NULL default ' ',
  "AAABEGPK" varchar(10) NOT NULL default ' ',
  "AAABEGPL" varchar(10) NOT NULL default ' ',
  "AAABEGPM" varchar(10) NOT NULL default ' ',
  "AAABEGPN" varchar(10) NOT NULL default ' ',
  "AAABEGPO" varchar(10) NOT NULL default ' ',
  "AAABEGPP" varchar(10) NOT NULL default ' ',
  "AAABEGPQ" varchar(10) NOT NULL default ' ',
  "AAABEGPR" varchar(10) NOT NULL default ' ',
  "AAABEGPS" varchar(10) NOT NULL default ' ',
  "AAABEGPT" varchar(10) NOT NULL default ' ',
  "AAABEGPU" varchar(10) NOT NULL default ' ',
  "AAABEGPV" varchar(10) NOT NULL default ' ',
  "AAABEGPW" varchar(10) NOT NULL default ' ',
  "AAABEGPX" varchar(10) NOT NULL default ' ',
  "AAABEGPY" varchar(10) NOT NULL default ' ',
  "AAABEGPZ" varchar(10) NOT NULL default ' ',
  "AAABEGQA" varchar(10) NOT NULL default ' ',
  "AAABEGQB" varchar(10) NOT NULL default ' ',
  "AAABEGQC" varchar(10) NOT NULL default ' ',
  "AAABEGQD" varchar(10) NOT NULL default ' ',
  "AAABEGQE" varchar(10) NOT NULL default ' ',
  "AAABEGQF" varchar(3) NOT NULL default ' ',
  "AAABEGQG" varchar(10) NOT NULL default ' ',
  "AAABEGQH" varchar(10) NOT NULL default ' ',
  "AAABEGQI" varchar(10) NOT NULL default ' ',
  "AAABEGQJ" varchar(10) NOT NULL default ' ',
  "AAABEGQK" varchar(3) NOT NULL default ' ',
  "AAABEGQL" varchar(10) NOT NULL default ' ',
  "AAABEGQM" varchar(10) NOT NULL default ' ',
  "AAABEGQN" varchar(3) NOT NULL default ' ',
  "AAABEGQO" varchar(10) NOT NULL default ' ',
  "AAABEGQP" varchar(10) NOT NULL default ' ',
  "AAABEGQQ" varchar(3) NOT NULL default ' ',
  "AAABEGQR" varchar(10) NOT NULL default ' ',
  "AAABEGQS" varchar(10) NOT NULL default ' ',
  "AAABEGQT" varchar(3) NOT NULL default ' ',
  "AAABEGQU" varchar(10) NOT NULL default ' ',
  "AAABEGQV" varchar(10) NOT NULL default ' ',
  "AAABEGQW" varchar(3) NOT NULL default ' ',
  "AAABEGQX" varchar(10) NOT NULL default ' ',
  "AAABEGQY" varchar(10) NOT NULL default ' ',
  "AAABEGQZ" varchar(3) NOT NULL default ' ',
  "AAABEGRA" varchar(10) NOT NULL default ' ',
  "AAABEGRB" varchar(10) NOT NULL default ' ',
  "AAABEGRC" varchar(10) NOT NULL default ' ',
  "AAABEGRD" varchar(10) NOT NULL default ' ',
  "AAABEGRE" varchar(10) NOT NULL default ' ',
  "AAABEGRF" varchar(10) NOT NULL default ' ',
  "AAABEGRG" varchar(10) NOT NULL default ' ',
  "AAABEGRH" varchar(10) NOT NULL default ' ',
  "AAABEGRI" varchar(3) NOT NULL default ' ',
  "AAABEGRJ" varchar(10) NOT NULL default ' ',
  "AAABEGRK" varchar(10) NOT NULL default ' ',
  "AAABEGRL" varchar(3) NOT NULL default ' ',
  "AAABEGRM" varchar(10) NOT NULL default ' ',
  "AAABEGRN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABEPXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABELHG" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELFZ" varchar(3) NOT NULL default ' ',
  "AAABEJEO" varchar(2) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(36) NOT NULL default ' ',
  "AAABLEPE" varchar(36) NOT NULL default ' ',
  "AAABHTLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABELHG","AAABEPXB","AAABELFZ","AAABEJEO","AAABEPVW","AAABMQBR","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEGZX" varchar(1) NOT NULL default ' ',
  "AAABEGSP" varchar(2) NOT NULL default '00',
  "AAABEGSQ" varchar(2) NOT NULL default '00',
  "AAABEGSR" varchar(1) NOT NULL default ' ',
  "AAABEGSS" varchar(1) NOT NULL default ' ',
  "AAABEGST" varchar(1) NOT NULL default ' ',
  "AAABEGSU" varchar(2) NOT NULL default ' ',
  "AAABEGSV" varchar(3) NOT NULL default ' ',
  "AAABEGSW" varchar(1) NOT NULL default ' ',
  "AAABEGSX" varchar(1) NOT NULL default '0',
  "AAABEGSY" varchar(1) NOT NULL default ' ',
  "AAABEGSZ" varchar(1) NOT NULL default ' ',
  "AAABEGTA" varchar(1) NOT NULL default ' ',
  "AAABEGTB" varchar(1) NOT NULL default '0',
  "AAABEGTC" varchar(1) NOT NULL default ' ',
  "AAABEGTD" varchar(1) NOT NULL default ' ',
  "AAABEGTE" varchar(1) NOT NULL default ' ',
  "AAABEGTF" varchar(4) NOT NULL default '0000',
  "AAABEGTG" varchar(1) NOT NULL default '0',
  "AAABEGTH" varchar(1) NOT NULL default ' ',
  "AAABEGTI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABIWTN" varchar(15) NOT NULL default ' ',
  "AAABEHMZ" varchar(30) NOT NULL default ' ',
  "AAABEGTR" varchar(25) NOT NULL default ' ',
  "AAABEGTS" varchar(25) NOT NULL default ' ',
  "AAABEGTT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGTX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEGUA" varchar(1) NOT NULL default ' ',
  "AAABEGUB" varchar(1) NOT NULL default ' ',
  "AAABEGUC" varchar(1) NOT NULL default ' ',
  "AAABEGUD" varchar(1) NOT NULL default ' ',
  "AAABEGUE" varchar(1) NOT NULL default ' ',
  "AAABEGUF" varchar(1) NOT NULL default ' ',
  "AAABEGUG" varchar(1) NOT NULL default ' ',
  "AAABEGUH" varchar(1) NOT NULL default ' ',
  "AAABEGUI" varchar(1) NOT NULL default ' ',
  "AAABEGUJ" varchar(1) NOT NULL default ' ',
  "AAABEGUK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEGUQ" varchar(1) NOT NULL default ' ',
  "AAABEGUR" varchar(1) NOT NULL default ' ',
  "AAABEGUS" varchar(1) NOT NULL default ' ',
  "AAABEGUT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEGVB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEIKG" varchar(4) NOT NULL default ' ',
  "AAABEHEZ" varchar(5) NOT NULL default ' ',
  "AAABEHFA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABMQHY","AAABEPWA","AAABEIKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGVV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEOYA" varchar(1) NOT NULL default ' ',
  "AAABJFGP" varchar(2) NOT NULL default ' ',
  "AAABEHCE" varchar(1) NOT NULL default ' ',
  "AAABEGWC" varchar(10) NOT NULL default ' ',
  "AAABEIVQ" varchar(1) NOT NULL default ' ',
  "AAABEGWE" varchar(1) NOT NULL default ' ',
  "AAABEGWF" varchar(30) NOT NULL default ' ',
  "AAABEGWG" varchar(30) NOT NULL default ' ',
  "AAABEGWH" varchar(1) NOT NULL default ' ',
  "AAABEGWI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABIWTN" varchar(15) NOT NULL default ' ',
  "AAABEHMZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABLZCP" varchar(50) NOT NULL default ' ',
  "AAABLZCR" varchar(50) NOT NULL default ' ',
  "AAABEGXD" varchar(50) NOT NULL default ' ',
  "AAABEGXE" varchar(10) NOT NULL default ' ',
  "AAABJSTP" varchar(30) NOT NULL default ' ',
  "AAABEGXG" varchar(10) NOT NULL default ' ',
  "AAABEGXH" varchar(10) NOT NULL default ' ',
  "AAABEGXI" varchar(15) NOT NULL default ' ',
  "AAABEGXJ" varchar(15) NOT NULL default ' ',
  "AAABEGXK" varchar(30) NOT NULL default ' ',
  "AAABHTLE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUS","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABEIKF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUS","AAABMQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABIFOV" varchar(1) NOT NULL default ' ',
  "AAABEGYT" varchar(5) NOT NULL default ' ',
  "AAABKTAF" varchar(60) NOT NULL default ' ',
  "AAABIIVE" varchar(1) NOT NULL default ' ',
  "AAABKRXK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGZB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEIKG" varchar(4) NOT NULL default ' ',
  "AAABEGZH" varchar(7) NOT NULL default ' ',
  "AAABEGZI" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUS","AAABEIKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEGZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEITU" varchar(5) NOT NULL default ' ',
  "AAABEHAW" decimal(7,4) NOT NULL default '0.0000',
  "AAABEGZX" varchar(1) NOT NULL default ' ',
  "AAABEGZY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUS","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEHAW" decimal(7,4) NOT NULL default '0.0000',
  "AAABEHAX" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUS","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEHCA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEHCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEHCA" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEHCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEHCA" varchar(5) NOT NULL default ' ',
  "AAABMLPV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUS","AAABEHCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGP" varchar(2) NOT NULL default ' ',
  "AAABEHCE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFGP" varchar(2) NOT NULL default ' ',
  "AAABIWTN" varchar(15) NOT NULL default ' ',
  "AAABEHMZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFGP" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(1) NOT NULL default '0',
  "AAABIWTP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFGP","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEHLM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABIWTN" varchar(15) NOT NULL default ' ',
  "AAABEHMZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEPUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEIKG" varchar(4) NOT NULL default ' ',
  "AAABEHEJ" varchar(1) NOT NULL default ' ',
  "AAABEHEK" varchar(7) NOT NULL default ' ',
  "AAABEHKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUY","AAABEIKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHER" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEIKG" varchar(4) NOT NULL default ' ',
  "AAABEHEZ" varchar(5) NOT NULL default ' ',
  "AAABEHFA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABMQHY","AAABEPWA","AAABEPUY","AAABEIKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUY","AAABMQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEHGK" varchar(1) NOT NULL default ' ',
  "AAABFLEJ" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUY","AAABMQHY","AAABEPWA","AAABEHGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEIKG" varchar(4) NOT NULL default ' ',
  "AAABEJDR" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEPUY","AAABMQHY","AAABEPWA","AAABEIKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHHB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKFS" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEHHJ" varchar(4) NOT NULL default '0000',
  "AAABJHLY" varchar(3) NOT NULL default '000',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAABEHHM" varchar(3) NOT NULL default '000',
  "AAABEHHN" varchar(1) NOT NULL default ' ',
  "AAABEHHO" varchar(1) NOT NULL default ' ',
  "AAABEHHP" varchar(5) NOT NULL default '00000',
  "AAABEHHQ" varchar(4) NOT NULL default '0000',
  "AAABEHHR" varchar(3) NOT NULL default '000',
  "AAABEHHS" varchar(1) NOT NULL default ' ',
  "AAABEHHT" varchar(1) NOT NULL default ' ',
  "AAABEHHU" varchar(1) NOT NULL default ' ',
  "AAABEHHV" varchar(1) NOT NULL default ' ',
  "AAABEHHW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKFS","AAABEPUY","AAABEPUS","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEILD" varchar(2) NOT NULL default ' ',
  "AAABEHII" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEILD" varchar(2) NOT NULL default ' ',
  "AAABIWTN" varchar(15) NOT NULL default ' ',
  "AAABEHMZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEILD" varchar(2) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEMFA" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEILD","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHJJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEILD" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEHJP" varchar(1) NOT NULL default ' ',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  "AAABEHJR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEILD" varchar(2) NOT NULL default ' ',
  "AAABEIKG" varchar(4) NOT NULL default ' ',
  "AAABEHKC" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEILD","AAABEIKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEHLM" varchar(2) NOT NULL default ' ',
  "AAABEHKM" varchar(19) NOT NULL default ' ',
  "AAABLADN" varchar(18) NOT NULL default ' ',
  "AAABEHKO" varchar(30) NOT NULL default ' ',
  "AAABEHKP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEHLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEHLM" varchar(2) NOT NULL default ' ',
  "AAABIWTN" varchar(15) NOT NULL default ' ',
  "AAABEHMZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEHLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEILD" varchar(2) NOT NULL default ' ',
  "AAABEHLM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEILD","AAABEHLM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEHLU" varchar(2) NOT NULL default ' ',
  "AAABEHLR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEHLU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEHNL" varchar(3) NOT NULL default ' ',
  "AAABEHLY" varchar(1) NOT NULL default '0',
  "AAABEHLZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEHNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEHNL" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEHNL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEHNL" varchar(3) NOT NULL default ' ',
  "AAABEHNM" varchar(9) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABEHMP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEHNL","AAABEHNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEHNL" varchar(3) NOT NULL default ' ',
  "AAABEHNM" varchar(9) NOT NULL default ' ',
  "AAABIWTN" varchar(15) NOT NULL default ' ',
  "AAABEHMZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEHNL","AAABEHNM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEHNL" varchar(3) NOT NULL default ' ',
  "AAABEHNM" varchar(9) NOT NULL default ' ',
  "AAABEHNN" varchar(3) NOT NULL default ' ',
  "AAABEHNO" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEHNL","AAABEHNM","AAABEHNN","AAABEHNO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHNP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEKFS" varchar(3) NOT NULL default ' ',
  "AAABEIJV" varchar(3) NOT NULL default ' ',
  "AAABEHQM" varchar(3) NOT NULL default ' ',
  "AAABEIVF" varchar(3) NOT NULL default ' ',
  "AAABEHRM" varchar(3) NOT NULL default ' ',
  "AAABELHG" varchar(3) NOT NULL default ' ',
  "AAABEHSM" varchar(3) NOT NULL default ' ',
  "AAABEKDE" varchar(3) NOT NULL default ' ',
  "AAABEKFR" varchar(3) NOT NULL default ' ',
  "AAABEKGW" varchar(3) NOT NULL default ' ',
  "AAABEKKF" varchar(3) NOT NULL default ' ',
  "AAABEKPH" varchar(3) NOT NULL default ' ',
  "AAABEHUZ" varchar(3) NOT NULL default ' ',
  "AAABEKXU" varchar(3) NOT NULL default ' ',
  "AAABEJMA" varchar(3) NOT NULL default ' ',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  "AAABEHYH" varchar(3) NOT NULL default ' ',
  "AAABEHOJ" varchar(3) NOT NULL default ' ',
  "AAABEHOK" varchar(1) NOT NULL default ' ',
  "AAABEHOL" varchar(1) NOT NULL default ' ',
  "AAABEHOM" varchar(1) NOT NULL default ' ',
  "AAABEHON" varchar(1) NOT NULL default ' ',
  "AAABEHOO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEKFS" varchar(3) NOT NULL default ' ',
  "AAABEHPC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEKFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEKFS" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEKFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIJV" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIJV" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIJV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEHQM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEHQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEHQM" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEHQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIVF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIVF" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEHRM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEHRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEHRM" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEHRM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABELHG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABELHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHRS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABELHG" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABELHG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEHSM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEHSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHSF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEHSM" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEHSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHSN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEKDE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEKDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEKDE" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEKDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEKFR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEKFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEKFR" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEKFR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEKGW" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEKGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEKGW" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEKGW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEKPH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEKPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEKPH" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEKPH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEHUZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEHUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEHUZ" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEHUZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEKXU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEKXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEKXU" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEKXU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEKKF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEKKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEKKF" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEKKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHWA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEJEG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKCN","AAABEJEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEJEG" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEKCN","AAABEJEG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEJMA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEJMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHXA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEJMA" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEJMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHXI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEJVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHXN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEJVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHXV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEHYH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEHYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEHYH" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEHYH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEJKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEJKH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEHYZ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABEJKH","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIAB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHZI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIAB" varchar(3) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIAB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIAB" varchar(3) NOT NULL default ' ',
  "AAABEHZX" varchar(2) NOT NULL default '00',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIAB","AAABEHZX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEHZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEIAB" varchar(3) NOT NULL default ' ',
  "AAABEIAC" varchar(4) NOT NULL default '0000',
  "AAABEIAD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIAG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEIAO" varchar(12) NOT NULL default ' ',
  "AAABLCOZ" varchar(8) NOT NULL default '00000000',
  "AAABEIAQ" varchar(6) NOT NULL default '000000',
  "AAABLIVG" varchar(1) NOT NULL default ' ',
  "AAABEIAS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABMQHX","AAABMQHY","AAABEPWA","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABMQHX","AAABMQHY","AAABEPWA","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABKWQO" varchar(4) NOT NULL default '0000',
  "AAABLGXX" varchar(4) NOT NULL default '0000',
  "AAABGBTL" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABMQHX","AAABMQHY","AAABEPWA","AAABEKCN","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEICN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEIJH" varchar(12) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABGBTL" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABMQHY","AAABEPWA","AAABEIJH","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABMQHX","AAABMQHY","AAABEPWA","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABKWQO" varchar(4) NOT NULL default '0000',
  "AAABLGXX" varchar(4) NOT NULL default '0000',
  "AAABGBTL" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABMQHX","AAABMQHY","AAABEPWA","AAABEKCN","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABMADG" varchar(14) NOT NULL default ' ',
  "AAABEIFB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABMQHY","AAABEPWA","AAABEKCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABLWKM" varchar(1) NOT NULL default ' ',
  "AAABEIFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEKCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEIVG" varchar(1) NOT NULL default ' ',
  "AAABEIFY" varchar(1) NOT NULL default ' ',
  "AAABEIFZ" varchar(1) NOT NULL default ' ',
  "AAABEIGA" varchar(1) NOT NULL default ' ',
  "AAABEIGB" varchar(1) NOT NULL default ' ',
  "AAABEIGC" varchar(1) NOT NULL default ' ',
  "AAABEIGD" varchar(1) NOT NULL default ' ',
  "AAABEIGE" varchar(1) NOT NULL default ' ',
  "AAABEIGF" varchar(1) NOT NULL default ' ',
  "AAABEIGG" varchar(1) NOT NULL default ' ',
  "AAABEIGH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABFRWJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUY","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABFRWJ" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUS","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEIHN" varchar(10) NOT NULL default ' ',
  "AAABEIHO" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEIHN","AAABEIHO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIKC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIHU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIKC" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIKC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIKF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIIH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIKF" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIKG" varchar(4) NOT NULL default ' ',
  "AAABEIIS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIIV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIKG" varchar(4) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIKC" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABEIJH" varchar(12) NOT NULL default ' ',
  "AAABHTUC" varchar(1) NOT NULL default ' ',
  "AAABEIJJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIKC","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIJV" varchar(3) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEIJR" varchar(1) NOT NULL default ' ',
  "AAABIIXH" varchar(1) NOT NULL default ' ',
  "AAABEIJT" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEIJV","AAABMPWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIJX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEIKF" varchar(1) NOT NULL default ' ',
  "AAABEIKG" varchar(4) NOT NULL default ' ',
  "AAABEIKC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABEIKF","AAABEIKG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIKH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEIKL" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIKP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEILB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEILD" varchar(2) NOT NULL default ' ',
  "AAABEMFA" varchar(18) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUY","AAABEILB","AAABMQHX","AAABEILD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEILE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEIMI" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  "AAABIZQZ" varchar(8) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUY","AAABEIMI","AAABMQHX","AAABEPXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEILU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEIMI" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEIMK" varchar(18) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUY","AAABEIMI","AAABMQHX","AAABEIMK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEIMP" varchar(2) NOT NULL default ' ',
  "AAABEIMQ" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIGO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEINC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJEVU" varchar(11) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJEVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEINK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEINQ" varchar(11) NOT NULL default ' ',
  "AAABEINR" varchar(11) NOT NULL default ' ',
  "AAABEINO" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEINQ","AAABEINR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEINS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJEVU" varchar(11) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(36) NOT NULL default ' ',
  "AAABLEPE" varchar(36) NOT NULL default ' ',
  "AAABEINZ" varchar(30) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  "AAABJDFU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJEVU","AAABLQVJ","AAABMREF","AAABMMJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABEIPA" varchar(3) NOT NULL default ' ',
  "AAABEIPB" varchar(3) NOT NULL default '000',
  "AAABEIOM" varchar(3) NOT NULL default '000',
  "AAABEION" varchar(3) NOT NULL default '000',
  "AAABMLDX" varchar(3) NOT NULL default '000',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABMJCM" varchar(2) NOT NULL default ' ',
  "AAABEIOR" varchar(30) NOT NULL default ' ',
  "AAABEIOS" varchar(1) NOT NULL default ' ',
  "AAABJEVU" varchar(11) NOT NULL default ' ',
  "AAABJETU" decimal(17,3) NOT NULL default '0.000',
  "AAABEIOV" varchar(30) NOT NULL default ' ',
  "AAABEIOW" varchar(30) NOT NULL default ' ',
  "AAABEIOX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIGO","AAABEIPA","AAABEIPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIQJ" varchar(5) NOT NULL default ' ',
  "AAABEIPF" varchar(2) NOT NULL default '00',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEIPH" varchar(1) NOT NULL default ' ',
  "AAABEIPI" varchar(1) NOT NULL default ' ',
  "AAABGIEM" varchar(1) NOT NULL default ' ',
  "AAABEIPK" varchar(1) NOT NULL default ' ',
  "AAABEIPL" varchar(1) NOT NULL default ' ',
  "AAABEIPM" varchar(1) NOT NULL default ' ',
  "AAABEIPN" varchar(1) NOT NULL default ' ',
  "AAABEIPO" varchar(1) NOT NULL default ' ',
  "AAABEIPP" varchar(1) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIQJ" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIQJ" varchar(5) NOT NULL default ' ',
  "AAABEIQK" varchar(1) NOT NULL default '0',
  "AAABLXTF" varchar(2) NOT NULL default '00',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABJSDF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABEIQJ","AAABEIQK","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIQM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELFZ" varchar(3) NOT NULL default ' ',
  "AAABELGA" varchar(10) NOT NULL default ' ',
  "AAABELGB" varchar(3) NOT NULL default ' ',
  "AAABELGC" varchar(10) NOT NULL default ' ',
  "AAABEIQU" varchar(10) NOT NULL default ' ',
  "AAABEIQV" varchar(3) NOT NULL default ' ',
  "AAABEIQW" varchar(10) NOT NULL default ' ',
  "AAABEIQX" varchar(10) NOT NULL default ' ',
  "AAABEIQY" varchar(3) NOT NULL default ' ',
  "AAABEIQZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABEPXB","AAABELFZ","AAABELGA","AAABELGB","AAABELGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEITU" varchar(5) NOT NULL default ' ',
  "AAABEIRL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEITU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEITU" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEITU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEITU" varchar(5) NOT NULL default ' ',
  "AAABEITV" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABEISG" varchar(34) NOT NULL default ' ',
  "AAABEISH" varchar(3) NOT NULL default ' ',
  "AAABEISI" varchar(34) NOT NULL default ' ',
  "AAABEISJ" varchar(34) NOT NULL default ' ',
  "AAABEISK" varchar(1) NOT NULL default ' ',
  "AAABEISL" varchar(1) NOT NULL default ' ',
  "AAABEISM" varchar(1) NOT NULL default ' ',
  "AAABEISN" varchar(1) NOT NULL default ' ',
  "AAABELFQ" varchar(10) NOT NULL default ' ',
  "AAABELFT" varchar(10) NOT NULL default ' ',
  "AAABEISQ" varchar(1) NOT NULL default ' ',
  "AAABEISR" varchar(1) NOT NULL default ' ',
  "AAABEISS" varchar(1) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEPVK" varchar(10) NOT NULL default ' ',
  "AAABFTWM" varchar(3) NOT NULL default ' ',
  "AAABEISW" varchar(1) NOT NULL default ' ',
  "AAABEISX" varchar(1) NOT NULL default ' ',
  "AAABEISY" varchar(1) NOT NULL default ' ',
  "AAABEISZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEITU","AAABEITV","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEITG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEITU" varchar(5) NOT NULL default ' ',
  "AAABEITV" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEITO" varchar(36) NOT NULL default ' ',
  "AAABEITP" varchar(36) NOT NULL default ' ',
  "AAABHTLA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEITU","AAABEITV","AAABLQVJ","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEITY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEIUB" varchar(10) NOT NULL default ' ',
  "AAABEIUC" varchar(3) NOT NULL default ' ',
  "AAABEIUD" varchar(10) NOT NULL default ' ',
  "AAABEIUE" varchar(10) NOT NULL default ' ',
  "AAABEIUF" varchar(3) NOT NULL default ' ',
  "AAABEIUG" varchar(10) NOT NULL default ' ',
  "AAABEIUH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIVG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIVG" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIVG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIVF" varchar(3) NOT NULL default ' ',
  "AAABEIVG" varchar(1) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABLXJP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIVF","AAABEIVG","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEIVN" varchar(2) NOT NULL default ' ',
  "AAABEIVO" varchar(1) NOT NULL default ' ',
  "AAABEJAV" varchar(1) NOT NULL default ' ',
  "AAABEIVQ" varchar(1) NOT NULL default ' ',
  "AAABEJFS" varchar(1) NOT NULL default ' ',
  "AAABEIVS" varchar(1) NOT NULL default ' ',
  "AAABEIVT" varchar(1) NOT NULL default ' ',
  "AAABEIVU" varchar(1) NOT NULL default ' ',
  "AAABEIVV" varchar(1) NOT NULL default ' ',
  "AAABEIVW" varchar(1) NOT NULL default ' ',
  "AAABEIVX" varchar(1) NOT NULL default ' ',
  "AAABEIVY" varchar(1) NOT NULL default ' ',
  "AAABEIVZ" varchar(1) NOT NULL default ' ',
  "AAABEIWA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEIWW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQBR","AAABEIWW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEIXC" varchar(10) NOT NULL default ' ',
  "AAABEIXD" varchar(3) NOT NULL default ' ',
  "AAABEIXE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABFSXC" varchar(2) NOT NULL default ' ',
  "AAABEIXP" varchar(1) NOT NULL default ' ',
  "AAABEIXQ" varchar(1) NOT NULL default ' ',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  "AAABEIXS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABMQBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEIYC" varchar(1) NOT NULL default '0',
  "AAABEIYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQBR","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABHJRW" varchar(2) NOT NULL default ' ',
  "AAABMPMH" varchar(4) NOT NULL default '0000',
  "AAABFIGO" varchar(7) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABHJRW","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIZS" varchar(12) NOT NULL default ' ',
  "AAABHTLA" varchar(1) NOT NULL default ' ',
  "AAABLSTE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEIZS" varchar(12) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEIZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIZK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEIZS" varchar(12) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAABEIZO" varchar(2) NOT NULL default '00',
  "AAABEIZP" varchar(1) NOT NULL default ' ',
  "AAABEIZQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEIZS","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEIZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJDR" varchar(5) NOT NULL default ' ',
  "AAABEIZY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJDR" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEJDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJAM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJDR" varchar(5) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEJAR" varchar(1) NOT NULL default ' ',
  "AAABEJAS" varchar(10) NOT NULL default ' ',
  "AAABEJAT" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABEJAV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJDR","AAABEKCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJBA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJDR" varchar(5) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABEJBG" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJDR","AAABEKCN","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEJEO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  "AAABIWTN" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEKCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEJEG" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEKCN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEJEG" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKCN","AAABEJEG","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEJEG" varchar(3) NOT NULL default ' ',
  "AAABEJDR" varchar(5) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKCN","AAABEJEG","AAABEJDR","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEJEG" varchar(3) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKCN","AAABEJEG","AAABEPUS","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABEJEO" varchar(2) NOT NULL default ' ',
  "AAABEJEP" varchar(1) NOT NULL default ' ',
  "AAABEJEQ" varchar(1) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEJLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABEJFL" varchar(1) NOT NULL default ' ',
  "AAABEJFM" varchar(1) NOT NULL default ' ',
  "AAABEJFN" varchar(1) NOT NULL default ' ',
  "AAABEJFO" varchar(1) NOT NULL default ' ',
  "AAABEJFP" varchar(1) NOT NULL default ' ',
  "AAABEJFQ" varchar(1) NOT NULL default ' ',
  "AAABEJKI" varchar(1) NOT NULL default ' ',
  "AAABEJFS" varchar(1) NOT NULL default ' ',
  "AAABEJFT" varchar(1) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABKWRL" decimal(17,2) NOT NULL default '0.00',
  "AAABEJFW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJLV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABEJGG" varchar(34) NOT NULL default ' ',
  "AAABEJGH" varchar(34) NOT NULL default ' ',
  "AAABEJGI" varchar(3) NOT NULL default ' ',
  "AAABEJGJ" varchar(34) NOT NULL default ' ',
  "AAABEJGK" varchar(34) NOT NULL default ' ',
  "AAABEJGL" varchar(3) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEJGN" varchar(10) NOT NULL default ' ',
  "AAABEJGO" varchar(10) NOT NULL default ' ',
  "AAABEJGP" varchar(10) NOT NULL default ' ',
  "AAABEJGQ" varchar(10) NOT NULL default ' ',
  "AAABEJGR" varchar(3) NOT NULL default ' ',
  "AAABEJGS" varchar(3) NOT NULL default ' ',
  "AAABEJGT" varchar(3) NOT NULL default ' ',
  "AAABEJGU" varchar(3) NOT NULL default ' ',
  "AAABEJGV" varchar(10) NOT NULL default ' ',
  "AAABEJGW" varchar(10) NOT NULL default ' ',
  "AAABEJGX" varchar(10) NOT NULL default ' ',
  "AAABEJGY" varchar(10) NOT NULL default ' ',
  "AAABEJGZ" varchar(1) NOT NULL default ' ',
  "AAABEJHA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEJLV","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJHG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEJHN" varchar(36) NOT NULL default ' ',
  "AAABEJHO" varchar(34) NOT NULL default ' ',
  "AAABEJHP" varchar(36) NOT NULL default ' ',
  "AAABEJHQ" varchar(34) NOT NULL default ' ',
  "AAABEJHR" varchar(36) NOT NULL default ' ',
  "AAABEJHS" varchar(36) NOT NULL default ' ',
  "AAABEJHT" varchar(36) NOT NULL default ' ',
  "AAABEJHU" varchar(36) NOT NULL default ' ',
  "AAABEJHV" varchar(1) NOT NULL default ' ',
  "AAABEJHW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEJLV","AAABLUVE","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJID" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABJABF" varchar(5) NOT NULL default ' ',
  "AAABKWRL" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJLV","AAABJABF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABEJJV" varchar(10) NOT NULL default ' ',
  "AAABEJJW" varchar(3) NOT NULL default ' ',
  "AAABEJJX" varchar(10) NOT NULL default ' ',
  "AAABEJJZ" varchar(10) NOT NULL default ' ',
  "AAABEJKA" varchar(3) NOT NULL default ' ',
  "AAABEJKB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEJLV","AAABLUVE","AAABEJJV","AAABEJJW","AAABEJJX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEJJO" varchar(10) NOT NULL default ' ',
  "AAABEJJP" varchar(34) NOT NULL default ' ',
  "AAABEJJQ" varchar(3) NOT NULL default ' ',
  "AAABEJJR" varchar(10) NOT NULL default ' ',
  "AAABEJJS" varchar(34) NOT NULL default ' ',
  "AAABEJJT" varchar(18) NOT NULL default ' ',
  "AAABEJJU" varchar(34) NOT NULL default ' ',
  "AAABEJJV" varchar(10) NOT NULL default ' ',
  "AAABEJJW" varchar(3) NOT NULL default ' ',
  "AAABEJJX" varchar(10) NOT NULL default ' ',
  "AAABEJJY" varchar(18) NOT NULL default ' ',
  "AAABEJJZ" varchar(10) NOT NULL default ' ',
  "AAABEJKA" varchar(3) NOT NULL default ' ',
  "AAABEJKB" varchar(10) NOT NULL default ' ',
  "AAABEJKC" varchar(18) NOT NULL default ' ',
  "AAABEJKD" varchar(1) NOT NULL default ' ',
  "AAABEJKE" decimal(7,4) NOT NULL default '0.0000',
  "AAABEJKF" varchar(1) NOT NULL default ' ',
  "AAABEJKG" varchar(1) NOT NULL default ' ',
  "AAABEJKH" varchar(1) NOT NULL default ' ',
  "AAABEJKI" varchar(1) NOT NULL default ' ',
  "AAABEJKJ" varchar(1) NOT NULL default ' ',
  "AAABEJKK" varchar(1) NOT NULL default ' ',
  "AAABEJKL" varchar(1) NOT NULL default ' ',
  "AAABEJKM" varchar(1) NOT NULL default ' ',
  "AAABEJKN" varchar(1) NOT NULL default ' ',
  "AAABEJKO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEJLV","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEJLB" varchar(36) NOT NULL default ' ',
  "AAABEJLC" varchar(34) NOT NULL default ' ',
  "AAABEJLD" varchar(36) NOT NULL default ' ',
  "AAABGRRT" varchar(36) NOT NULL default ' ',
  "AAABEJLF" varchar(1) NOT NULL default ' ',
  "AAABEJLG" varchar(1) NOT NULL default ' ',
  "AAABEJLH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEJLV","AAABLUVE","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEJMA" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEJLV" varchar(5) NOT NULL default ' ',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKCN","AAABEJMA","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEKCN","AAABMQHY","AAABEPWA","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEJNA" varchar(2) NOT NULL default ' ',
  "AAABEJNB" varchar(2) NOT NULL default ' ',
  "AAABEJNC" varchar(2) NOT NULL default ' ',
  "AAABEJND" varchar(2) NOT NULL default ' ',
  "AAABEJNE" varchar(2) NOT NULL default ' ',
  "AAABEJNF" varchar(2) NOT NULL default ' ',
  "AAABEJNG" varchar(2) NOT NULL default ' ',
  "AAABEJNH" varchar(2) NOT NULL default ' ',
  "AAABEJNI" varchar(2) NOT NULL default ' ',
  "AAABEJNJ" varchar(2) NOT NULL default ' ',
  "AAABEJNK" varchar(2) NOT NULL default ' ',
  "AAABEJNL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABEKCN","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMX" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGAMX" varchar(18) NOT NULL default ' ',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABGAMX" varchar(18) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABGAMX" varchar(18) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABIAEU" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJPA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABGAMX" varchar(18) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABGAMX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQY" varchar(18) NOT NULL default ' ',
  "AAABGAMX" varchar(18) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABEUVS" decimal(17,2) NOT NULL default '0.00',
  "AAABEJPY" decimal(17,2) NOT NULL default '0.00',
  "AAABEJPZ" decimal(17,2) NOT NULL default '0.00',
  "AAABJDMI" decimal(17,3) NOT NULL default '0.000',
  "AAABEJQB" decimal(17,2) NOT NULL default '0.00',
  "AAABEJQC" decimal(11,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEJVK","AAABMQHY","AAABEPWA","AAABEPUS","AAABEKQY","AAABGAMX","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQY" varchar(18) NOT NULL default ' ',
  "AAABGAMX" varchar(18) NOT NULL default ' ',
  "AAABEJQW" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEJQX" varchar(1) NOT NULL default ' ',
  "AAABEJQY" varchar(1) NOT NULL default ' ',
  "AAABEJQZ" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEJRA" decimal(17,2) NOT NULL default '0.00',
  "AAABEJRB" decimal(11,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJVK","AAABMQHY","AAABEPWA","AAABEPUS","AAABEKQY","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJRK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEJRO" varchar(10) NOT NULL default ' ',
  "AAABEJRP" varchar(3) NOT NULL default ' ',
  "AAABEJRQ" varchar(10) NOT NULL default ' ',
  "AAABEJRR" varchar(10) NOT NULL default ' ',
  "AAABEJRS" varchar(3) NOT NULL default ' ',
  "AAABEJRT" varchar(10) NOT NULL default ' ',
  "AAABEJRU" varchar(10) NOT NULL default ' ',
  "AAABEJRV" varchar(3) NOT NULL default ' ',
  "AAABEJRW" varchar(10) NOT NULL default ' ',
  "AAABEJRX" varchar(10) NOT NULL default ' ',
  "AAABEJRY" varchar(3) NOT NULL default ' ',
  "AAABEJRZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABEPXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJSD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELFZ" varchar(3) NOT NULL default ' ',
  "AAABELGA" varchar(10) NOT NULL default ' ',
  "AAABELGB" varchar(3) NOT NULL default ' ',
  "AAABELGC" varchar(10) NOT NULL default ' ',
  "AAABELFQ" varchar(10) NOT NULL default ' ',
  "AAABELFR" varchar(3) NOT NULL default ' ',
  "AAABELGK" varchar(10) NOT NULL default ' ',
  "AAABELFT" varchar(10) NOT NULL default ' ',
  "AAABELFU" varchar(3) NOT NULL default ' ',
  "AAABELGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJVK","AAABEPXB","AAABELFZ","AAABELGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABGAMX" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJVK","AAABEPXB","AAABLIMF","AAABGAMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELFZ" varchar(3) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABGAMX" varchar(18) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(36) NOT NULL default ' ',
  "AAABLEPE" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJVK","AAABEPXB","AAABELFZ","AAABLIMF","AAABGAMX","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEJUL" varchar(36) NOT NULL default ' ',
  "AAABEJUM" varchar(36) NOT NULL default ' ',
  "AAABEJUN" varchar(36) NOT NULL default ' ',
  "AAABEJUO" varchar(36) NOT NULL default ' ',
  "AAABEJUP" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEJVK","AAABLIMF","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEJVK" varchar(3) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  "AAABEJVE" varchar(2) NOT NULL default ' ',
  "AAABHYMV" varchar(1) NOT NULL default ' ',
  "AAABEJVG" varchar(1) NOT NULL default ' ',
  "AAABEJVH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEJVK","AAABEKCN","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEJVT" varchar(1) NOT NULL default ' ',
  "AAABEJVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKBC" varchar(5) NOT NULL default ' ',
  "AAABEKDD" varchar(1) NOT NULL default ' ',
  "AAABEJWE" varchar(1) NOT NULL default ' ',
  "AAABEJWF" varchar(1) NOT NULL default ' ',
  "AAABEJWG" varchar(1) NOT NULL default ' ',
  "AAABEJWH" varchar(1) NOT NULL default ' ',
  "AAABEJWI" varchar(1) NOT NULL default ' ',
  "AAABEJWJ" varchar(1) NOT NULL default ' ',
  "AAABEJWK" varchar(1) NOT NULL default ' ',
  "AAABEJWL" varchar(1) NOT NULL default ' ',
  "AAABEJWM" varchar(1) NOT NULL default ' ',
  "AAABEJWN" varchar(1) NOT NULL default ' ',
  "AAABEJWO" varchar(1) NOT NULL default ' ',
  "AAABEJWP" varchar(1) NOT NULL default ' ',
  "AAABEJWQ" varchar(1) NOT NULL default ' ',
  "AAABELAW" varchar(1) NOT NULL default ' ',
  "AAABELAX" varchar(1) NOT NULL default ' ',
  "AAABELAY" varchar(1) NOT NULL default ' ',
  "AAABELAZ" varchar(1) NOT NULL default ' ',
  "AAABELBA" varchar(3) NOT NULL default ' ',
  "AAABELBB" varchar(10) NOT NULL default ' ',
  "AAABELBC" varchar(10) NOT NULL default ' ',
  "AAABELBD" varchar(10) NOT NULL default ' ',
  "AAABELBE" varchar(3) NOT NULL default ' ',
  "AAABELBF" varchar(10) NOT NULL default ' ',
  "AAABELBG" varchar(10) NOT NULL default ' ',
  "AAABELBH" varchar(10) NOT NULL default ' ',
  "AAABELBI" varchar(2) NOT NULL default '00',
  "AAABELBJ" varchar(3) NOT NULL default '000',
  "AAABELBK" varchar(1) NOT NULL default ' ',
  "AAABELBL" varchar(1) NOT NULL default ' ',
  "AAABELBM" varchar(1) NOT NULL default ' ',
  "AAABELBN" decimal(17,2) NOT NULL default '0.00',
  "AAABELBO" varchar(5) NOT NULL default ' ',
  "AAABELBP" varchar(2) NOT NULL default '00',
  "AAABELBQ" varchar(3) NOT NULL default '000',
  "AAABELBR" varchar(1) NOT NULL default ' ',
  "AAABELBS" varchar(1) NOT NULL default ' ',
  "AAABELBT" varchar(1) NOT NULL default ' ',
  "AAABELBU" decimal(17,2) NOT NULL default '0.00',
  "AAABELBV" varchar(5) NOT NULL default ' ',
  "AAABELBW" varchar(2) NOT NULL default '00',
  "AAABELBX" varchar(3) NOT NULL default '000',
  "AAABELBY" varchar(1) NOT NULL default ' ',
  "AAABELBZ" varchar(1) NOT NULL default ' ',
  "AAABELCA" varchar(1) NOT NULL default ' ',
  "AAABELCB" decimal(17,2) NOT NULL default '0.00',
  "AAABELCC" varchar(5) NOT NULL default ' ',
  "AAABELCD" varchar(2) NOT NULL default '00',
  "AAABELCE" varchar(3) NOT NULL default '000',
  "AAABELCF" varchar(1) NOT NULL default ' ',
  "AAABELCG" varchar(1) NOT NULL default ' ',
  "AAABELCH" varchar(1) NOT NULL default ' ',
  "AAABELCI" decimal(17,2) NOT NULL default '0.00',
  "AAABELCJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEJYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEKBC" varchar(5) NOT NULL default ' ',
  "AAABELCQ" varchar(10) NOT NULL default ' ',
  "AAABELCR" varchar(3) NOT NULL default ' ',
  "AAABELCS" varchar(10) NOT NULL default ' ',
  "AAABELCT" varchar(10) NOT NULL default ' ',
  "AAABELCU" varchar(3) NOT NULL default ' ',
  "AAABELCV" varchar(10) NOT NULL default ' ',
  "AAABELCW" varchar(10) NOT NULL default ' ',
  "AAABELHQ" varchar(10) NOT NULL default ' ',
  "AAABELHR" varchar(3) NOT NULL default ' ',
  "AAABELHS" varchar(10) NOT NULL default ' ',
  "AAABELDA" varchar(10) NOT NULL default ' ',
  "AAABELDB" varchar(3) NOT NULL default ' ',
  "AAABELDC" varchar(10) NOT NULL default ' ',
  "AAABELDD" varchar(10) NOT NULL default ' ',
  "AAABELDE" varchar(10) NOT NULL default ' ',
  "AAABELDF" varchar(3) NOT NULL default ' ',
  "AAABELDG" varchar(10) NOT NULL default ' ',
  "AAABELDH" varchar(10) NOT NULL default ' ',
  "AAABELDI" varchar(10) NOT NULL default ' ',
  "AAABELDJ" varchar(3) NOT NULL default ' ',
  "AAABELDK" varchar(10) NOT NULL default ' ',
  "AAABELDL" varchar(10) NOT NULL default ' ',
  "AAABELDM" varchar(3) NOT NULL default ' ',
  "AAABELDN" varchar(10) NOT NULL default ' ',
  "AAABELDO" varchar(10) NOT NULL default ' ',
  "AAABELDP" varchar(3) NOT NULL default ' ',
  "AAABELDQ" varchar(10) NOT NULL default ' ',
  "AAABELDR" varchar(10) NOT NULL default ' ',
  "AAABELDS" varchar(3) NOT NULL default ' ',
  "AAABELDT" varchar(10) NOT NULL default ' ',
  "AAABELDU" varchar(10) NOT NULL default ' ',
  "AAABELDV" varchar(3) NOT NULL default ' ',
  "AAABELDW" varchar(10) NOT NULL default ' ',
  "AAABELDX" varchar(10) NOT NULL default ' ',
  "AAABELHT" varchar(10) NOT NULL default ' ',
  "AAABELHU" varchar(3) NOT NULL default ' ',
  "AAABELHV" varchar(10) NOT NULL default ' ',
  "AAABELEB" varchar(10) NOT NULL default ' ',
  "AAABELEC" varchar(3) NOT NULL default ' ',
  "AAABELED" varchar(10) NOT NULL default ' ',
  "AAABELEE" varchar(10) NOT NULL default ' ',
  "AAABELEF" varchar(10) NOT NULL default ' ',
  "AAABELEG" varchar(3) NOT NULL default ' ',
  "AAABELEH" varchar(10) NOT NULL default ' ',
  "AAABELEI" varchar(10) NOT NULL default ' ',
  "AAABELEJ" varchar(10) NOT NULL default ' ',
  "AAABELEK" varchar(3) NOT NULL default ' ',
  "AAABELEL" varchar(10) NOT NULL default ' ',
  "AAABELEM" varchar(10) NOT NULL default ' ',
  "AAABELEN" varchar(3) NOT NULL default ' ',
  "AAABELEO" varchar(10) NOT NULL default ' ',
  "AAABELEP" varchar(10) NOT NULL default ' ',
  "AAABELEQ" varchar(3) NOT NULL default ' ',
  "AAABELER" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEKBC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKDE" varchar(3) NOT NULL default ' ',
  "AAABEKFS" varchar(3) NOT NULL default ' ',
  "AAABEKFR" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKBR" varchar(7) NOT NULL default '0000000',
  "AAABEKBC" varchar(5) NOT NULL default ' ',
  "AAABEKBD" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKBE" varchar(1) NOT NULL default '0',
  "AAABEKBF" varchar(18) NOT NULL default ' ',
  "AAABEKBG" varchar(1) NOT NULL default ' ',
  "AAABEKBH" varchar(3) NOT NULL default '000',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKWH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKDE","AAABEKFS","AAABEKFR","AAABEPUY","AAABEPUS","AAABEKBR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKDD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKCN","AAABELHY","AAABEKDD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKCN" varchar(10) NOT NULL default ' ',
  "AAABEKDE" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKCN","AAABEKDE","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKCR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKDD" varchar(1) NOT NULL default ' ',
  "AAABEKDE" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABMQBR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABELHY","AAABEKDD","AAABEKDE","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEKFR" varchar(3) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKWH" varchar(10) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEPVK" varchar(10) NOT NULL default ' ',
  "AAABEKQY" varchar(18) NOT NULL default ' ',
  "AAABEKUB" varchar(1) NOT NULL default ' ',
  "AAABEKFJ" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFK" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFL" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFM" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKXJ" varchar(1) NOT NULL default ' ',
  "AAABEKXK" decimal(17,2) NOT NULL default '0.00',
  "AAABEKXL" decimal(17,2) NOT NULL default '0.00',
  "AAABHYMV" varchar(1) NOT NULL default ' ',
  "AAABEKXN" decimal(9,5) NOT NULL default '0.00000',
  "AAABEKEI" varchar(1) NOT NULL default ' ',
  "AAABEKEJ" decimal(17,2) NOT NULL default '0.00',
  "AAABEKEK" decimal(17,2) NOT NULL default '0.00',
  "AAABEKEL" varchar(12) NOT NULL default '000000000000',
  "AAABEKEM" varchar(12) NOT NULL default '000000000000',
  "AAABFNLY" varchar(5) NOT NULL default '00000',
  "AAABEKEO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPXB","AAABEKFR","AAABEKXV","AAABMQHY","AAABEPWA","AAABEPUS","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKEZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEKFR" varchar(3) NOT NULL default ' ',
  "AAABEKFS" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEKFJ" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFK" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFL" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFM" decimal(11,8) NOT NULL default '0.00000000',
  "AAABEKFN" decimal(11,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEKFR","AAABEKFS","AAABEPUY","AAABEPUS","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEKGW" varchar(3) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKWH" varchar(10) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEPVK" varchar(10) NOT NULL default ' ',
  "AAABEKXJ" varchar(1) NOT NULL default ' ',
  "AAABEKXK" decimal(17,2) NOT NULL default '0.00',
  "AAABEKXL" decimal(17,2) NOT NULL default '0.00',
  "AAABHYMV" varchar(1) NOT NULL default ' ',
  "AAABEKXN" decimal(9,5) NOT NULL default '0.00000',
  "AAABEKGR" varchar(12) NOT NULL default '000000000000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPXB","AAABEKGW","AAABEKXV","AAABMQHY","AAABEPWA","AAABLIMF","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEKKF" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABEKQV" varchar(4) NOT NULL default '0000',
  "AAABEKQW" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQY" varchar(18) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKWH" varchar(10) NOT NULL default ' ',
  "AAABEKRD" varchar(4) NOT NULL default '0000',
  "AAABEKRE" varchar(3) NOT NULL default '000',
  "AAABEKLI" varchar(4) NOT NULL default '0000',
  "AAABEKLJ" varchar(3) NOT NULL default '000',
  "AAABEKMN" varchar(4) NOT NULL default '0000',
  "AAABJRFL" varchar(3) NOT NULL default '000',
  "AAABEKQG" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQH" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQI" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQJ" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQK" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQL" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQM" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQN" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQO" decimal(17,2) NOT NULL default '0.00',
  "AAABEKKA" varchar(1) NOT NULL default ' ',
  "AAABEKKB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPXB","AAABEKKF","AAABEPUY","AAABEKXV","AAABEKQV","AAABEKQW","AAABEPUS","AAABEKQY","AAABELHY","AAABEKWH","AAABEKRD","AAABEKRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEKKF" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABEKQV" varchar(4) NOT NULL default '0000',
  "AAABEKQW" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQY" varchar(18) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKWH" varchar(10) NOT NULL default ' ',
  "AAABEKRD" varchar(4) NOT NULL default '0000',
  "AAABEKRE" varchar(3) NOT NULL default '000',
  "AAABEKKQ" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABEKLI" varchar(4) NOT NULL default '0000',
  "AAABEKLJ" varchar(3) NOT NULL default '000',
  "AAABEKMN" varchar(4) NOT NULL default '0000',
  "AAABJRFL" varchar(3) NOT NULL default '000',
  "AAABEKQG" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQH" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQI" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQJ" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQK" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQL" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQM" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQN" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQO" decimal(17,2) NOT NULL default '0.00',
  "AAABEKKA" varchar(1) NOT NULL default ' ',
  "AAABEKKB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEKKF","AAABEPUY","AAABEKXV","AAABEKQV","AAABEKQW","AAABEPUS","AAABEKQY","AAABELHY","AAABEKWH","AAABEKRD","AAABEKRE","AAABEKKQ","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKKS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQZ" varchar(12) NOT NULL default ' ',
  "AAABEKKX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUS","AAABEKQZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQZ" varchar(12) NOT NULL default ' ',
  "AAABEKRA" varchar(4) NOT NULL default ' ',
  "AAABEKLI" varchar(4) NOT NULL default '0000',
  "AAABEKLJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUS","AAABEKQZ","AAABEKRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQZ" varchar(12) NOT NULL default ' ',
  "AAABEKRA" varchar(4) NOT NULL default ' ',
  "AAABIWTP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABEPUS","AAABEKQZ","AAABEKRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKPH" varchar(3) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQZ" varchar(12) NOT NULL default ' ',
  "AAABEKRA" varchar(4) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEKMM" varchar(2) NOT NULL default '00',
  "AAABEKMN" varchar(4) NOT NULL default '0000',
  "AAABJRFL" varchar(3) NOT NULL default '000',
  "AAABEKMP" varchar(1) NOT NULL default '0',
  "AAABEKMQ" decimal(11,8) NOT NULL default '0.00000000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEKPH","AAABEPUS","AAABEKQZ","AAABEKRA","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQZ" varchar(12) NOT NULL default ' ',
  "AAABEKRA" varchar(4) NOT NULL default ' ',
  "AAABEKNG" varchar(10) NOT NULL default ' ',
  "AAABEKNH" varchar(10) NOT NULL default ' ',
  "AAABEKNI" varchar(10) NOT NULL default ' ',
  "AAABEKNJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABEPUS","AAABEKQZ","AAABEKRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEKPH" varchar(3) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEKQZ" varchar(12) NOT NULL default ' ',
  "AAABEKRA" varchar(4) NOT NULL default ' ',
  "AAABEKOB" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOC" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOD" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOE" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOF" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOG" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOH" varchar(1) NOT NULL default ' ',
  "AAABEKOI" decimal(9,5) NOT NULL default '0.00000',
  "AAABEKOJ" varchar(1) NOT NULL default ' ',
  "AAABEKOK" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOL" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOM" varchar(1) NOT NULL default ' ',
  "AAABEKON" decimal(9,5) NOT NULL default '0.00000',
  "AAABEKOO" varchar(1) NOT NULL default ' ',
  "AAABEKOP" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOQ" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOR" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOS" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOT" varchar(1) NOT NULL default ' ',
  "AAABEKOU" decimal(9,5) NOT NULL default '0.00000',
  "AAABEKOV" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOW" decimal(17,2) NOT NULL default '0.00',
  "AAABEKOX" varchar(1) NOT NULL default ' ',
  "AAABEKOY" decimal(9,5) NOT NULL default '0.00000',
  "AAABEKOZ" decimal(17,2) NOT NULL default '0.00',
  "AAABEKPA" decimal(17,2) NOT NULL default '0.00',
  "AAABEKPB" decimal(17,2) NOT NULL default '0.00',
  "AAABEKPC" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPXB","AAABEKPH","AAABEKXV","AAABMQHY","AAABEPWA","AAABEKQZ","AAABEKRA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKPN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABEKQV" varchar(4) NOT NULL default '0000',
  "AAABEKQW" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEKQY" varchar(18) NOT NULL default ' ',
  "AAABEKQZ" varchar(12) NOT NULL default ' ',
  "AAABEKRA" varchar(4) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKWH" varchar(10) NOT NULL default ' ',
  "AAABEKRD" varchar(4) NOT NULL default '0000',
  "AAABEKRE" varchar(3) NOT NULL default '000',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEKQF" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQG" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQH" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQI" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQJ" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQK" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQL" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQM" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQN" decimal(17,2) NOT NULL default '0.00',
  "AAABEKQO" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABMQHX","AAABEPUY","AAABEKXV","AAABEKQV","AAABEKQW","AAABEPUS","AAABEKQY","AAABEKQZ","AAABEKRA","AAABELHY","AAABEKWH","AAABEKRD","AAABEKRE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEKWH" varchar(10) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABEKRQ" varchar(2) NOT NULL default ' ',
  "AAABEKRR" varchar(10) NOT NULL default ' ',
  "AAABEKRS" varchar(18) NOT NULL default ' ',
  "AAABEKRT" varchar(1) NOT NULL default ' ',
  "AAABGYPK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEKWH"),
  KEY "AAABEKRX" ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPXB","AAABMQHX","AAABEKXV","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEKWH" varchar(10) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(10) NOT NULL default '0000000000',
  "AAABEKUQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPXB","AAABMQHX","AAABEKXV","AAABMQHY","AAABEPWA","AAABEKWH","AAABELHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKTD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEKTL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPXB","AAABMQHX","AAABMQHY","AAABEPWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKTY" varchar(10) NOT NULL default ' ',
  "AAABEKTZ" varchar(3) NOT NULL default ' ',
  "AAABEKUA" varchar(10) NOT NULL default ' ',
  "AAABEKUB" varchar(1) NOT NULL default ' ',
  "AAABEKUC" varchar(1) NOT NULL default ' ',
  "AAABEKUD" varchar(10) NOT NULL default ' ',
  "AAABEKUE" varchar(3) NOT NULL default ' ',
  "AAABEKUF" varchar(10) NOT NULL default ' ',
  "AAABEKUG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB","AAABELHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKUL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABLXTF" varchar(2) NOT NULL default '00',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABEKUQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABLXTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABLIMF" varchar(34) NOT NULL default ' ',
  "AAABEPVK" varchar(34) NOT NULL default ' ',
  "AAABEKVB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABELHY","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABELCQ" varchar(34) NOT NULL default ' ',
  "AAABELCS" varchar(34) NOT NULL default ' ',
  "AAABELCT" varchar(34) NOT NULL default ' ',
  "AAABEKVO" varchar(34) NOT NULL default ' ',
  "AAABELDR" varchar(34) NOT NULL default ' ',
  "AAABELDT" varchar(34) NOT NULL default ' ',
  "AAABELDU" varchar(34) NOT NULL default ' ',
  "AAABEKVS" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABELHY","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEKXU" varchar(3) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABELHI" varchar(1) NOT NULL default '0',
  "AAABEKWH" varchar(10) NOT NULL default ' ',
  "AAABEKXJ" varchar(1) NOT NULL default ' ',
  "AAABEKXK" decimal(17,2) NOT NULL default '0.00',
  "AAABEKXL" decimal(17,2) NOT NULL default '0.00',
  "AAABHYMV" varchar(1) NOT NULL default ' ',
  "AAABEKXN" decimal(9,5) NOT NULL default '0.00000',
  "AAABKWQY" decimal(9,0) NOT NULL default '0',
  "AAABKWQZ" decimal(9,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPXB","AAABEKXU","AAABEKXV","AAABMQHY","AAABEPWA","AAABELHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHV" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEKXU" varchar(3) NOT NULL default ' ',
  "AAABEKXV" varchar(18) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABELHI" varchar(1) NOT NULL default '0',
  "AAABEKXJ" varchar(1) NOT NULL default ' ',
  "AAABEKXK" decimal(17,2) NOT NULL default '0.00',
  "AAABEKXL" decimal(17,2) NOT NULL default '0.00',
  "AAABHYMV" varchar(1) NOT NULL default ' ',
  "AAABEKXN" decimal(9,5) NOT NULL default '0.00000',
  "AAABKWQY" decimal(9,0) NOT NULL default '0',
  "AAABKWQZ" decimal(9,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHV","AAABEPXB","AAABEKXU","AAABEKXV","AAABMQHY","AAABEPWA","AAABEPUS","AAABELHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABELHI" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABELHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABELHI" varchar(1) NOT NULL default '0',
  "AAABLUDG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABELHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELHI" varchar(1) NOT NULL default '0',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABEPVK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABELHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEKYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEKZA" varchar(1) NOT NULL default ' ',
  "AAABEKZB" varchar(1) NOT NULL default ' ',
  "AAABEKZC" varchar(1) NOT NULL default ' ',
  "AAABEKZD" varchar(1) NOT NULL default ' ',
  "AAABEKZE" varchar(1) NOT NULL default ' ',
  "AAABEKZF" varchar(1) NOT NULL default ' ',
  "AAABJUHV" varchar(1) NOT NULL default ' ',
  "AAABEKZH" varchar(1) NOT NULL default ' ',
  "AAABEKZI" varchar(1) NOT NULL default ' ',
  "AAABEKZJ" varchar(1) NOT NULL default ' ',
  "AAABEKZK" varchar(1) NOT NULL default ' ',
  "AAABEKZL" varchar(1) NOT NULL default ' ',
  "AAABEKZM" varchar(1) NOT NULL default ' ',
  "AAABEKZN" varchar(1) NOT NULL default ' ',
  "AAABEKZO" varchar(1) NOT NULL default ' ',
  "AAABEKZP" varchar(1) NOT NULL default ' ',
  "AAABEKZQ" varchar(1) NOT NULL default ' ',
  "AAABEKZR" varchar(1) NOT NULL default ' ',
  "AAABEKZS" varchar(1) NOT NULL default ' ',
  "AAABEKZT" varchar(1) NOT NULL default ' ',
  "AAABEKZU" varchar(1) NOT NULL default ' ',
  "AAABEKZV" varchar(1) NOT NULL default ' ',
  "AAABEKZW" varchar(1) NOT NULL default ' ',
  "AAABEKZX" varchar(1) NOT NULL default ' ',
  "AAABEKZY" varchar(1) NOT NULL default ' ',
  "AAABEKZZ" varchar(1) NOT NULL default ' ',
  "AAABELAA" varchar(1) NOT NULL default ' ',
  "AAABELAB" varchar(1) NOT NULL default ' ',
  "AAABELAC" varchar(1) NOT NULL default ' ',
  "AAABELAD" varchar(1) NOT NULL default ' ',
  "AAABELAE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABELAK" varchar(1) NOT NULL default ' ',
  "AAABELAL" varchar(1) NOT NULL default ' ',
  "AAABELAM" varchar(1) NOT NULL default ' ',
  "AAABELAN" varchar(1) NOT NULL default ' ',
  "AAABELAO" varchar(1) NOT NULL default ' ',
  "AAABELAP" varchar(3) NOT NULL default ' ',
  "AAABELAQ" varchar(10) NOT NULL default ' ',
  "AAABELAR" varchar(10) NOT NULL default ' ',
  "AAABELAS" varchar(1) NOT NULL default ' ',
  "AAABELAT" varchar(1) NOT NULL default ' ',
  "AAABELAU" varchar(1) NOT NULL default ' ',
  "AAABELAV" varchar(1) NOT NULL default '0',
  "AAABELAW" varchar(1) NOT NULL default ' ',
  "AAABELAX" varchar(1) NOT NULL default ' ',
  "AAABELAY" varchar(1) NOT NULL default ' ',
  "AAABELAZ" varchar(1) NOT NULL default ' ',
  "AAABELBA" varchar(3) NOT NULL default ' ',
  "AAABELBB" varchar(10) NOT NULL default ' ',
  "AAABELBC" varchar(10) NOT NULL default ' ',
  "AAABELBD" varchar(10) NOT NULL default ' ',
  "AAABELBE" varchar(3) NOT NULL default ' ',
  "AAABELBF" varchar(10) NOT NULL default ' ',
  "AAABELBG" varchar(10) NOT NULL default ' ',
  "AAABELBH" varchar(10) NOT NULL default ' ',
  "AAABELBI" varchar(2) NOT NULL default '00',
  "AAABELBJ" varchar(3) NOT NULL default '000',
  "AAABELBK" varchar(1) NOT NULL default ' ',
  "AAABELBL" varchar(1) NOT NULL default ' ',
  "AAABELBM" varchar(1) NOT NULL default ' ',
  "AAABELBN" decimal(17,2) NOT NULL default '0.00',
  "AAABELBO" varchar(5) NOT NULL default ' ',
  "AAABELBP" varchar(2) NOT NULL default '00',
  "AAABELBQ" varchar(3) NOT NULL default '000',
  "AAABELBR" varchar(1) NOT NULL default ' ',
  "AAABELBS" varchar(1) NOT NULL default ' ',
  "AAABELBT" varchar(1) NOT NULL default ' ',
  "AAABELBU" decimal(17,2) NOT NULL default '0.00',
  "AAABELBV" varchar(5) NOT NULL default ' ',
  "AAABELBW" varchar(2) NOT NULL default '00',
  "AAABELBX" varchar(3) NOT NULL default '000',
  "AAABELBY" varchar(1) NOT NULL default ' ',
  "AAABELBZ" varchar(1) NOT NULL default ' ',
  "AAABELCA" varchar(1) NOT NULL default ' ',
  "AAABELCB" decimal(17,2) NOT NULL default '0.00',
  "AAABELCC" varchar(5) NOT NULL default ' ',
  "AAABELCD" varchar(2) NOT NULL default '00',
  "AAABELCE" varchar(3) NOT NULL default '000',
  "AAABELCF" varchar(1) NOT NULL default ' ',
  "AAABELCG" varchar(1) NOT NULL default ' ',
  "AAABELCH" varchar(1) NOT NULL default ' ',
  "AAABELCI" decimal(17,2) NOT NULL default '0.00',
  "AAABELCJ" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELCQ" varchar(10) NOT NULL default ' ',
  "AAABELCR" varchar(3) NOT NULL default ' ',
  "AAABELCS" varchar(10) NOT NULL default ' ',
  "AAABELCT" varchar(10) NOT NULL default ' ',
  "AAABELCU" varchar(3) NOT NULL default ' ',
  "AAABELCV" varchar(10) NOT NULL default ' ',
  "AAABELCW" varchar(10) NOT NULL default ' ',
  "AAABELHQ" varchar(10) NOT NULL default ' ',
  "AAABELHR" varchar(3) NOT NULL default ' ',
  "AAABELHS" varchar(10) NOT NULL default ' ',
  "AAABELDA" varchar(10) NOT NULL default ' ',
  "AAABELDB" varchar(3) NOT NULL default ' ',
  "AAABELDC" varchar(10) NOT NULL default ' ',
  "AAABELDD" varchar(10) NOT NULL default ' ',
  "AAABELDE" varchar(10) NOT NULL default ' ',
  "AAABELDF" varchar(3) NOT NULL default ' ',
  "AAABELDG" varchar(10) NOT NULL default ' ',
  "AAABELDH" varchar(10) NOT NULL default ' ',
  "AAABELDI" varchar(10) NOT NULL default ' ',
  "AAABELDJ" varchar(3) NOT NULL default ' ',
  "AAABELDK" varchar(10) NOT NULL default ' ',
  "AAABELDL" varchar(10) NOT NULL default ' ',
  "AAABELDM" varchar(3) NOT NULL default ' ',
  "AAABELDN" varchar(10) NOT NULL default ' ',
  "AAABELDO" varchar(10) NOT NULL default ' ',
  "AAABELDP" varchar(3) NOT NULL default ' ',
  "AAABELDQ" varchar(10) NOT NULL default ' ',
  "AAABELDR" varchar(10) NOT NULL default ' ',
  "AAABELDS" varchar(3) NOT NULL default ' ',
  "AAABELDT" varchar(10) NOT NULL default ' ',
  "AAABELDU" varchar(10) NOT NULL default ' ',
  "AAABELDV" varchar(3) NOT NULL default ' ',
  "AAABELDW" varchar(10) NOT NULL default ' ',
  "AAABELDX" varchar(10) NOT NULL default ' ',
  "AAABELHT" varchar(10) NOT NULL default ' ',
  "AAABELHU" varchar(3) NOT NULL default ' ',
  "AAABELHV" varchar(10) NOT NULL default ' ',
  "AAABELEB" varchar(10) NOT NULL default ' ',
  "AAABELEC" varchar(3) NOT NULL default ' ',
  "AAABELED" varchar(10) NOT NULL default ' ',
  "AAABELEE" varchar(10) NOT NULL default ' ',
  "AAABELEF" varchar(10) NOT NULL default ' ',
  "AAABELEG" varchar(3) NOT NULL default ' ',
  "AAABELEH" varchar(10) NOT NULL default ' ',
  "AAABELEI" varchar(10) NOT NULL default ' ',
  "AAABELEJ" varchar(10) NOT NULL default ' ',
  "AAABELEK" varchar(3) NOT NULL default ' ',
  "AAABELEL" varchar(10) NOT NULL default ' ',
  "AAABELEM" varchar(10) NOT NULL default ' ',
  "AAABELEN" varchar(3) NOT NULL default ' ',
  "AAABELEO" varchar(10) NOT NULL default ' ',
  "AAABELEP" varchar(10) NOT NULL default ' ',
  "AAABELEQ" varchar(3) NOT NULL default ' ',
  "AAABELER" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELEV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABELFA" varchar(10) NOT NULL default ' ',
  "AAABELFB" varchar(10) NOT NULL default ' ',
  "AAABELFC" varchar(10) NOT NULL default ' ',
  "AAABELFD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABFJCO","AAABELHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELFI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELFZ" varchar(3) NOT NULL default ' ',
  "AAABELGA" varchar(10) NOT NULL default ' ',
  "AAABELGB" varchar(3) NOT NULL default ' ',
  "AAABELGC" varchar(10) NOT NULL default ' ',
  "AAABELFQ" varchar(10) NOT NULL default ' ',
  "AAABELFR" varchar(3) NOT NULL default ' ',
  "AAABELGK" varchar(10) NOT NULL default ' ',
  "AAABELFT" varchar(10) NOT NULL default ' ',
  "AAABELFU" varchar(3) NOT NULL default ' ',
  "AAABELGL" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHX","AAABEPXB","AAABELFZ","AAABELGA","AAABELGB","AAABELGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABELHG" varchar(3) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELHI" varchar(1) NOT NULL default '0',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABELGK" varchar(10) NOT NULL default ' ',
  "AAABELGL" varchar(10) NOT NULL default ' ',
  "AAABELGM" varchar(10) NOT NULL default ' ',
  "AAABELGN" varchar(3) NOT NULL default ' ',
  "AAABELGO" varchar(10) NOT NULL default ' ',
  "AAABELGP" varchar(10) NOT NULL default ' ',
  "AAABELGQ" varchar(3) NOT NULL default ' ',
  "AAABELGR" varchar(10) NOT NULL default ' ',
  "AAABELGS" varchar(10) NOT NULL default ' ',
  "AAABELGT" varchar(10) NOT NULL default ' ',
  "AAABELGU" varchar(3) NOT NULL default ' ',
  "AAABELGV" varchar(10) NOT NULL default ' ',
  "AAABELGW" varchar(10) NOT NULL default ' ',
  "AAABELGX" varchar(3) NOT NULL default ' ',
  "AAABELGY" varchar(10) NOT NULL default ' ',
  "AAABELGZ" varchar(10) NOT NULL default ' ',
  "AAABELHA" varchar(3) NOT NULL default ' ',
  "AAABELHB" varchar(10) NOT NULL default ' ',
  "AAABELHC" varchar(10) NOT NULL default ' ',
  "AAABELHD" varchar(3) NOT NULL default ' ',
  "AAABELHE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABELHG","AAABEPXB","AAABELHI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABELHY" varchar(2) NOT NULL default ' ',
  "AAABELHN" varchar(10) NOT NULL default ' ',
  "AAABELHO" varchar(3) NOT NULL default ' ',
  "AAABELHP" varchar(10) NOT NULL default ' ',
  "AAABELHQ" varchar(10) NOT NULL default ' ',
  "AAABELHR" varchar(3) NOT NULL default ' ',
  "AAABELHS" varchar(10) NOT NULL default ' ',
  "AAABELHT" varchar(10) NOT NULL default ' ',
  "AAABELHU" varchar(3) NOT NULL default ' ',
  "AAABELHV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABELHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABELKD" varchar(4) NOT NULL default '0000',
  "AAABELKE" varchar(3) NOT NULL default '000',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABMQHX","AAABELKD","AAABELKE","AAABEPUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABFNLY" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABFNLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABFNLY" varchar(5) NOT NULL default '00000',
  "AAABLUDG" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEPVW","AAABFNLY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABFNLY" varchar(5) NOT NULL default '00000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABELKD" varchar(4) NOT NULL default '0000',
  "AAABELKE" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABFNLY","AAABMQHX","AAABELKD","AAABELKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABFNLY" varchar(5) NOT NULL default '00000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABELKD" varchar(4) NOT NULL default '0000',
  "AAABELKE" varchar(3) NOT NULL default '000',
  "AAABELKF" varchar(18) NOT NULL default ' ',
  "AAABELJY" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABFNLY","AAABMQHX","AAABELKD","AAABELKE","AAABELKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELKG" (
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
  "AAABELSL" varchar(3) NOT NULL default '000',
  "AAABELSM" varchar(1) NOT NULL default ' ',
  "AAABELSN" varchar(1) NOT NULL default ' ',
  "AAABELSO" varchar(1) NOT NULL default ' ',
  "AAABELSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELML" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELMR" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFLXW" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABFLXY" varchar(3) NOT NULL default ' ',
  "AAABFLXZ" varchar(1) NOT NULL default ' ',
  "AAABFLYA" varchar(30) NOT NULL default ' ',
  "AAABFLYB" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(1) NOT NULL default ' ',
  "AAABFLYD" varchar(30) NOT NULL default ' ',
  "AAABHFMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELNF" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABFLXW" varchar(30) NOT NULL default ' ',
  "AAABFLXS" varchar(4) NOT NULL default '0000',
  "AAABFLXP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABFLXW","AAABFLXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELNN" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFLXW" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABFLXY" varchar(3) NOT NULL default ' ',
  "AAABFLXZ" varchar(1) NOT NULL default ' ',
  "AAABFLYA" varchar(30) NOT NULL default ' ',
  "AAABFLYB" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(1) NOT NULL default ' ',
  "AAABFLYD" varchar(30) NOT NULL default ' ',
  "AAABHFMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELOB" (
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
  "AAABELSL" varchar(3) NOT NULL default '000',
  "AAABELSM" varchar(1) NOT NULL default ' ',
  "AAABELSN" varchar(1) NOT NULL default ' ',
  "AAABELSO" varchar(1) NOT NULL default ' ',
  "AAABELSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELQG" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELQM" (
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
  "AAABELSL" varchar(3) NOT NULL default '000',
  "AAABELSM" varchar(1) NOT NULL default ' ',
  "AAABELSN" varchar(1) NOT NULL default ' ',
  "AAABELSO" varchar(1) NOT NULL default ' ',
  "AAABELSP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELSR" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABHFOD" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELSX" (
  "AAABIHHB" varchar(30) NOT NULL default ' ',
  "AAABIHLK" varchar(4) NOT NULL default '0000',
  "AAABFLXW" varchar(30) NOT NULL default ' ',
  "AAABIIMJ" varchar(30) NOT NULL default ' ',
  "AAABFLXY" varchar(3) NOT NULL default ' ',
  "AAABFLXZ" varchar(1) NOT NULL default ' ',
  "AAABFLYA" varchar(30) NOT NULL default ' ',
  "AAABFLYB" varchar(30) NOT NULL default ' ',
  "AAABIFOV" varchar(1) NOT NULL default ' ',
  "AAABFLYD" varchar(30) NOT NULL default ' ',
  "AAABHFMU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABIHHB","AAABIHLK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELTL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABELUC" varchar(8) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  "AAABHFNT" varchar(1) NOT NULL default ' ',
  "AAABELTQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABELUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABELUC" varchar(8) NOT NULL default ' ',
  "AAABGRZV" varchar(20) NOT NULL default ' ',
  "AAABHIHE" varchar(40) NOT NULL default ' ',
  "AAABJGCY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABELUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELUD" (
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABETCL" varchar(4) NOT NULL default '0000',
  "AAABGCKI" varchar(4) NOT NULL default '0000',
  "AAABETDZ" varchar(1) NOT NULL default ' ',
  "AAABELUI" varchar(1) NOT NULL default ' ',
  "AAABELUJ" varchar(1) NOT NULL default ' ',
  "AAABELUK" varchar(1) NOT NULL default ' ',
  "AAABELUL" varchar(1) NOT NULL default ' ',
  "AAABELUM" varchar(1) NOT NULL default ' ',
  "AAABELUN" varchar(1) NOT NULL default ' ',
  "AAABELUO" varchar(1) NOT NULL default ' ',
  "AAABELUP" varchar(2) NOT NULL default ' ',
  "AAABFTKZ" varchar(5) NOT NULL default '00000',
  "AAABGWMU" varchar(1) NOT NULL default ' ',
  "AAABETCO" varchar(12) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  "AAABLTLE" varchar(6) NOT NULL default '000000',
  "AAABELUV" varchar(1) NOT NULL default ' ',
  "AAABETCR" varchar(4) NOT NULL default '0000',
  "AAABETCS" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMLXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELUZ" (
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABETCX" varchar(31) NOT NULL default ' ',
  "AAABETCY" varchar(31) NOT NULL default ' ',
  "AAABETCZ" varchar(31) NOT NULL default ' ',
  "AAABETDA" varchar(31) NOT NULL default ' ',
  "AAABETDB" varchar(31) NOT NULL default ' ',
  "AAABETDC" varchar(31) NOT NULL default ' ',
  "AAABETDD" varchar(31) NOT NULL default ' ',
  "AAABETDE" varchar(31) NOT NULL default ' ',
  "AAABETDF" varchar(31) NOT NULL default ' ',
  "AAABETDG" varchar(31) NOT NULL default ' ',
  "AAABETDH" varchar(31) NOT NULL default ' ',
  "AAABETDI" varchar(31) NOT NULL default ' ',
  "AAABFTKZ" varchar(5) NOT NULL default '00000',
  "AAABETDJ" varchar(3) NOT NULL default '000',
  "AAABELVQ" varchar(3) NOT NULL default '000',
  "AAABETDM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLXR","AAABMRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABKWQZ" (
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLXR","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELWA" (
  "AAABELWU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABELWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELWD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPWP" varchar(1) NOT NULL default ' ',
  "AAABELWU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPWP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELWJ" (
  "AAABELWU" varchar(2) NOT NULL default ' ',
  "AAABMLTD" varchar(2) NOT NULL default '00',
  "AAABGRZB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABELWU","AAABMLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELWP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABELWU" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABELWU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELWV" (
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABINDH" varchar(8) NOT NULL default '00000000',
  "AAABINDF" varchar(8) NOT NULL default '00000000',
  "AAABMBIU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLXR","AAABMRBY","AAABINDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELXE" (
  "AAABELXK" varchar(1) NOT NULL default ' ',
  "AAABMLXR" varchar(2) NOT NULL default ' ',
  "AAABMRBY" varchar(4) NOT NULL default '0000',
  "AAABINDH" varchar(8) NOT NULL default '00000000',
  "AAABMOQQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABELXK","AAABMLXR","AAABMRBY","AAABINDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABELZA" varchar(6) NOT NULL default ' ',
  "AAABELZB" varchar(1) NOT NULL default ' ',
  "AAABELXS" varchar(2) NOT NULL default ' ',
  "AAABELXT" varchar(9) NOT NULL default ' ',
  "AAABEQVB" varchar(14) NOT NULL default ' ',
  "AAABELXV" varchar(3) NOT NULL default '000',
  "AAABELXW" varchar(5) NOT NULL default '00000',
  "AAABELXX" varchar(2) NOT NULL default '00',
  "AAABELXY" varchar(3) NOT NULL default '000',
  "AAABELXZ" varchar(11) NOT NULL default ' ',
  "AAABELYA" varchar(1) NOT NULL default ' ',
  "AAABELYB" varchar(5) NOT NULL default '00000',
  "AAABELYC" varchar(5) NOT NULL default '00000',
  "AAABELYD" varchar(5) NOT NULL default '00000',
  "AAABELYE" varchar(5) NOT NULL default '00000',
  "AAABELYF" varchar(5) NOT NULL default '00000',
  "AAABELYG" varchar(9) NOT NULL default ' ',
  "AAABELYH" varchar(9) NOT NULL default ' ',
  "AAABEQZH" varchar(12) NOT NULL default ' ',
  "AAABELYJ" varchar(4) NOT NULL default '0000',
  "AAABELYK" varchar(4) NOT NULL default '0000',
  "AAABELYL" varchar(1) NOT NULL default ' ',
  "AAABELYM" varchar(1) NOT NULL default ' ',
  "AAABELYN" varchar(4) NOT NULL default '0000',
  "AAABELYO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABELZA","AAABELZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELYS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABELZA" varchar(6) NOT NULL default ' ',
  "AAABELZB" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABELZA","AAABELZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLBW" varchar(5) NOT NULL default ' ',
  "AAABLDNL" varchar(30) NOT NULL default ' ',
  "AAABJLIY" varchar(30) NOT NULL default ' ',
  "AAABFVBU" varchar(10) NOT NULL default ' ',
  "AAABELZI" varchar(2) NOT NULL default '00',
  "AAABELZJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMLBW","AAABLDNL","AAABJLIY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABELZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(4) NOT NULL default ' ',
  "AAABEMDK" varchar(62) NOT NULL default ' ',
  "AAABEMDD" varchar(2) NOT NULL default ' ',
  "AAABEMDE" varchar(25) NOT NULL default ' ',
  "AAABEMDF" varchar(25) NOT NULL default ' ',
  "AAABELZW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNYT","AAABMNLX","AAABEMDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMAB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(4) NOT NULL default ' ',
  "AAABMMCU" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNYT","AAABMNLX","AAABMMCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(4) NOT NULL default ' ',
  "AAABEMDK" varchar(62) NOT NULL default ' ',
  "AAABEMCA" varchar(25) NOT NULL default ' ',
  "AAABEMBU" VARBINARY(255),
  "AAABEMBV" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABMNYT","AAABMNLX","AAABEMDK","AAABEMCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMAX" (
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABEMBD" varchar(62) NOT NULL default ' ',
  "AAABLDEM" varchar(3) NOT NULL default '000',
  "AAABEMBB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNYT","AAABEMBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMBE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABLDEM" varchar(3) NOT NULL default '000',
  "AAABHPIX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMNYT","AAABLDEM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(4) NOT NULL default ' ',
  "AAABEMDK" varchar(62) NOT NULL default ' ',
  "AAABEMCA" varchar(25) NOT NULL default ' ',
  "AAABEMBU" VARBINARY(255),
  "AAABEMBV" VARBINARY(255),
  PRIMARY KEY ("AAABMRIL","AAABMNYT","AAABMNLX","AAABEMDK","AAABEMCA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMCB" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(4) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABMNYT","AAABMNLX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(4) NOT NULL default ' ',
  "AAABLDEM" varchar(3) NOT NULL default '000',
  "AAABEMDK" varchar(62) NOT NULL default ' ',
  "AAABEMDD" varchar(2) NOT NULL default ' ',
  "AAABEMDE" varchar(25) NOT NULL default ' ',
  "AAABEMDF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNYT","AAABMNLX","AAABLDEM","AAABEMDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYT" varchar(40) NOT NULL default ' ',
  "AAABMNLX" varchar(4) NOT NULL default ' ',
  "AAABLDEM" varchar(3) NOT NULL default '000',
  "AAABEMDK" varchar(62) NOT NULL default ' ',
  "AAABEMDD" varchar(2) NOT NULL default ' ',
  "AAABEMDE" varchar(25) NOT NULL default ' ',
  "AAABEMDF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMNYT","AAABMNLX","AAABLDEM","AAABEMDK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMDL" (
  "AAABMPLT" varchar(10) NOT NULL default ' ',
  "AAABEMDR" varchar(3) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABEMDP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPLT","AAABEMDR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMDS" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABGGAK" varchar(8) NOT NULL default ' ',
  "AAABEMDV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABGGAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMDY" (
  "AAABGGAK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABGGAK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABEMEF" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODI","AAABIHKQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEMFA" varchar(18) NOT NULL default ' ',
  "AAABEMES" varchar(2) NOT NULL default '00',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODI","AAABIHKQ","AAABEPVW","AAABEPXB","AAABMQHX","AAABEMFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABFTZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODI","AAABIHKQ","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODI","AAABIHKQ","AAABEPVW","AAABEPXB","AAABMQHX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMODI" varchar(32) NOT NULL default ' ',
  "AAABIHKQ" varchar(30) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABIWNN" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMODI","AAABIHKQ","AAABEPVW","AAABEPXB","AAABMQHX","AAABIWNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABHFBC" varchar(5) NOT NULL default ' ',
  "AAABEMGU" varchar(10) NOT NULL default ' ',
  "AAABEMGV" varchar(10) NOT NULL default ' ',
  "AAABEMHX" varchar(3) NOT NULL default ' ',
  "AAABEMHY" varchar(3) NOT NULL default ' ',
  "AAABEMGY" varchar(1) NOT NULL default ' ',
  "AAABEMGZ" varchar(5) NOT NULL default ' ',
  "AAABEMHA" varchar(50) NOT NULL default ' ',
  "AAABIGAW" varchar(4) NOT NULL default ' ',
  "AAABIMHJ" varchar(12) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABEMHE" varchar(3) NOT NULL default ' ',
  "AAABEMHF" varchar(3) NOT NULL default ' ',
  "AAABEMHG" varchar(3) NOT NULL default ' ',
  "AAABEMHH" varchar(1) NOT NULL default ' ',
  "AAABEMIN" varchar(1) NOT NULL default ' ',
  "AAABEMHJ" varchar(1) NOT NULL default ' ',
  "AAABEMHK" varchar(4) NOT NULL default ' ',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABEMHM" varchar(1) NOT NULL default ' ',
  "AAABMCSL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEMHV" varchar(10) NOT NULL default ' ',
  "AAABEMHW" varchar(10) NOT NULL default ' ',
  "AAABEMHX" varchar(3) NOT NULL default ' ',
  "AAABEMHY" varchar(3) NOT NULL default ' ',
  "AAABLCFI" varchar(10) NOT NULL default ' ',
  "AAABEMIA" varchar(30) NOT NULL default ' ',
  "AAABEMIB" varchar(50) NOT NULL default ' ',
  "AAABEMIC" varchar(30) NOT NULL default ' ',
  "AAABMNEA" varchar(10) NOT NULL default ' ',
  "AAABEMIE" varchar(15) NOT NULL default ' ',
  "AAABHFBC" varchar(5) NOT NULL default ' ',
  "AAABIGAW" varchar(4) NOT NULL default ' ',
  "AAABEMIH" varchar(1) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  "AAABIMHJ" varchar(12) NOT NULL default ' ',
  "AAABEMIL" varchar(1) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABEMIN" varchar(1) NOT NULL default ' ',
  "AAABGCHL" varchar(12) NOT NULL default ' ',
  "AAABEMIP" varchar(1) NOT NULL default ' ',
  "AAABLTHN" varchar(6) NOT NULL default ' ',
  "AAABEMIR" varchar(1) NOT NULL default ' ',
  "AAABEMIS" varchar(4) NOT NULL default ' ',
  "AAABEMIT" varchar(6) NOT NULL default ' ',
  "AAABMCSL" varchar(1) NOT NULL default ' ',
  "AAABEMIV" varchar(60) NOT NULL default ' ',
  "AAABEMIW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMDEN","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVCV" varchar(2) NOT NULL default ' ',
  "AAABHUVN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLVCV","AAABHUVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUVN" varchar(4) NOT NULL default ' ',
  "AAABHUVO" varchar(3) NOT NULL default '000',
  "AAABHUVM" varchar(30) NOT NULL default ' ',
  "AAABFXYZ" varchar(30) NOT NULL default ' ',
  "AAABFXUZ" varchar(30) NOT NULL default ' ',
  "AAABFXVA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUVN","AAABHUVO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUVN" varchar(4) NOT NULL default ' ',
  "AAABEMJV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHUVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHUVN" varchar(4) NOT NULL default ' ',
  "AAABEMKC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHUVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABEMZP" varchar(10) NOT NULL default ' ',
  "AAABEMYQ" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEMYW" varchar(10) NOT NULL default ' ',
  "AAABKUYQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABEMZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABEMZP" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABEMZP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABENAC" varchar(1) NOT NULL default '0',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUEO","AAABENAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABENAC" varchar(1) NOT NULL default '0',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUEO","AAABENAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUEO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABEMNC" int(11) NOT NULL default '0',
  "AAABMLZS" int(11) NOT NULL default '0',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABEMNC","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABEMNC" int(11) NOT NULL default '0',
  "AAABMLZS" int(11) NOT NULL default '0',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMOHM","AAABEMNC","AAABMLZS","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLMKI" varchar(8) NOT NULL default '00000000',
  "AAABEMNK" varchar(1) NOT NULL default ' ',
  "AAABMDBB" varchar(4) NOT NULL default '0000',
  "AAABMPVB" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABEMZP" varchar(10) NOT NULL default ' ',
  "AAABEMYQ" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEMYW" varchar(10) NOT NULL default ' ',
  "AAABKUYQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMOHM","AAABEMZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABEMZP" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMOHM","AAABEMZP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMOX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABENAC" varchar(1) NOT NULL default '0',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABLUEO","AAABENAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABLUEO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLIMF" varchar(12) NOT NULL default ' ',
  "AAABENBK" varchar(12) NOT NULL default ' ',
  "AAABENBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABLIMF","AAABENBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(32) NOT NULL default ' ',
  "AAABENBS" varchar(1) NOT NULL default ' ',
  "AAABENBT" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABENBV" varchar(1) NOT NULL default ' ',
  "AAABHASI" varchar(2) NOT NULL default ' ',
  "AAABHASJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABHMRI" varchar(12) NOT NULL default ' ',
  "AAABLIMF" varchar(12) NOT NULL default ' ',
  "AAABENCR" int(11) NOT NULL default '0',
  "AAABMLZS" int(11) NOT NULL default '0',
  "AAABENCT" int(11) NOT NULL default '0',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABLMLR" varchar(12) NOT NULL default ' ',
  "AAABENCY" varchar(20) NOT NULL default ' ',
  "AAABENCZ" varchar(12) NOT NULL default ' ',
  "AAABENDA" varchar(12) NOT NULL default ' ',
  "AAABENDB" int(11) NOT NULL default '0',
  "AAABENDC" varchar(1) NOT NULL default ' ',
  "AAABENDD" varchar(6) NOT NULL default '000000',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAABENDG" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABJURT" varchar(40) NOT NULL default ' ',
  "AAABENDJ" varchar(10) NOT NULL default '0000000000',
  "AAABENDK" varchar(8) NOT NULL default '00000000',
  "AAABENDL" varchar(6) NOT NULL default '000000',
  "AAABENDM" varchar(42) NOT NULL default ' ',
  "AAABENDN" varchar(8) NOT NULL default ' ',
  "AAABENDO" varchar(12) NOT NULL default ' ',
  "AAABENDP" varchar(1) NOT NULL default ' ',
  "AAABENDQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABHMRI","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABENHM" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMLQA","AAABENHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABENHM" varchar(12) NOT NULL default ' ',
  "AAABENEO" varchar(12) NOT NULL default ' ',
  "AAABENEP" varchar(12) NOT NULL default ' ',
  "AAABMLZS" int(11) NOT NULL default '0',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABENEU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMOHM","AAABENHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  "AAABENGG" varchar(32) NOT NULL default ' ',
  "AAABENFG" varchar(5) NOT NULL default ' ',
  "AAABENFH" varchar(10) NOT NULL default ' ',
  "AAABENFI" varchar(30) NOT NULL default ' ',
  "AAABENFJ" varchar(30) NOT NULL default ' ',
  "AAABENFK" varchar(30) NOT NULL default ' ',
  "AAABENFL" varchar(30) NOT NULL default ' ',
  "AAABENFM" varchar(30) NOT NULL default ' ',
  "AAABENFN" varchar(30) NOT NULL default ' ',
  "AAABENFO" varchar(30) NOT NULL default ' ',
  "AAABENFP" varchar(30) NOT NULL default ' ',
  "AAABENFQ" varchar(30) NOT NULL default ' ',
  "AAABENFR" varchar(30) NOT NULL default ' ',
  "AAABENFS" varchar(30) NOT NULL default ' ',
  "AAABENFT" varchar(30) NOT NULL default ' ',
  "AAABENFU" varchar(30) NOT NULL default ' ',
  "AAABENFV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABENGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABENGG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABENGK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMVS" (
  "AAABENGU" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABGBTL" varchar(12) NOT NULL default ' ',
  "AAABENHM" varchar(12) NOT NULL default ' ',
  "AAABENGS" varchar(64) NOT NULL default ' ',
  "AAABENGT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABENGU","AAABLUVS","AAABHFAP","AAABGBTL","AAABENHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABENHM" varchar(12) NOT NULL default ' ',
  "AAABENHN" int(11) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(10) NOT NULL default ' ',
  "AAABMNNA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABENHM","AAABENHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  "AAABENGG" varchar(32) NOT NULL default ' ',
  "AAABENFG" varchar(5) NOT NULL default ' ',
  "AAABENFH" varchar(10) NOT NULL default ' ',
  "AAABENFI" varchar(30) NOT NULL default ' ',
  "AAABENFJ" varchar(30) NOT NULL default ' ',
  "AAABENFK" varchar(30) NOT NULL default ' ',
  "AAABENFL" varchar(30) NOT NULL default ' ',
  "AAABENFM" varchar(30) NOT NULL default ' ',
  "AAABENFN" varchar(30) NOT NULL default ' ',
  "AAABENFO" varchar(30) NOT NULL default ' ',
  "AAABENFP" varchar(30) NOT NULL default ' ',
  "AAABENFQ" varchar(30) NOT NULL default ' ',
  "AAABENFR" varchar(30) NOT NULL default ' ',
  "AAABENFS" varchar(30) NOT NULL default ' ',
  "AAABENFT" varchar(30) NOT NULL default ' ',
  "AAABENFU" varchar(30) NOT NULL default ' ',
  "AAABENFV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABENGG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENGK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEMYE" varchar(10) NOT NULL default ' ',
  "AAABEMYF" int(11) NOT NULL default '0',
  "AAABEMYJ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEMYE","AAABEMYF","AAABEMYJ"),
  KEY "AAABEMYH" ("AAABMRIL","AAABEMYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABEMZP" varchar(10) NOT NULL default ' ',
  "AAABEMYQ" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABMCVV" varchar(4) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABEMYW" varchar(10) NOT NULL default ' ',
  "AAABKUYQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMOHM","AAABEMZP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABEMZP" varchar(10) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMOHM","AAABEMZP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEMZR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABENAC" varchar(1) NOT NULL default '0',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABLUEO","AAABENAC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENAD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABLUEO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLIMF" varchar(12) NOT NULL default ' ',
  "AAABENBK" varchar(12) NOT NULL default ' ',
  "AAABENBF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABLIMF","AAABENBK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABLUEO" varchar(1) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(32) NOT NULL default ' ',
  "AAABENBS" varchar(1) NOT NULL default ' ',
  "AAABENBT" varchar(1) NOT NULL default ' ',
  "AAABMLXQ" varchar(2) NOT NULL default ' ',
  "AAABENBV" varchar(1) NOT NULL default ' ',
  "AAABHASI" varchar(2) NOT NULL default ' ',
  "AAABHASJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABHMRI" varchar(12) NOT NULL default ' ',
  "AAABLIMF" varchar(12) NOT NULL default ' ',
  "AAABENCR" int(11) NOT NULL default '0',
  "AAABMLZS" int(11) NOT NULL default '0',
  "AAABENCT" int(11) NOT NULL default '0',
  "AAABMNLA" varchar(1) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABMOIB" varchar(14) NOT NULL default ' ',
  "AAABLMLR" varchar(12) NOT NULL default ' ',
  "AAABENCY" varchar(20) NOT NULL default ' ',
  "AAABENCZ" varchar(12) NOT NULL default ' ',
  "AAABENDA" varchar(12) NOT NULL default ' ',
  "AAABENDB" int(11) NOT NULL default '0',
  "AAABENDC" varchar(1) NOT NULL default ' ',
  "AAABENDD" varchar(6) NOT NULL default '000000',
  "AAABJASN" varchar(1) NOT NULL default ' ',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAABENDG" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABJURT" varchar(40) NOT NULL default ' ',
  "AAABENDJ" varchar(10) NOT NULL default '0000000000',
  "AAABENDK" varchar(8) NOT NULL default '00000000',
  "AAABENDL" varchar(6) NOT NULL default '000000',
  "AAABENDM" varchar(42) NOT NULL default ' ',
  "AAABENDN" varchar(8) NOT NULL default ' ',
  "AAABENDO" varchar(12) NOT NULL default ' ',
  "AAABENDP" varchar(1) NOT NULL default ' ',
  "AAABENDQ" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABHMRI","AAABLIMF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABENHM" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMLQA","AAABENHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENEI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABMOHM" varchar(5) NOT NULL default ' ',
  "AAABENHM" varchar(12) NOT NULL default ' ',
  "AAABENEO" varchar(12) NOT NULL default ' ',
  "AAABENEP" varchar(12) NOT NULL default ' ',
  "AAABMLZS" int(11) NOT NULL default '0',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  "AAABKMMG" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  "AAABENEU" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABMOHM","AAABENHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  "AAABENGG" varchar(32) NOT NULL default ' ',
  "AAABENFG" varchar(5) NOT NULL default ' ',
  "AAABENFH" varchar(10) NOT NULL default ' ',
  "AAABENFI" varchar(30) NOT NULL default ' ',
  "AAABENFJ" varchar(30) NOT NULL default ' ',
  "AAABENFK" varchar(30) NOT NULL default ' ',
  "AAABENFL" varchar(30) NOT NULL default ' ',
  "AAABENFM" varchar(30) NOT NULL default ' ',
  "AAABENFN" varchar(30) NOT NULL default ' ',
  "AAABENFO" varchar(30) NOT NULL default ' ',
  "AAABENFP" varchar(30) NOT NULL default ' ',
  "AAABENFQ" varchar(30) NOT NULL default ' ',
  "AAABENFR" varchar(30) NOT NULL default ' ',
  "AAABENFS" varchar(30) NOT NULL default ' ',
  "AAABENFT" varchar(30) NOT NULL default ' ',
  "AAABENFU" varchar(30) NOT NULL default ' ',
  "AAABENFV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABENGK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABENGK" varchar(5) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABENGG" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABENGK","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENGM" (
  "AAABENGU" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABGBTL" varchar(12) NOT NULL default ' ',
  "AAABENHM" varchar(12) NOT NULL default ' ',
  "AAABENGS" varchar(64) NOT NULL default ' ',
  "AAABENGT" varchar(64) NOT NULL default ' ',
  PRIMARY KEY ("AAABENGU","AAABLUVS","AAABHFAP","AAABGBTL","AAABENHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUVS" varchar(10) NOT NULL default ' ',
  "AAABHFAP" int(11) NOT NULL default '0',
  "AAABENHM" varchar(12) NOT NULL default ' ',
  "AAABENHN" int(11) NOT NULL default '0',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABIJIG" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(10) NOT NULL default ' ',
  "AAABMNNA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUVS","AAABHFAP","AAABENHM","AAABENHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENHO" (
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABILZK" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(2) NOT NULL default ' ',
  "AAABENHS" varchar(8) NOT NULL default '00000000',
  "AAABLECD" varchar(1) NOT NULL default ' ',
  "AAABMMKX" varchar(3) NOT NULL default '000',
  "AAABENHV" varchar(1) NOT NULL default ' ',
  "AAABLUVJ" varchar(8) NOT NULL default ' ',
  "AAABENHX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ"),
  KEY "AAABENHZ" ("AAABILZK"),
  KEY "AAABENIB" ("AAABILZK","AAABLJIM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENIE" (
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABILZK" varchar(40) NOT NULL default ' ',
  "AAABLJIM" varchar(2) NOT NULL default ' ',
  "AAABENII" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKGJ","AAABILZK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  "AAABENIP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJCTT","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  "AAABENIY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJCTT","AAABLPSK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABENKB" varchar(3) NOT NULL default '000',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABLRXL" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  "AAABENJL" varchar(30) NOT NULL default ' ',
  "AAABENJM" varchar(30) NOT NULL default ' ',
  "AAABENJN" varchar(30) NOT NULL default ' ',
  "AAABLCJF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABENKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABENKB" varchar(3) NOT NULL default '000',
  "AAABMCWO" varchar(3) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  "AAABMNRG" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABENKB","AAABMCWO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENKD" (
  "AAABMREF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENKG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABENKQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENKT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKIKF" varchar(12) NOT NULL default ' ',
  "AAABENLO" varchar(20) NOT NULL default ' ',
  "AAABMCRK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKIKF","AAABENLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABJCTT" varchar(4) NOT NULL default ' ',
  "AAABLPSK" varchar(4) NOT NULL default ' ',
  "AAABENLO" varchar(20) NOT NULL default ' ',
  "AAABMOAH" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABJCTT","AAABLPSK","AAABENLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENLP" (
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABENLR" varchar(30) NOT NULL default ' ',
  "AAABENLS" varchar(1) NOT NULL default ' ',
  "AAABENLT" varchar(1) NOT NULL default ' ',
  "AAABENLU" varchar(1) NOT NULL default ' ',
  "AAABENLV" varchar(1) NOT NULL default ' ',
  "AAABENLW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENLY" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABENMA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENMC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENMI" (
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABENMX" varchar(2) NOT NULL default '00',
  "AAABJTCY" varchar(30) NOT NULL default ' ',
  "AAABGKWP" varchar(6) NOT NULL default ' ',
  "AAABMMYX" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMPMH","AAABENMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENMQ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABENMX" varchar(2) NOT NULL default '00',
  "AAABLXIS" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMPMH","AAABENMX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENSM" varchar(14) NOT NULL default ' ',
  "AAABLSOF" varchar(10) NOT NULL default ' ',
  "AAABMKFG" varchar(10) NOT NULL default ' ',
  "AAABENND" varchar(4) NOT NULL default ' ',
  "AAABENNR" varchar(3) NOT NULL default ' ',
  "AAABENNS" varchar(15) NOT NULL default ' ',
  "AAABENNT" varchar(18) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABLPTF" varchar(4) NOT NULL default ' ',
  "AAABENOA" varchar(50) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENNN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENSM" varchar(14) NOT NULL default ' ',
  "AAABMLPV" varchar(35) NOT NULL default ' ',
  "AAABENNR" varchar(3) NOT NULL default ' ',
  "AAABENNS" varchar(15) NOT NULL default ' ',
  "AAABENNT" varchar(18) NOT NULL default ' ',
  "AAABENNU" varchar(2) NOT NULL default ' ',
  "AAABENNV" varchar(20) NOT NULL default ' ',
  "AAABLPMM" varchar(4) NOT NULL default ' ',
  "AAABLPTF" varchar(4) NOT NULL default ' ',
  "AAABENNY" varchar(1) NOT NULL default ' ',
  "AAABENNZ" varchar(10) NOT NULL default ' ',
  "AAABENOA" varchar(50) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  "AAABENOC" varchar(35) NOT NULL default ' ',
  "AAABENOD" varchar(10) NOT NULL default ' ',
  "AAABJSTP" varchar(35) NOT NULL default ' ',
  "AAABENOF" varchar(10) NOT NULL default ' ',
  "AAABJXDX" varchar(35) NOT NULL default ' ',
  "AAABIXXS" varchar(10) NOT NULL default ' ',
  "AAABLEHK" varchar(3) NOT NULL default ' ',
  "AAABIYJJ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABENPN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABENPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABENPN" varchar(2) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAABKWCV" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABENPM","AAABENPN","AAABMCTJ","AAABKWCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENPE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABENPM" varchar(4) NOT NULL default ' ',
  "AAABENPN" varchar(2) NOT NULL default ' ',
  "AAABENPJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABENPM","AAABENPN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKYIH" varchar(1) NOT NULL default ' ',
  "AAABLTHN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKYIH","AAABLTHN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENPV" (
  "AAABKYIH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKYIH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABENSM" varchar(14) NOT NULL default ' ',
  "AAABENQC" varchar(4) NOT NULL default ' ',
  "AAABENQD" varchar(5) NOT NULL default ' ',
  "AAABENQE" varchar(5) NOT NULL default ' ',
  "AAABENQF" varchar(5) NOT NULL default ' ',
  "AAABENQG" decimal(13,2) NOT NULL default '0.00',
  "AAABENQH" varchar(5) NOT NULL default ' ',
  "AAABENQI" decimal(13,2) NOT NULL default '0.00',
  "AAABENQJ" varchar(1) NOT NULL default ' ',
  "AAABLOCD" varchar(50) NOT NULL default ' ',
  "AAABENQL" varchar(1) NOT NULL default ' ',
  "AAABENQM" varchar(2) NOT NULL default ' ',
  "AAABENQN" varchar(2) NOT NULL default '00',
  "AAABENQO" varchar(2) NOT NULL default '00',
  "AAABENQP" varchar(2) NOT NULL default '00',
  "AAABENQQ" varchar(2) NOT NULL default '00',
  "AAABENQR" varchar(3) NOT NULL default ' ',
  "AAABENQS" varchar(3) NOT NULL default ' ',
  "AAABENQT" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSDL","AAABENSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENQX" (
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABJCBT" varchar(4) NOT NULL default ' ',
  "AAABEQRV" varchar(20) NOT NULL default ' ',
  "AAABENRC" varchar(30) NOT NULL default ' ',
  "AAABENRD" varchar(30) NOT NULL default ' ',
  "AAABENRE" varchar(30) NOT NULL default ' ',
  "AAABENRF" varchar(30) NOT NULL default ' ',
  "AAABENRG" varchar(30) NOT NULL default ' ',
  "AAABENRH" varchar(30) NOT NULL default ' ',
  "AAABENRI" varchar(30) NOT NULL default ' ',
  "AAABMAYY" varchar(2) NOT NULL default ' ',
  "AAABENRK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABENRP" varchar(1) NOT NULL default ' ',
  "AAABENRQ" varchar(1) NOT NULL default ' ',
  "AAABENRR" varchar(1) NOT NULL default ' ',
  "AAABENRS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABLPNX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSDL","AAABLPNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABENSM" varchar(14) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMQCA" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSDL","AAABENSM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENSN" (
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABENSP" varchar(1) NOT NULL default ' ',
  "AAABENSQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENSS" (
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABENSU" varchar(1) NOT NULL default ' ',
  "AAABENSV" varchar(1) NOT NULL default ' ',
  "AAABENSW" varchar(1) NOT NULL default ' ',
  "AAABENSX" varchar(1) NOT NULL default ' ',
  "AAABENSY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENTA" (
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABENTC" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENTE" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLSDL" varchar(8) NOT NULL default ' ',
  "AAABENTH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLSDL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENTK" (
  "AAABENTQ" varchar(2) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABENUO" varchar(1) NOT NULL default '0',
  "AAABENUP" varchar(3) NOT NULL default ' ',
  "AAABENTP" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABENTQ","AAABLESS","AAABENUO","AAABENUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENTU" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABENUO" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLESS","AAABENUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENTZ" (
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABENUO" varchar(1) NOT NULL default '0',
  "AAABENUP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABLESS","AAABENUO","AAABENUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENUG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLESS" varchar(40) NOT NULL default ' ',
  "AAABENUO" varchar(1) NOT NULL default '0',
  "AAABENUP" varchar(3) NOT NULL default ' ',
  "AAABENUL" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLESS","AAABENUO","AAABENUP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMLDW" varchar(20) NOT NULL default ' ',
  "AAABMLDY" varchar(1) NOT NULL default ' ',
  "AAABLXDR" varchar(3) NOT NULL default ' ',
  "AAABMRCU" varchar(3) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABFWLV" varchar(1) NOT NULL default ' ',
  "AAABLHGN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMLDW","AAABMLDY","AAABLXDR","AAABMRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENVM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABMPTG" varchar(1) NOT NULL default ' ',
  "AAABMEFK" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMREX","AAABEPCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABEPCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABEPCO","AAABMRHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMQCD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABMCRD" varchar(4) NOT NULL default ' ',
  "AAABIAXA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABIMHJ","AAABEPCO","AAABMCRD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABICIU" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABIAWX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABICIU","AAABEPCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABFKTJ" decimal(5,2) NOT NULL default '0.00',
  "AAABENYC" varchar(1) NOT NULL default ' ',
  "AAABENYD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABIMHJ","AAABEPCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABENYR" varchar(20) NOT NULL default ' ',
  "AAABENYS" varchar(1) NOT NULL default '0',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIGXH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABENYR","AAABENYS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABENZC" varchar(8) NOT NULL default '00000000',
  "AAABENYY" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABENZC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENZD" (
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABFYNM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENZH" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABGGHA" varchar(74) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLUEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENZN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABENZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABHMSB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMNC","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABKNCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABKNCK" varchar(2) NOT NULL default ' ',
  "AAABLPUZ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMNC","AAABKNCK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIGXH" varchar(1) NOT NULL default '0',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABEOEP" varchar(20) NOT NULL default ' ',
  "AAABEOBC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABIGXH","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEOCI" varchar(1) NOT NULL default '0',
  "AAABEOCJ" varchar(20) NOT NULL default ' ',
  "AAABEOCK" varchar(20) NOT NULL default ' ',
  "AAABIGXH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEOCI","AAABEOCJ","AAABEOCK","AAABIGXH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOBV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEOCZ" varchar(20) NOT NULL default ' ',
  "AAABEOCI" varchar(1) NOT NULL default '0',
  "AAABEOCJ" varchar(20) NOT NULL default ' ',
  "AAABEOCK" varchar(20) NOT NULL default ' ',
  "AAABEOCL" varchar(20) NOT NULL default ' ',
  "AAABIGXH" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEOCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOCQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEOCZ" varchar(20) NOT NULL default ' ',
  "AAABHMSB" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMNC","AAABEOCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEODA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEOEP" varchar(20) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABEODR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEOEP","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEODL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEOEP" varchar(20) NOT NULL default ' ',
  "AAABEODW" varchar(30) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABEODR" varchar(1) NOT NULL default ' ',
  "AAABEODS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEOEP","AAABEODW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEODX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEOEP" varchar(20) NOT NULL default ' ',
  "AAABEOEB" varchar(1) NOT NULL default ' ',
  "AAABEOEC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEOEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEOEP" varchar(20) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMNC","AAABEOEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOEQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABEOEU" varchar(1) NOT NULL default '0',
  "AAABEOEV" varchar(132) NOT NULL default ' ',
  "AAABEOEW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABEONB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABIMHJ","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOFM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOQD" varchar(8) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABIMHJ","AAABEPCO","AAABEOQD","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABLUEO","AAABIMHJ","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOQD" varchar(8) NOT NULL default ' ',
  "AAABEOIA" varchar(1) NOT NULL default ' ',
  "AAABEOGY" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABLUEO","AAABEOTZ","AAABIMHJ","AAABEPCO","AAABEOQD","AAABEOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABEOIA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUEO","AAABEOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABEOIA" varchar(1) NOT NULL default ' ',
  "AAABEOHV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMNC","AAABLUEO","AAABEOIA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABEOIG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMRHS","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOQD" varchar(8) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  "AAABEOIT" varchar(10) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABEOIV" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABIMHJ","AAABEPCO","AAABEOQD","AAABEOJJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOJD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABEOJJ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABIMHJ","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOJP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOQD" varchar(8) NOT NULL default ' ',
  "AAABEONB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABIMHJ","AAABEPCO","AAABEOQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOQD" varchar(8) NOT NULL default ' ',
  "AAABEPCC" varchar(12) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABEPCP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABIMHJ","AAABEPCO","AAABEOQD","AAABEPCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX","AAABEPCO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMQIJ" varchar(4) NOT NULL default ' ',
  "AAABEOLO" varchar(4) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABEOLJ" varchar(1) NOT NULL default ' ',
  "AAABEOLK" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMQIJ","AAABEOLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOLP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABIJIU" varchar(30) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEONB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEONB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEONB" varchar(10) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  "AAABKGNU" varchar(30) NOT NULL default ' ',
  "AAABKGNV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEONB","AAABEOTZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEONB" varchar(10) NOT NULL default ' ',
  "AAABEOMX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMNC","AAABEONB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEONC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEONK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEONR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMNC","AAABIMHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOQD" varchar(8) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABLUEO","AAABIMHJ","AAABEPCO","AAABEOQD","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEOPK" varchar(30) NOT NULL default ' ',
  "AAABLHCS" varchar(30) NOT NULL default ' ',
  "AAABEOPD" varchar(30) NOT NULL default ' ',
  "AAABLHCR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABLUEO","AAABIMHJ","AAABEOPK","AAABLHCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOPM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABLUEO" varchar(30) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOQD" varchar(8) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABMPMH","AAABLUEO","AAABIMHJ","AAABEPCO","AAABEOQD","AAABMREL","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABKGBL" varchar(1) NOT NULL default ' ',
  "AAABIHQV" varchar(4) NOT NULL default ' ',
  "AAABIHQW" varchar(2) NOT NULL default ' ',
  "AAABFOEJ" varchar(4) NOT NULL default ' ',
  "AAABFODY" varchar(8) NOT NULL default ' ',
  "AAABKNCF" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABKGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEORT" varchar(10) NOT NULL default ' ',
  "AAABKGBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEORT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEORA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEORT" varchar(10) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLMNC","AAABEORT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEORK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEORT" varchar(10) NOT NULL default ' ',
  "AAABLTLR" varchar(4) NOT NULL default '0000',
  "AAABLQSF" varchar(7) NOT NULL default ' ',
  "AAABEORQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEORT","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEORV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOSL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEOSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOSA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEOSL" varchar(2) NOT NULL default ' ',
  "AAABEOSE" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEOSL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(8) NOT NULL default ' ',
  "AAABEOSL" varchar(2) NOT NULL default ' ',
  "AAABEOSM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOZE" varchar(8) NOT NULL default ' ',
  "AAABHRYW" varchar(8) NOT NULL default ' ',
  "AAABHRYX" varchar(8) NOT NULL default ' ',
  "AAABHRYY" varchar(8) NOT NULL default ' ',
  "AAABEOSV" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLOZE" varchar(8) NOT NULL default ' ',
  "AAABMOHX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMPMH" varchar(20) NOT NULL default ' ',
  "AAABIMHJ" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEOTS" varchar(8) NOT NULL default ' ',
  "AAABLOZE" varchar(8) NOT NULL default ' ',
  "AAABEOTN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMPMH","AAABIMHJ","AAABEPCO","AAABEOTS","AAABLOZE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABEOUC" varchar(10) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEOTZ" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABEOUC","AAABMREF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOUQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEOUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEOUQ" varchar(2) NOT NULL default ' ',
  "AAABLPSU" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEOUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOUR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABLELA" varchar(20) NOT NULL default ' ',
  "AAABLELB" varchar(14) NOT NULL default ' ',
  "AAABEOVF" varchar(30) NOT NULL default ' ',
  "AAABLDEV" varchar(30) NOT NULL default ' ',
  "AAABEOUY" varchar(30) NOT NULL default ' ',
  "AAABMJWN" varchar(30) NOT NULL default ' ',
  "AAABEOVA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABLELA","AAABLELB","AAABEOVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLMNC" varchar(4) NOT NULL default ' ',
  "AAABLELA" varchar(20) NOT NULL default ' ',
  "AAABMRGK" varchar(14) NOT NULL default ' ',
  "AAABMNRG" varchar(2) NOT NULL default '00',
  "AAABLELB" varchar(14) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABEOVO" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLMNC","AAABLELA","AAABMRGK","AAABMNRG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOYB" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABJPEL" varchar(3) NOT NULL default ' ',
  "AAABMQBS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEOYB","AAABEPVW","AAABEPXB","AAABJPEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOWG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOYB" varchar(1) NOT NULL default ' ',
  "AAABEOWR" varchar(1) NOT NULL default '0',
  "AAABEOWS" varchar(1) NOT NULL default '0',
  "AAABEOWT" varchar(1) NOT NULL default '0',
  "AAABEOWM" varchar(1) NOT NULL default '0',
  "AAABLRQQ" varchar(4) NOT NULL default ' ',
  "AAABJSDF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABEOYB","AAABEOWR","AAABEOWS","AAABEOWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOYB" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEOYB","AAABEPVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOYB" varchar(1) NOT NULL default ' ',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEOXF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEOYB","AAABEPVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEOYB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEOYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEOYB" varchar(1) NOT NULL default ' ',
  "AAABLUDG" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABEOYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOXW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  "AAABEOYA" varchar(1) NOT NULL default ' ',
  "AAABEOYB" varchar(1) NOT NULL default ' ',
  "AAABMQDU" varchar(10) NOT NULL default ' ',
  "AAABLNYS" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABERDG" varchar(4) NOT NULL default ' ',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABEOYI" varchar(34) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVW","AAABEPUS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRJ" varchar(4) NOT NULL default ' ',
  "AAABMCWT" varchar(10) NOT NULL default ' ',
  "AAABEOYQ" varchar(34) NOT NULL default ' ',
  "AAABEOYR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRJ","AAABMCWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABEOYZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEOZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABEPXB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLZBM" varchar(4) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABEOZV" varchar(10) NOT NULL default ' ',
  "AAABEOZW" varchar(10) NOT NULL default ' ',
  "AAABLIMF" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLZBM","AAABEPXB","AAABEOZV","AAABEOZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEOZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHWH" varchar(16) NOT NULL default ' ',
  "AAABHHWI" varchar(8) NOT NULL default '00000000',
  "AAABEPAS" varchar(61) NOT NULL default ' ',
  "AAABEPAO" varchar(61) NOT NULL default ' ',
  "AAABEPAD" int(11) NOT NULL default '0',
  "AAABEPAE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHHWH","AAABHHWI","AAABEPAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHHWH" varchar(16) NOT NULL default ' ',
  "AAABHHWI" varchar(8) NOT NULL default '00000000',
  "AAABEPAS" varchar(61) NOT NULL default ' ',
  "AAABEPAO" varchar(61) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHHWH","AAABHHWI","AAABEPAS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEPCP" varchar(12) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABEPCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABEPCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEPCP" varchar(12) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEZHC","AAABEPCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEPCC" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABEPCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPBT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABEPCC" varchar(12) NOT NULL default ' ',
  "AAABEPBY" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEZHC","AAABEPCC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEZHC" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPCO" varchar(4) NOT NULL default ' ',
  "AAABEPCP" varchar(12) NOT NULL default ' ',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  "AAABEPCK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEZHC","AAABMREX","AAABEPCO","AAABEPCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPCQ" (
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABEPCU" varchar(1) NOT NULL default ' ',
  "AAABEPCV" varchar(1) NOT NULL default ' ',
  "AAABEPCW" varchar(1) NOT NULL default ' ',
  "AAABEPCX" varchar(1) NOT NULL default ' ',
  "AAABEPCY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPDA" (
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMMJT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPDG" (
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABLEEK" varchar(4) NOT NULL default ' ',
  "AAABEPGB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPFA"),
  KEY "AAABEPDL" ("AAABEPGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPDN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABEPDQ" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPDT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABEPDW" varchar(16) NOT NULL default ' ',
  "AAABEPDX" varchar(16) NOT NULL default ' ',
  "AAABEPDY" varchar(16) NOT NULL default ' ',
  "AAABEPDZ" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPEC" (
  "AAABLEEK" varchar(4) NOT NULL default ' ',
  "AAABLTQJ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABLEEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPEG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLEEK" varchar(4) NOT NULL default ' ',
  "AAABEPEJ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLEEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPEM" (
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPES" (
  "AAABEPFA" varchar(16) NOT NULL default ' ',
  "AAABEPEU" varchar(30) NOT NULL default ' ',
  "AAABEPEV" varchar(30) NOT NULL default ' ',
  "AAABEPEW" varchar(30) NOT NULL default ' ',
  "AAABEPEX" varchar(30) NOT NULL default ' ',
  "AAABEPEY" varchar(30) NOT NULL default ' ',
  "AAABEPEZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPFA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPFD" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPFS" varchar(4) NOT NULL default ' ',
  "AAABEPFI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPFL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPFS" varchar(4) NOT NULL default ' ',
  "AAABEPFP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEPFS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPFT" (
  "AAABEPGB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPFW" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPGB" varchar(4) NOT NULL default ' ',
  "AAABEPFZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEPGB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPGC" (
  "AAABKWJY" varchar(8) NOT NULL default ' ',
  "AAABMPMH" varchar(8) NOT NULL default ' ',
  "AAABFOMB" varchar(3) NOT NULL default '000',
  "AAABMNYU" varchar(30) NOT NULL default ' ',
  "AAABJAOB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKWJY","AAABMPMH","AAABFOMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPGL" (
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABLLNS" varchar(1) NOT NULL default ' ',
  "AAABEPGO" varchar(4) NOT NULL default ' ',
  "AAABEPGP" varchar(4) NOT NULL default ' ',
  "AAABEPGQ" varchar(4) NOT NULL default ' ',
  "AAABEPGR" varchar(4) NOT NULL default ' ',
  "AAABEPGS" varchar(4) NOT NULL default ' ',
  "AAABEPGT" varchar(4) NOT NULL default ' ',
  "AAABEPGU" varchar(1) NOT NULL default ' ',
  "AAABEPGV" varchar(1) NOT NULL default ' ',
  "AAABEPGW" varchar(1) NOT NULL default ' ',
  "AAABEPGX" varchar(1) NOT NULL default ' ',
  "AAABEPGY" varchar(1) NOT NULL default ' ',
  "AAABEPGZ" varchar(1) NOT NULL default ' ',
  "AAABEPHA" varchar(1) NOT NULL default ' ',
  "AAABEPHB" varchar(1) NOT NULL default ' ',
  "AAABEPHC" varchar(1) NOT NULL default ' ',
  "AAABEPHD" varchar(1) NOT NULL default ' ',
  "AAABEPHE" varchar(1) NOT NULL default ' ',
  "AAABEPOC" varchar(6) NOT NULL default ' ',
  "AAABEPHG" varchar(8) NOT NULL default ' ',
  "AAABEPHH" varchar(8) NOT NULL default ' ',
  "AAABEPHI" varchar(30) NOT NULL default ' ',
  "AAABEPHJ" varchar(8) NOT NULL default ' ',
  "AAABEPHK" varchar(20) NOT NULL default ' ',
  "AAABEPHL" varchar(30) NOT NULL default ' ',
  "AAABEPHM" varchar(30) NOT NULL default ' ',
  "AAABEPHN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPHP" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABEPIE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEPQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPHV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPIQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPIQ" varchar(4) NOT NULL default ' ',
  "AAABEPIE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEPIQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPII" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPIQ" varchar(4) NOT NULL default ' ',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABEPOT" varchar(1) NOT NULL default ' ',
  "AAABEPIN" varchar(4) NOT NULL default ' ',
  "AAABEPIO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPIQ","AAABEPQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPIS" (
  "AAABLLNS" varchar(1) NOT NULL default ' ',
  "AAABIANG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPIW" (
  "AAABEPJB" varchar(4) NOT NULL default ' ',
  "AAABJRHY" varchar(25) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPJC" (
  "AAABEPJF" varchar(2) NOT NULL default ' ',
  "AAABEPJE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPJG" (
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABEPJJ" varchar(3) NOT NULL default ' ',
  "AAABEPJK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPQQ","AAABEPKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPJN" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABEPJR" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEPQQ","AAABEPKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPJV" (
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABEPKB" varchar(4) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABEPJZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPQQ","AAABEPKB","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPKD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABJMGZ" varchar(4) NOT NULL default ' ',
  "AAABEPKH" varchar(30) NOT NULL default ' ',
  "AAABEPKI" varchar(30) NOT NULL default ' ',
  "AAABKVKB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ","AAABJMGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPKN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABEPKQ" varchar(4) NOT NULL default '0000',
  "AAABEPKR" varchar(4) NOT NULL default '0000',
  "AAABEPKS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABMBXE" varchar(10) NOT NULL default ' ',
  "AAABMBXF" varchar(10) NOT NULL default ' ',
  "AAABGBIE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPLQ" varchar(4) NOT NULL default ' ',
  "AAABEPLG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPLQ" varchar(4) NOT NULL default ' ',
  "AAABEPLN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEPLQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPLR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJMGZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJMGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPLW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABJMGZ" varchar(4) NOT NULL default ' ',
  "AAABEPMA" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABJMGZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABEPMI" smallint(6) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFEAL" varchar(4) NOT NULL default ' ',
  "AAABGNDO" varchar(2) NOT NULL default ' ',
  "AAABLDTB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFEAL","AAABGNDO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPNH" varchar(2) NOT NULL default ' ',
  "AAABEPMX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEPNH" varchar(2) NOT NULL default ' ',
  "AAABEPNE" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEPNH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPNN" (
  "AAABMKFH" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMKFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLUAV","AAABKAFO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPNX" (
  "AAABLLNS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPOA" (
  "AAABLLNS" varchar(1) NOT NULL default ' ',
  "AAABEPOC" varchar(6) NOT NULL default ' ',
  "AAABEPOD" varchar(1) NOT NULL default ' ',
  "AAABEPOE" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPOG" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLLNS" varchar(1) NOT NULL default ' ',
  "AAABEPOJ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLLNS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPOM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABEPOT" varchar(1) NOT NULL default ' ',
  "AAABEPOQ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ","AAABEPOT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPOU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABMGHG" varchar(10) NOT NULL default ' ',
  "AAABEPOY" varchar(10) NOT NULL default ' ',
  "AAABEPOZ" varchar(10) NOT NULL default ' ',
  "AAABEPPA" varchar(10) NOT NULL default ' ',
  "AAABEPPB" varchar(10) NOT NULL default ' ',
  "AAABMNZR" varchar(8) NOT NULL default '00000000',
  "AAABKGFD" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMCZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPPG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCZQ" varchar(16) NOT NULL default ' ',
  "AAABEPPK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCZQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABEPPX" varchar(10) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABEPPX","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPPZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKWJY" varchar(10) NOT NULL default ' ',
  "AAABFJOS" varchar(10) NOT NULL default '0000000000',
  "AAABEPQD" varchar(4) NOT NULL default ' ',
  "AAABEPQE" varchar(4) NOT NULL default ' ',
  "AAABEPQF" varchar(36) NOT NULL default ' ',
  "AAABEPQG" varchar(36) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKWJY","AAABFJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPQQ" varchar(4) NOT NULL default ' ',
  "AAABFJOE" int(11) NOT NULL default '0',
  "AAABFJOF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPQQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPQR" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  "AAABEVFX" varchar(30) NOT NULL default ' ',
  "AAABJRLS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKKAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPQY" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABKKAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPRD" (
  "AAABLUXQ" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKKAN" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLUXQ","AAABMRHS","AAABKKAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPRL" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEPSA" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABMOHE" varchar(10) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAABEPRS" varchar(12) NOT NULL default ' ',
  "AAABEPRT" varchar(8) NOT NULL default '00000000',
  "AAABEPRU" varchar(6) NOT NULL default '000000',
  "AAABEPRV" varchar(12) NOT NULL default ' ',
  "AAABEPRW" varchar(8) NOT NULL default '00000000',
  "AAABEPRX" varchar(6) NOT NULL default '000000',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEPSA","AAABLTXI","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPSD" (
  "AAABINAW" varchar(1) NOT NULL default ' ',
  "AAABEPSM" varchar(4) NOT NULL default ' ',
  "AAABLTXI" varchar(2) NOT NULL default ' ',
  "AAABLEST" varchar(2) NOT NULL default ' ',
  "AAABMOHE" varchar(10) NOT NULL default ' ',
  "AAABEZLL" varchar(60) NOT NULL default ' ',
  "AAABEPSK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABINAW","AAABEPSM","AAABLTXI","AAABLEST")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPSP" (
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  "AAABKMWI" varchar(2) NOT NULL default '00',
  "AAABLENG" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABKMLY","AAABKMLZ","AAABKMWI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPSX" (
  "AAABEPTB" varchar(20) NOT NULL default ' ',
  "AAABKMLY" varchar(4) NOT NULL default ' ',
  "AAABKMLZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABEPTB","AAABKMLY","AAABKMLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGACW" varchar(3) NOT NULL default ' ',
  "AAABHSJX" varchar(2) NOT NULL default ' ',
  "AAABGACU" varchar(1) NOT NULL default ' ',
  "AAABEPTJ" varchar(30) NOT NULL default ' ',
  "AAABEPTK" varchar(30) NOT NULL default ' ',
  "AAABEPTL" varchar(30) NOT NULL default ' ',
  "AAABEPTM" varchar(30) NOT NULL default ' ',
  "AAABEPTN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGACW","AAABHSJX","AAABGACU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPTS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABGACW" varchar(3) NOT NULL default ' ',
  "AAABEPTW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABGACW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPUA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTD" varchar(1) NOT NULL default ' ',
  "AAABEPUI" varchar(1) NOT NULL default ' ',
  "AAABLODQ" varchar(3) NOT NULL default '000',
  "AAABMCMO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXTD","AAABEPUI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPUO" varchar(2) NOT NULL default ' ',
  "AAABEPUM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABETQU" varchar(6) NOT NULL default ' ',
  "AAABEPUS" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABETQU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPUV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABEPUY" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHZ" varchar(10) NOT NULL default ' ',
  "AAABEPVE" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJKTY" varchar(3) NOT NULL default ' ',
  "AAABEPVK" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJKTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPVN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPVS" varchar(3) NOT NULL default ' ',
  "AAABEPVQ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEPVS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLQVJ" varchar(1) NOT NULL default '0',
  "AAABEPVW" varchar(2) NOT NULL default ' ',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAABEPWA" varchar(3) NOT NULL default '000',
  "AAABMQHX" varchar(3) NOT NULL default ' ',
  "AAABEPWC" varchar(3) NOT NULL default ' ',
  "AAABEPWD" varchar(1) NOT NULL default ' ',
  "AAABEPWE" varchar(1) NOT NULL default ' ',
  "AAABEPWF" varchar(1) NOT NULL default ' ',
  "AAABEPWG" varchar(30) NOT NULL default ' ',
  "AAABEPWH" varchar(30) NOT NULL default ' ',
  "AAABEPWI" varchar(30) NOT NULL default ' ',
  "AAABEPWJ" varchar(1) NOT NULL default ' ',
  "AAABEPWK" varchar(1) NOT NULL default ' ',
  "AAABEPWL" varchar(1) NOT NULL default ' ',
  "AAABEPWM" varchar(30) NOT NULL default ' ',
  "AAABEPWN" varchar(30) NOT NULL default ' ',
  "AAABEPWO" varchar(30) NOT NULL default ' ',
  "AAABEPWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPWS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEPXB" varchar(2) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(10) NOT NULL default ' ',
  "AAABMNNA" varchar(10) NOT NULL default ' ',
  "AAABLQVJ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABEPXB","AAABMNML","AAABMNMY","AAABMNMZ","AAABMNNA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFJCO" varchar(3) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABEPXT" varchar(3) NOT NULL default ' ',
  "AAABEPXU" varchar(3) NOT NULL default ' ',
  "AAABEPXV" varchar(10) NOT NULL default ' ',
  "AAABEPXW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFJCO","AAABMNML","AAABMNMY","AAABEPXT","AAABEPXU","AAABEPXV","AAABEPXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABFIQY" varchar(4) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABEPYI" varchar(21) NOT NULL default ' ',
  "AAABEPYJ" varchar(21) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABFIQY","AAABMNML","AAABMNMY","AAABEPYI","AAABEPYJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKDN" varchar(3) NOT NULL default '000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAABLZKF" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABIKDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEPYT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABEPYW" varchar(50) NOT NULL default ' ',
  "AAABEPYX" varchar(50) NOT NULL default ' ',
  "AAABEPYY" varchar(50) NOT NULL default ' ',
  "AAABEPYZ" varchar(50) NOT NULL default ' ',
  "AAABEQSU" varchar(16) NOT NULL default ' ',
  "AAABEPZB" varchar(1) NOT NULL default ' ',
  "AAABERIX" varchar(1) NOT NULL default ' ',
  "AAABEPZD" varchar(11) NOT NULL default ' ',
  "AAABEPZE" varchar(4) NOT NULL default '0000',
  "AAABEPZF" varchar(1) NOT NULL default ' ',
  "AAABEPZG" varchar(5) NOT NULL default '00000',
  "AAABEPZH" varchar(5) NOT NULL default '00000',
  "AAABEPZI" varchar(5) NOT NULL default '00000',
  "AAABEPZJ" varchar(5) NOT NULL default '00000',
  "AAABEPZK" varchar(5) NOT NULL default '00000',
  "AAABEPZL" varchar(1) NOT NULL default ' ',
  "AAABEPZM" varchar(4) NOT NULL default '0000',
  "AAABEPZN" varchar(1) NOT NULL default ' ',
  "AAABEPZO" varchar(1) NOT NULL default ' ',
  "AAABEPZP" varchar(1) NOT NULL default ' ',
  "AAABEPZQ" varchar(1) NOT NULL default ' ',
  "AAABEPZR" varchar(1) NOT NULL default '0',
  "AAABEPZS" varchar(11) NOT NULL default ' ',
  "AAABEPZT" varchar(1) NOT NULL default ' ',
  "AAABEPZU" varchar(1) NOT NULL default ' ',
  "AAABEPZV" varchar(1) NOT NULL default ' ',
  "AAABEPZW" varchar(1) NOT NULL default ' ',
  "AAABEPZX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMLPV" varchar(6) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMLPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQAI" (
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAABEQAK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABKEXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQAM" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAABEQBA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQDV","AAABKEXE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQAS" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABEQAU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQAW" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABKEXE" varchar(1) NOT NULL default ' ',
  "AAABEQBD" varchar(8) NOT NULL default ' ',
  "AAABEQBA" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQDV","AAABKEXE","AAABEQBD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQBE" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABEQDM" varchar(1) NOT NULL default ' ',
  "AAABEQDN" varchar(1) NOT NULL default ' ',
  "AAABEQBO" varchar(3) NOT NULL default '000',
  "AAABEQBJ" smallint(6) NOT NULL default '0',
  "AAABEQBK" blob,
  PRIMARY KEY ("AAABEQDV","AAABEQDM","AAABEQDN","AAABEQBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQBP" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABEQDM" varchar(1) NOT NULL default ' ',
  "AAABEQDN" varchar(1) NOT NULL default ' ',
  "AAABEQBT" int(11) NOT NULL default '0',
  "AAABEQBU" int(11) NOT NULL default '0',
  "AAABEQBV" int(11) NOT NULL default '0',
  "AAABEQBW" int(11) NOT NULL default '0',
  "AAABEQBX" varchar(30) NOT NULL default ' ',
  "AAABEQBY" varchar(30) NOT NULL default ' ',
  "AAABEQBZ" varchar(1) NOT NULL default ' ',
  "AAABEQCA" varchar(30) NOT NULL default ' ',
  "AAABEQCB" varchar(1) NOT NULL default ' ',
  "AAABEQCC" varchar(1) NOT NULL default ' ',
  "AAABEQCD" varchar(50) NOT NULL default ' ',
  "AAABEQCE" varchar(10) NOT NULL default ' ',
  "AAABEQDK" varchar(50) NOT NULL default ' ',
  "AAABEQCG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABEQDV","AAABEQDM","AAABEQDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQCK" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABEQDM" varchar(1) NOT NULL default ' ',
  "AAABEQDN" varchar(1) NOT NULL default ' ',
  "AAABEQCO" varchar(4) NOT NULL default '0000',
  "AAABEQCP" varchar(1) NOT NULL default ' ',
  "AAABEQDK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQDV","AAABEQDM","AAABEQDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQCU" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABEQDM" varchar(1) NOT NULL default ' ',
  "AAABEQDN" varchar(1) NOT NULL default ' ',
  "AAABEQDE" varchar(3) NOT NULL default '000',
  "AAABLBDX" varchar(72) NOT NULL default ' ',
  "AAABEQDK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQDV","AAABEQDM","AAABEQDN","AAABEQDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQDF" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABEQDM" varchar(1) NOT NULL default ' ',
  "AAABEQDN" varchar(1) NOT NULL default ' ',
  "AAABEQDJ" varchar(72) NOT NULL default ' ',
  "AAABEQDK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQDV","AAABEQDM","AAABEQDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQDO" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABEQDQ" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQDS" (
  "AAABEQDV" varchar(8) NOT NULL default ' ',
  "AAABEQDU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQDW" (
  "AAABEQDZ" varchar(16) NOT NULL default ' ',
  "AAABEQDY" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQDZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQEA" (
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
  KEY "AAABEQFQ" ("AAABJAAI"),
  KEY "AAABEQFS" ("AAABHZOK","AAABHZOR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQFV" (
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
  KEY "AAABEQGR" ("AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQGU" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABLGEY" varchar(32) NOT NULL default ' ',
  "AAABJAAI" varchar(15) NOT NULL default ' ',
  "AAABHZQZ" varchar(15) NOT NULL default ' ',
  "AAABMRHK" varchar(32) NOT NULL default ' ',
  "AAABHZRB" varchar(12) NOT NULL default ' ',
  "AAABMRCK" varchar(75) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABLGEY","AAABJAAI","AAABHZQZ","AAABMRHK","AAABHZRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABKPVU" varchar(1) NOT NULL default ' ',
  "AAABKPWB" varchar(2) NOT NULL default ' ',
  "AAABKPWC" varchar(2) NOT NULL default ' ',
  "AAABKPVZ" varchar(2) NOT NULL default ' ',
  "AAABEQIX" varchar(1) NOT NULL default ' ',
  "AAABKPWH" varchar(1) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default ' ',
  "AAABKPXW" varchar(3) NOT NULL default '000',
  "AAABKPXN" varchar(1) NOT NULL default ' ',
  "AAABHOOJ" varchar(4) NOT NULL default ' ',
  "AAABKPWF" varchar(10) NOT NULL default ' ',
  "AAABKPYO" varchar(2) NOT NULL default ' ',
  "AAABEQHX" varchar(2) NOT NULL default ' ',
  "AAABKPWI" varchar(2) NOT NULL default ' ',
  "AAABKPWJ" varchar(2) NOT NULL default ' ',
  "AAABEQIA" varchar(2) NOT NULL default ' ',
  "AAABEQIB" varchar(3) NOT NULL default '000',
  "AAABEQIC" varchar(3) NOT NULL default '000',
  "AAABKPXA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABEQIK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKPVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQIO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABKPXJ" varchar(2) NOT NULL default ' ',
  "AAABKPXK" varchar(4) NOT NULL default ' ',
  "AAABKPXM" varchar(1) NOT NULL default ' ',
  "AAABKPXN" varchar(1) NOT NULL default ' ',
  "AAABLZAP" varchar(2) NOT NULL default ' ',
  "AAABKPXT" varchar(4) NOT NULL default ' ',
  "AAABEQIX" varchar(1) NOT NULL default ' ',
  "AAABKPWH" varchar(1) NOT NULL default ' ',
  "AAABKPXV" varchar(2) NOT NULL default ' ',
  "AAABKPXW" varchar(3) NOT NULL default '000',
  "AAABEQJB" varchar(1) NOT NULL default ' ',
  "AAABKPZD" varchar(1) NOT NULL default ' ',
  "AAABLXHW" varchar(2) NOT NULL default ' ',
  "AAABEQJE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKPVV","AAABKPXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKPVV" varchar(2) NOT NULL default ' ',
  "AAABKPXJ" varchar(2) NOT NULL default ' ',
  "AAABEQJN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKPVV","AAABKPXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQJS" (
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQJU" varchar(1) NOT NULL default ' ',
  "AAABEQJV" varchar(1) NOT NULL default ' ',
  "AAABEQJW" varchar(1) NOT NULL default ' ',
  "AAABEQJX" varchar(1) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABEQJZ" varchar(2) NOT NULL default '00',
  "AAABEQKA" varchar(30) NOT NULL default ' ',
  "AAABEQKB" varchar(28) NOT NULL default ' ',
  "AAABEQNK" varchar(2) NOT NULL default ' ',
  "AAABEQKD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQKF" (
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQOP","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABMPMH" varchar(2) NOT NULL default '00',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQOP","AAABMPMH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQKT" (
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQLB" varchar(250) NOT NULL default ' ',
  "AAABEQLC" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQKY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQLB" varchar(250) NOT NULL default ' ',
  "AAABEQLC" varchar(250) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQLF" (
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQMX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQMX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQLP" (
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQMA" varchar(1) NOT NULL default ' ',
  "AAABEQMB" varchar(1) NOT NULL default ' ',
  "AAABEQMC" varchar(1) NOT NULL default ' ',
  "AAABEQMD" varchar(1) NOT NULL default ' ',
  "AAABEQME" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQLX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQMA" varchar(1) NOT NULL default ' ',
  "AAABEQMB" varchar(1) NOT NULL default ' ',
  "AAABEQMC" varchar(1) NOT NULL default ' ',
  "AAABEQMD" varchar(1) NOT NULL default ' ',
  "AAABEQME" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQMH" (
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQMX" varchar(30) NOT NULL default ' ',
  "AAABEQMY" varchar(30) NOT NULL default ' ',
  "AAABMFSJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQOP","AAABEQMX","AAABEQMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQMX" varchar(30) NOT NULL default ' ',
  "AAABEQMY" varchar(30) NOT NULL default ' ',
  "AAABMFSJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQOP","AAABEQMX","AAABEQMY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQMZ" (
  "AAABEQNC" varchar(2) NOT NULL default ' ',
  "AAABEQNB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQNC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQND" (
  "AAABEQNK" varchar(2) NOT NULL default ' ',
  "AAABEQNF" varchar(30) NOT NULL default ' ',
  "AAABEQNG" varchar(30) NOT NULL default ' ',
  "AAABEQNH" varchar(30) NOT NULL default ' ',
  "AAABEQNI" varchar(30) NOT NULL default ' ',
  "AAABHHPG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQNK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQNL" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQNO" varchar(50) NOT NULL default ' ',
  "AAABEQNP" varchar(28) NOT NULL default ' ',
  "AAABEQNQ" varchar(28) NOT NULL default ' ',
  "AAABEQNR" varchar(28) NOT NULL default ' ',
  "AAABEQNS" varchar(28) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEQOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQNV" (
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQOQ" varchar(6) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABJSDF" varchar(3) NOT NULL default '000',
  "AAABLMLB" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABEQOP","AAABEQOQ","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQOE" (
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQOQ" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQOP","AAABEQOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQOJ" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEQOP" varchar(30) NOT NULL default ' ',
  "AAABEQOQ" varchar(6) NOT NULL default ' ',
  "AAABEQON" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABEQOP","AAABEQOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQQC" varchar(30) NOT NULL default ' ',
  "AAABEQOU" varchar(1) NOT NULL default ' ',
  "AAABEQOV" varchar(1) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEQQC" varchar(30) NOT NULL default ' ',
  "AAABEQPD" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEQQC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQPH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEQQC" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABFLVN" varchar(1) NOT NULL default '0',
  "AAABEQPZ" varchar(150) NOT NULL default ' ',
  "AAABEQQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEQQC","AAABMOHM","AAABFLVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQPU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQQC" varchar(30) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  "AAABFLVN" varchar(1) NOT NULL default '0',
  "AAABEQPZ" varchar(150) NOT NULL default ' ',
  "AAABEQQA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQQC","AAABMOHM","AAABFLVN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABMLQA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQQS" (
  "AAABHLLX" varchar(4) NOT NULL default ' ',
  "AAABEQRX" varchar(1) NOT NULL default ' ',
  "AAABEQQV" varchar(3) NOT NULL default ' ',
  "AAABEQQW" varchar(3) NOT NULL default ' ',
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABHLLX","AAABEQRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQRA" (
  "AAABHLLX" varchar(4) NOT NULL default ' ',
  "AAABEQRX" varchar(1) NOT NULL default ' ',
  "AAABEQRD" varchar(40) NOT NULL default ' ',
  "AAABEQRE" varchar(60) NOT NULL default ' ',
  "AAABEQRF" varchar(3) NOT NULL default ' ',
  "AAABEQRG" varchar(40) NOT NULL default ' ',
  "AAABEQRH" varchar(60) NOT NULL default ' ',
  "AAABEQRI" varchar(27) NOT NULL default ' ',
  "AAABEQRJ" varchar(60) NOT NULL default ' ',
  "AAABEQRK" varchar(40) NOT NULL default ' ',
  "AAABEQRL" varchar(40) NOT NULL default ' ',
  "AAABEQRM" varchar(40) NOT NULL default ' ',
  "AAABEQRN" varchar(27) NOT NULL default ' ',
  "AAABEQRO" varchar(60) NOT NULL default ' ',
  "AAABEQRP" varchar(40) NOT NULL default ' ',
  "AAABEQRQ" varchar(40) NOT NULL default ' ',
  "AAABEQRR" varchar(40) NOT NULL default ' ',
  "AAABEQRS" varchar(1) NOT NULL default ' ',
  "AAABEQRT" varchar(1) NOT NULL default ' ',
  "AAABEQRU" varchar(1) NOT NULL default ' ',
  "AAABEQRV" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABHLLX","AAABEQRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQRY" (
  "AAABMNDU" varchar(40) NOT NULL default ' ',
  "AAABJETV" varchar(30) NOT NULL default ' ',
  "AAABEQSG" varchar(20) NOT NULL default ' ',
  "AAABEQSH" varchar(20) NOT NULL default ' ',
  "AAABEQSD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMNDU","AAABJETV","AAABEQSG","AAABEQSH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQSI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEQSU" varchar(16) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEQSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQSU" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQSU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQSV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLUNI" varchar(2) NOT NULL default ' ',
  "AAABLRTA" varchar(32) NOT NULL default ' ',
  "AAABLUMP" int(11) NOT NULL default '0',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  "AAABLUML" smallint(6) NOT NULL default '0',
  "AAABLUMM" blob,
  PRIMARY KEY ("AAABMRIL","AAABLUNI","AAABLRTA","AAABLUMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQTJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMY" varchar(2) NOT NULL default ' ',
  "AAABEQTT" varchar(1) NOT NULL default ' ',
  "AAABEQTN" varchar(1) NOT NULL default ' ',
  "AAABEQTO" varchar(1) NOT NULL default ' ',
  "AAABEQTP" varchar(1) NOT NULL default ' ',
  "AAABEQTQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMY","AAABEQTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXKV" varchar(12) NOT NULL default ' ',
  "AAABJEGO" varchar(2) NOT NULL default ' ',
  "AAABEQUD" varchar(12) NOT NULL default ' ',
  "AAABLILW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXKV"),
  KEY "AAABEQUC" ("AAABEQUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKRBY" varchar(7) NOT NULL default ' ',
  "AAABEQUN" varchar(2) NOT NULL default ' ',
  "AAABMLZS" varchar(4) NOT NULL default '0000',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKRBY","AAABEQUN","AAABMLZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEQVB" varchar(14) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEQVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQVB" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQVB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQVC" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMKGJ" varchar(30) NOT NULL default ' ',
  "AAABLPWW" varchar(74) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABMKGJ"),
  KEY "AAABEQVI" ("AAABMKGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQVK" (
  "AAABEQVP" varchar(40) NOT NULL default ' ',
  "AAABLUAV" varchar(30) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABMLES" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABEQVP","AAABLUAV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQVR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXUT" varchar(8) NOT NULL default ' ',
  "AAABKXTW" varchar(6) NOT NULL default ' ',
  "AAABLYOO" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLXUT","AAABKXTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXTW" varchar(6) NOT NULL default ' ',
  "AAABLYOO" varchar(12) NOT NULL default ' ',
  "AAABJUPI" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXTW","AAABLYOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQWH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXTW" varchar(6) NOT NULL default ' ',
  "AAABLYOO" varchar(12) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXTW","AAABLYOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQWR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKXTW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKXTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHBAO" varchar(4) NOT NULL default ' ',
  "AAABEQXE" varchar(3) NOT NULL default ' ',
  "AAABLHXQ" varchar(2) NOT NULL default ' ',
  "AAABKXTW" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHBAO","AAABEQXE","AAABLHXQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABKXTW" varchar(6) NOT NULL default ' ',
  "AAABMAHQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABKXTW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQXO" (
  "AAABMMZL" varchar(2) NOT NULL default ' ',
  "AAABEQXQ" varchar(10) NOT NULL default '0000000000',
  "AAABEQXR" varchar(10) NOT NULL default '0000000000',
  "AAABGHOW" varchar(2) NOT NULL default '00',
  "AAABGHOY" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQXV" (
  "AAABEQYB" varchar(2) NOT NULL default ' ',
  "AAABEQXX" varchar(10) NOT NULL default '0000000000',
  "AAABEQXY" varchar(10) NOT NULL default '0000000000',
  "AAABEQZP" varchar(4) NOT NULL default '0000',
  "AAABEQZQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABEQYB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQYC" (
  "AAABEQYH" varchar(30) NOT NULL default ' ',
  "AAABIOFM" varchar(30) NOT NULL default ' ',
  "AAABGYKT" varchar(30) NOT NULL default ' ',
  "AAABIOFN" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABEQYH","AAABIOFM"),
  KEY "AAABEQYJ" ("AAABGYKT","AAABIOFN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQYM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQYU" varchar(1) NOT NULL default ' ',
  "AAABEQYP" varchar(3) NOT NULL default ' ',
  "AAABEQYQ" varchar(10) NOT NULL default ' ',
  "AAABEQYR" varchar(3) NOT NULL default ' ',
  "AAABEQYS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQYU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQYV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEQZH" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEQZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQZA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABEQZH" varchar(12) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABEQZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQZI" (
  "AAABMMZL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQZL" (
  "AAABMMZL" varchar(2) NOT NULL default ' ',
  "AAABFPKP" varchar(10) NOT NULL default '0000000000',
  "AAABGRSI" varchar(10) NOT NULL default '0000000000',
  "AAABEQZP" varchar(10) NOT NULL default '0000000000',
  "AAABEQZQ" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMMZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEQZS" (
  "AAABERBG" varchar(10) NOT NULL default ' ',
  "AAABERBH" varchar(4) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  "AAABERBJ" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABERBG","AAABERBH","AAABMALF","AAABERBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERAB" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABERBG" varchar(10) NOT NULL default ' ',
  "AAABERBH" varchar(4) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  "AAABERBJ" varchar(8) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABERBG","AAABERBH","AAABMALF","AAABERBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERAN" (
  "AAABERBG" varchar(10) NOT NULL default ' ',
  "AAABERBH" varchar(4) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  "AAABERBJ" varchar(8) NOT NULL default ' ',
  "AAABERAS" varchar(1) NOT NULL default ' ',
  "AAABERAT" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABERBG","AAABERBH","AAABMALF","AAABERBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERAY" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABERBG" varchar(10) NOT NULL default ' ',
  "AAABERBH" varchar(4) NOT NULL default ' ',
  "AAABMALF" varchar(3) NOT NULL default '000',
  "AAABERBJ" varchar(8) NOT NULL default ' ',
  "AAABLXIS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAABERBG","AAABERBH","AAABMALF","AAABERBJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERBK" (
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABHBUL" varchar(1) NOT NULL default ' ',
  "AAABHBUM" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABKMZH","AAABHBUL","AAABHBUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERBV" (
  "AAABIRUN" varchar(30) NOT NULL default ' ',
  "AAABKMZH" varchar(1) NOT NULL default ' ',
  "AAABHBUL" varchar(1) NOT NULL default ' ',
  "AAABHBUM" varchar(1) NOT NULL default ' ',
  "AAABIIUG" varchar(1) NOT NULL default ' ',
  "AAABLGEV" varchar(12) NOT NULL default ' ',
  "AAABLGEW" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABIRUN","AAABKMZH","AAABHBUL","AAABHBUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABERDG" varchar(4) NOT NULL default ' ',
  "AAABERDB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERCP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERDB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABERDB" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABERDB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERDC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABERDG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABERDG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAABHPIX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABMCPV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERDP" (
  "AAABEREB" varchar(2) NOT NULL default ' ',
  "AAABERDR" varchar(1) NOT NULL default ' ',
  "AAABERDS" varchar(1) NOT NULL default ' ',
  "AAABERDT" varchar(1) NOT NULL default ' ',
  "AAABERDU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABEREB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABERDW" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABEREB" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABEREB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAABEREC" (
  "AAABLUSV" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABLUSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
