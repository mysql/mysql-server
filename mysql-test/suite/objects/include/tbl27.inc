# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+
# |                             tbl27.inc                              |
# |                        Copyright 2006 by MySQL AB                  |
# +--------------------------------------------------------------------+
# |                         FOR INTERNAL USE ONLY!                     |
# +--------------------------------------------------------------------+

--disable_query_log
CREATE TABLE "AAAAYFML" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFNX" varchar(5) NOT NULL default ' ',
  "AAAAYFNP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYFNX","AAAAYFNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFNX" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFNP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYFNX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFNX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYFNX" varchar(5) NOT NULL default ' ',
  "AAAAYFNP" varchar(2) NOT NULL default '00',
  "AAAAYFNL" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYFNX","AAAAYFNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYFNX" varchar(5) NOT NULL default ' ',
  "AAAAYFNU" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYFNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFNY" (
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABMREL" varchar(30) NOT NULL default ' ',
  "AAABMREF" varchar(30) NOT NULL default ' ',
  "AAABIYBF" varchar(40) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMMJT" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMAMP","AAABMREL","AAABMREF","AAABIYBF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFOQ" varchar(2) NOT NULL default ' ',
  "AAAAYFOO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFOQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIHQ" varchar(4) NOT NULL default ' ',
  "AAABHIOX" varchar(10) NOT NULL default ' ',
  "AAAAYFOV" varchar(25) NOT NULL default ' ',
  "AAABJGBU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFPW" varchar(4) NOT NULL default ' ',
  "AAAAYIHQ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFPW","AAAAYIHQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFPW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYFPW" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYFPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGIS" varchar(4) NOT NULL default ' ',
  "AAABLPWW" varchar(15) NOT NULL default ' ',
  "AAAAYFQB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGIS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIHQ" varchar(4) NOT NULL default ' ',
  "AAAAYGIS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOYX" varchar(3) NOT NULL default '000',
  "AAAAYFQL" decimal(6,2) NOT NULL default '0.00',
  "AAAAYFQM" decimal(5,3) NOT NULL default '0.000',
  "AAAAYFQN" decimal(5,3) NOT NULL default '0.000',
  "AAAAYGDH" decimal(5,3) NOT NULL default '0.000',
  "AAAAYFQP" decimal(6,2) NOT NULL default '0.00',
  "AAAAYFQQ" decimal(5,3) NOT NULL default '0.000',
  "AAAAYFQR" decimal(5,3) NOT NULL default '0.000',
  "AAAAYFQS" decimal(5,3) NOT NULL default '0.000',
  "AAAAYFQT" varchar(4) NOT NULL default ' ',
  "AAAAYFQU" varchar(4) NOT NULL default ' ',
  "AAAAYFQV" varchar(4) NOT NULL default ' ',
  "AAAAYFQW" varchar(4) NOT NULL default ' ',
  "AAAAYFQX" varchar(4) NOT NULL default ' ',
  "AAAAYFQY" varchar(4) NOT NULL default ' ',
  "AAAAYFQZ" varchar(4) NOT NULL default ' ',
  "AAAAYFRA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIHQ","AAAAYGIS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFXI" varchar(2) NOT NULL default '00',
  "AAABGCHT" varchar(2) NOT NULL default '00',
  "AAAAYLJE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFXI","AAABGCHT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIHQ" varchar(4) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIHQ","AAABMQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLJE" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAAAYIHQ" varchar(4) NOT NULL default ' ',
  "AAAAYGIS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLJE","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOYX" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFSL" varchar(4) NOT NULL default ' ',
  "AAAAYFSM" decimal(12,2) NOT NULL default '0.00',
  "AAAAYFSN" varchar(4) NOT NULL default ' ',
  "AAAAYFSO" varchar(4) NOT NULL default ' ',
  "AAAAYFSP" decimal(12,2) NOT NULL default '0.00',
  "AAAAYFSQ" varchar(4) NOT NULL default ' ',
  "AAAAYFSR" varchar(4) NOT NULL default ' ',
  "AAAAYFSS" decimal(12,2) NOT NULL default '0.00',
  "AAAAYFST" varchar(4) NOT NULL default ' ',
  "AAAAYFSU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYOYX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFSY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOYX" varchar(3) NOT NULL default '000',
  "AAABLUVE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYFTF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYFTG" decimal(9,2) NOT NULL default '0.00',
  "AAAAYFTH" decimal(9,2) NOT NULL default '0.00',
  "AAAAYLRH" varchar(1) NOT NULL default ' ',
  "AAAAYFTJ" decimal(6,3) NOT NULL default '0.000',
  "AAAAZNIJ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYFTL" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYOYX","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFTQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLJE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYOYX" varchar(3) NOT NULL default '000',
  "AAAAYFTZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYOYX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGKS" varchar(2) NOT NULL default '00',
  "AAAAYFUX" varchar(2) NOT NULL default '00',
  "AAAAYFUY" varchar(7) NOT NULL default ' ',
  "AAAAYGCR" varchar(3) NOT NULL default '000',
  "AAABGZRZ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFUM" varchar(7) NOT NULL default ' ',
  "AAABKCJP" decimal(6,3) NOT NULL default '0.000',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAAAYFUP" varchar(1) NOT NULL default ' ',
  "AAAAYFUQ" varchar(4) NOT NULL default ' ',
  "AAAAYFUR" varchar(30) NOT NULL default ' ',
  "AAAAYFUS" varchar(4) NOT NULL default ' ',
  "AAAAYFUT" varchar(6) NOT NULL default '000000',
  "AAAAYFUU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGKS","AAAAYFUX","AAAAYFUY","AAAAYGCR","AAABGZRZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFVC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGKS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYGKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFVH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYGKS" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYGKS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGHA" varchar(3) NOT NULL default ' ',
  "AAAAYIHQ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYGHA","AAAAYIHQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGHA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYGHA" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYGHA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLJE" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYIHQ" varchar(4) NOT NULL default ' ',
  "AAAAYGIS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLJE","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFWY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYGXZ" varchar(9) NOT NULL default ' ',
  "AAAAYGYA" varchar(5) NOT NULL default ' ',
  "AAAAYFXE" varchar(4) NOT NULL default ' ',
  "AAAAYFXF" varchar(25) NOT NULL default ' ',
  "AAAAYFXG" varchar(18) NOT NULL default ' ',
  "AAAAYFXH" varchar(15) NOT NULL default ' ',
  "AAAAYFXI" varchar(2) NOT NULL default '00',
  "AAABGHJP" varchar(1) NOT NULL default ' ',
  "AAAAYFZS" varchar(4) NOT NULL default ' ',
  "AAAAYGJY" varchar(2) NOT NULL default ' ',
  "AAAAYGEF" varchar(2) NOT NULL default '00',
  "AAAAYGAR" varchar(2) NOT NULL default ' ',
  "AAAAYFXO" varchar(25) NOT NULL default ' ',
  "AAAAYFXP" varchar(15) NOT NULL default ' ',
  "AAAAYGNN" varchar(4) NOT NULL default ' ',
  "AAAAYGLX" varchar(2) NOT NULL default ' ',
  "AAAAYGJF" varchar(2) NOT NULL default ' ',
  "AAAAYFXT" varchar(2) NOT NULL default ' ',
  "AAAAYFXU" varchar(2) NOT NULL default ' ',
  "AAAAYFXV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFXZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFZS" varchar(4) NOT NULL default ' ',
  "AAAAYFYN" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFYF" varchar(2) NOT NULL default ' ',
  "AAAAYFYG" varchar(5) NOT NULL default ' ',
  "AAAAYFYH" varchar(1) NOT NULL default ' ',
  "AAAAYFYI" decimal(5,2) NOT NULL default '0.00',
  "AAAAYFYJ" decimal(3,1) NOT NULL default '0.0',
  "AAAAYFYK" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYFZS","AAAAYFYN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGAR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFYU" varchar(16) NOT NULL default ' ',
  "AAAAYFYV" varchar(4) NOT NULL default ' ',
  "AAAAYFYW" varchar(1) NOT NULL default ' ',
  "AAAAYFYX" varchar(1) NOT NULL default ' ',
  "AAAAYFYY" varchar(1) NOT NULL default ' ',
  "AAAAYFYZ" varchar(30) NOT NULL default ' ',
  "AAAAYFZO" varchar(20) NOT NULL default ' ',
  "AAAAYFZP" varchar(40) NOT NULL default ' ',
  "AAAAYFZC" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGAR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYFZS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYFZL" varchar(40) NOT NULL default ' ',
  "AAAAYFZM" varchar(1) NOT NULL default ' ',
  "AAAAYFZN" varchar(30) NOT NULL default ' ',
  "AAAAYFZO" varchar(20) NOT NULL default ' ',
  "AAAAYFZP" varchar(39) NOT NULL default ' ',
  "AAAAYFZQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYFZS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYFZU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYLJE" varchar(4) NOT NULL default ' ',
  "AAAAYFZY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYLJE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGAQ" varchar(3) NOT NULL default ' ',
  "AAAAYGAR" varchar(2) NOT NULL default ' ',
  "AAAAYGAS" varchar(3) NOT NULL default '000',
  "AAABIHLK" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGAL" varchar(1) NOT NULL default ' ',
  "AAAAYGAM" varchar(3) NOT NULL default ' ',
  "AAABMKIQ" varchar(2) NOT NULL default ' ',
  "AAABBJUZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGAQ","AAAAYGAR","AAAAYGAS","AAABIHLK","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGAW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGFP" varchar(2) NOT NULL default '00',
  "AAAAZBEI" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGGQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGFP","AAAAZBEI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGGQ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGBM" decimal(6,3) NOT NULL default '0.000',
  "AAAAYGBN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGGQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGFP" varchar(2) NOT NULL default '00',
  "AAAAZBEI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGGQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGFP","AAAAZBEI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGCC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGFP" varchar(2) NOT NULL default '00',
  "AAAAZBEI" varchar(2) NOT NULL default ' ',
  "AAAAYGCR" varchar(3) NOT NULL default '000',
  "AAABABDL" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZTYI" varchar(1) NOT NULL default ' ',
  "AAAAYGCL" varchar(2) NOT NULL default ' ',
  "AAAAYGCM" varchar(4) NOT NULL default '0000',
  "AAAAYGCN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGFP","AAAAZBEI","AAAAYGCR","AAABABDL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGFP" varchar(2) NOT NULL default '00',
  "AAAAZBEI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBABL" varchar(1) NOT NULL default ' ',
  "AAAAZGEI" varchar(1) NOT NULL default ' ',
  "AAAAYGDC" varchar(1) NOT NULL default ' ',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAAAZNXD" varchar(2) NOT NULL default ' ',
  "AAAAYGDF" varchar(5) NOT NULL default ' ',
  "AAAAYGDG" varchar(1) NOT NULL default ' ',
  "AAAAYGDH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGFP","AAAAZBEI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGFP" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGDS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYGFP","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIKV" varchar(2) NOT NULL default '00',
  "AAAAYGEF" varchar(2) NOT NULL default '00',
  "AAAAYGFP" varchar(2) NOT NULL default '00',
  "AAAAYGEC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIKV","AAAAYGEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGEO" varchar(3) NOT NULL default ' ',
  "AAAAYGEP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYGEO","AAAAYGEP","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGFP" varchar(2) NOT NULL default '00',
  "AAAAZBEI" varchar(2) NOT NULL default ' ',
  "AAAAYGFC" varchar(2) NOT NULL default '00',
  "AAAAYGEX" varchar(2) NOT NULL default ' ',
  "AAAAYGEY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGFP","AAAAZBEI","AAAAYGFC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGFP" varchar(2) NOT NULL default '00',
  "AAAAZBEI" varchar(2) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGGI" varchar(6) NOT NULL default ' ',
  "AAABLEOT" varchar(4) NOT NULL default ' ',
  "AAABMKAI" varchar(7) NOT NULL default ' ',
  "AAAAYGFN" decimal(3,0) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAYGFP","AAAAZBEI","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGFT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGGI" varchar(6) NOT NULL default ' ',
  "AAABJGOK" varchar(8) NOT NULL default ' ',
  "AAABMNMY" varchar(2) NOT NULL default ' ',
  "AAABMNMZ" varchar(45) NOT NULL default ' ',
  "AAABMNNA" varchar(45) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGGI","AAABJGOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGGI" varchar(6) NOT NULL default ' ',
  "AAAAYGXF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGGI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGGJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYGGQ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYGGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGGR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGGX" varchar(2) NOT NULL default ' ',
  "AAAAYGHL" varchar(2) NOT NULL default ' ',
  "AAABEUWE" varchar(2) NOT NULL default ' ',
  "AAAAYGHA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGHL" varchar(2) NOT NULL default ' ',
  "AAABEUWE" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGHL","AAABEUWE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGJY" varchar(2) NOT NULL default ' ',
  "AAAAYGIL" varchar(4) NOT NULL default '0000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLPQZ" decimal(15,2) NOT NULL default '0.00',
  "AAAAYGHU" decimal(9,2) NOT NULL default '0.00',
  "AAAAYGHV" decimal(9,2) NOT NULL default '0.00',
  "AAAAYGHW" decimal(9,2) NOT NULL default '0.00',
  "AAAAYGHX" decimal(9,2) NOT NULL default '0.00',
  "AAAAYGHY" decimal(9,2) NOT NULL default '0.00',
  "AAAAYGHZ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYGIA" decimal(6,3) NOT NULL default '0.000',
  "AAAAYGIB" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAYGIC" decimal(10,7) NOT NULL default '0.0000000',
  "AAAAYGID" varchar(8) NOT NULL default '00000000',
  "AAABLRNO" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAYGIG" varchar(8) NOT NULL default '00000000',
  "AAAAYGIH" varchar(8) NOT NULL default '00000000',
  "AAAAYGII" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYGJY","AAAAYGIL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGJY" varchar(2) NOT NULL default ' ',
  "AAAAYGIV" varchar(2) NOT NULL default '00',
  "AAAAYIHQ" varchar(4) NOT NULL default ' ',
  "AAAAYGIS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGJY","AAAAYGIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGJF" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGJN" decimal(11,2) NOT NULL default '0.00',
  "AAAAYGJD" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYGJF","AAABKFCZ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGJP" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGJN" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYGJP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGJR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYGJY" varchar(2) NOT NULL default ' ',
  "AAAAYGJV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYGJY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKLZ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGKE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKLZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZDHM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGKN" varchar(1) NOT NULL default ' ',
  "AAAAYGKO" varchar(2) NOT NULL default ' ',
  "AAAAYGKP" varchar(2) NOT NULL default ' ',
  "AAAAYGLZ" varchar(4) NOT NULL default ' ',
  "AAAAYGKR" varchar(2) NOT NULL default ' ',
  "AAAAYGKS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAZDHM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGLL" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGLL" varchar(3) NOT NULL default ' ',
  "AAAAYGLI" varchar(1) NOT NULL default ' ',
  "AAABGGNI" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGLX" varchar(2) NOT NULL default ' ',
  "AAAAYGLY" varchar(2) NOT NULL default ' ',
  "AAAAYGLZ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHNF" varchar(1) NOT NULL default ' ',
  "AAAAYGLU" varchar(1) NOT NULL default ' ',
  "AAAAYGLV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGLX","AAAAYGLY","AAAAYGLZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGPS" varchar(4) NOT NULL default ' ',
  "AAAAYGPH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYGPS" varchar(4) NOT NULL default ' ',
  "AAAAYGPP" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYGPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGMP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGNB" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYGNB" varchar(8) NOT NULL default ' ',
  "AAAAYGMY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYGNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGNC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGNN" varchar(4) NOT NULL default ' ',
  "AAABMDXM" varchar(30) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABLVHV" varchar(31) NOT NULL default ' ',
  "AAABEGXI" varchar(14) NOT NULL default ' ',
  "AAABAZBK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGOZ" varchar(2) NOT NULL default ' ',
  "AAAAYGPA" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYGOZ","AAAAYGPA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGOZ" varchar(2) NOT NULL default ' ',
  "AAAAYGOO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGOZ","AAAAYGOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYGOZ" varchar(2) NOT NULL default ' ',
  "AAAAYGOO" varchar(1) NOT NULL default ' ',
  "AAAAYHHG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYGOZ","AAAAYGOO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYGOZ" varchar(2) NOT NULL default ' ',
  "AAAAYGPA" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYHHG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYGOZ","AAAAYGPA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGPS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGPH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGPS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGPL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYGPS" varchar(4) NOT NULL default ' ',
  "AAAAYGPP" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYGPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGRO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYGRO" varchar(2) NOT NULL default ' ',
  "AAAAYGQC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYGRO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGRO" varchar(2) NOT NULL default ' ',
  "AAAAZOGT" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGRO","AAAAZOGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYGRO" varchar(2) NOT NULL default ' ',
  "AAAAZOGT" varchar(5) NOT NULL default ' ',
  "AAAAYGQS" varchar(60) NOT NULL default ' ',
  "AAAAYGQT" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYGRO","AAAAZOGT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLJE" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAAAYGRO" varchar(2) NOT NULL default ' ',
  "AAAAZOGT" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLJE","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGRH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGRO" varchar(2) NOT NULL default ' ',
  "AAAAZOGT" varchar(5) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGRO","AAAAZOGT","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGRR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGXH" varchar(1) NOT NULL default ' ',
  "AAABLVJB" varchar(1) NOT NULL default '0',
  "AAAAYGUY" varchar(2) NOT NULL default '00',
  "AAAAYGRW" varchar(12) NOT NULL default ' ',
  "AAAAYGRX" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGXH","AAABLVJB","AAAAYGUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGSC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGXH" varchar(1) NOT NULL default ' ',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  "AAAAYGVH" varchar(3) NOT NULL default '000',
  "AAAAYGXF" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGXH","AAAAYGWV","AAAAYGXI","AAAAYGVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  "AAAAYGVH" varchar(3) NOT NULL default '000',
  "AAAAYGSZ" varchar(2) NOT NULL default '00',
  "AAAAYGWC" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAAAYGWV","AAAAYGXI","AAAAYGVH","AAAAYGSZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGTA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGXH" varchar(1) NOT NULL default ' ',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  "AAAAYGWC" varchar(5) NOT NULL default '00000',
  "AAAAYGXF" varchar(20) NOT NULL default ' ',
  "AAABGGNI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGXH","AAAAYGWV","AAAAYGXI","AAAAYGWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGTN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGXH" varchar(1) NOT NULL default ' ',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  "AAAAYGWC" varchar(5) NOT NULL default '00000',
  "AAAAYGWD" varchar(5) NOT NULL default ' ',
  "AAAAYGXF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGXH","AAAAYGWV","AAAAYGXI","AAAAYGWC","AAAAYGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  "AAAAYGWC" varchar(5) NOT NULL default '00000',
  "AAAAYGWD" varchar(5) NOT NULL default ' ',
  "AAAAYGUR" varchar(2) NOT NULL default '00',
  "AAABMNMZ" varchar(8) NOT NULL default ' ',
  "AAABMNNA" varchar(8) NOT NULL default ' ',
  "AAABLGYD" varchar(2) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGWV","AAAAYGXI","AAAAYGWC","AAAAYGWD","AAAAYGUR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVJB" varchar(1) NOT NULL default '0',
  "AAAAYGUY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABLVJB","AAAAYGUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGUZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  "AAAAYGVH" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAYGWV","AAAAYGXI","AAAAYGVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  "AAAAYGWC" varchar(5) NOT NULL default '00000',
  "AAAAYGVN" varchar(1) NOT NULL default ' ',
  "AAABBXWP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGWV","AAAAYGXI","AAAAYGWC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGVT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  "AAAAYGWC" varchar(5) NOT NULL default '00000',
  "AAAAYGWD" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGWV","AAAAYGXI","AAAAYGWC","AAAAYGWD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLVJB" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAYGWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGXH" varchar(1) NOT NULL default ' ',
  "AAAAYGWV" varchar(1) NOT NULL default '0',
  "AAAAYGXF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGXH","AAAAYGWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAYGXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGXB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGXH" varchar(1) NOT NULL default ' ',
  "AAAAYGXI" varchar(1) NOT NULL default '0',
  "AAAAYGXF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGXH","AAAAYGXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGXJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGYK" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYGXZ" varchar(9) NOT NULL default ' ',
  "AAAAYGYA" varchar(5) NOT NULL default ' ',
  "AAAAYGYK" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYGXV" decimal(17,2) NOT NULL default '0.00',
  "AAAAYGXW" decimal(15,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYGXZ","AAAAYGYA","AAAAYGYK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYGYK" varchar(8) NOT NULL default ' ',
  "AAABMRCK" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYGYK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGYL" (
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAAAYGZS" varchar(30) NOT NULL default ' ',
  "AAAAYGZT" varchar(30) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYHEF" varchar(4) NOT NULL default ' ',
  "AAAAYHAA" varchar(30) NOT NULL default ' ',
  "AAAAYHAB" varchar(3) NOT NULL default ' ',
  "AAAAYHDW" varchar(1) NOT NULL default ' ',
  "AAAAYHDX" varchar(2) NOT NULL default '00',
  "AAAAYHDY" varchar(1) NOT NULL default '0',
  "AAAAYHDZ" varchar(2) NOT NULL default '00',
  "AAAAYHEB" varchar(2) NOT NULL default '00',
  "AAAAYHEA" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFGL","AAAAYGZS","AAAAYGZT","AAABJFHB","AAAAYHEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGZE" (
  "AAAAYGZR" varchar(30) NOT NULL default ' ',
  "AAAAYGZS" varchar(30) NOT NULL default ' ',
  "AAAAYGZT" varchar(30) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYHEF" varchar(4) NOT NULL default ' ',
  "AAAAYHAA" varchar(30) NOT NULL default ' ',
  "AAAAYHAB" varchar(3) NOT NULL default ' ',
  "AAAAYHDW" varchar(1) NOT NULL default ' ',
  "AAAAYHDX" varchar(2) NOT NULL default '00',
  "AAAAYHDY" varchar(1) NOT NULL default '0',
  "AAAAYHDZ" varchar(2) NOT NULL default '00',
  "AAAAYHEB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAYGZR","AAAAYGZS","AAAAYGZT","AAABJFHB","AAAAYHEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYGZW" (
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYHAI" varchar(1) NOT NULL default ' ',
  "AAAAYHEF" varchar(4) NOT NULL default ' ',
  "AAAAYHAA" varchar(30) NOT NULL default ' ',
  "AAAAYHAB" varchar(3) NOT NULL default ' ',
  "AAAAYHDW" varchar(1) NOT NULL default ' ',
  "AAAAYHDX" varchar(2) NOT NULL default '00',
  "AAAAYHDY" varchar(1) NOT NULL default '0',
  "AAAAYHDZ" varchar(2) NOT NULL default '00',
  "AAAAYHEB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABHVYL","AAAAYHAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHAJ" (
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYHAX" varchar(3) NOT NULL default ' ',
  "AAAAYHER" varchar(2) NOT NULL default ' ',
  "AAAAYHAN" varchar(4) NOT NULL default ' ',
  "AAAAZMNH" varchar(8) NOT NULL default ' ',
  "AAABBHNP" varchar(25) NOT NULL default ' ',
  "AAAAYHAQ" varchar(1) NOT NULL default ' ',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAAAYHAS" varchar(1) NOT NULL default ' ',
  "AAAAYHAT" varchar(1) NOT NULL default ' ',
  "AAAAYHAU" varchar(1) NOT NULL default ' ',
  "AAABMAZB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABHVYL","AAAAYHAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHAY" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHBJ" varchar(3) NOT NULL default ' ',
  "AAAAYHEF" varchar(4) NOT NULL default ' ',
  "AAAAYHBL" varchar(16) NOT NULL default ' ',
  "AAAAYHKP" varchar(40) NOT NULL default ' ',
  "AAAAYHBE" varchar(4) NOT NULL default ' ',
  "AAAAYHBF" varchar(16) NOT NULL default ' ',
  "AAAAYHBG" varchar(40) NOT NULL default ' ',
  "AAAAYHBH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAAAYHBJ","AAAAYHEF","AAAAYHBL","AAAAYHKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHBN" (
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHBQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKOTX","AAAAYHLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHBT" (
  "AAAAYHDN" varchar(6) NOT NULL default ' ',
  "AAAAYHDO" varchar(2) NOT NULL default ' ',
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHDQ" varchar(5) NOT NULL default ' ',
  "AAAAYHDR" varchar(4) NOT NULL default ' ',
  "AAAAYHDS" varchar(1) NOT NULL default ' ',
  "AAAAYHDC" varchar(5) NOT NULL default ' ',
  "AAAAYHDL" varchar(2) NOT NULL default ' ',
  "AAAAYHDM" varchar(255) NOT NULL default ' ',
  "AAAAYHCD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHDN","AAAAYHDO","AAAAYHLN","AAAAYHDQ","AAAAYHDR","AAAAYHDS","AAAAYHDC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHCL" (
  "AAAAYHDN" varchar(6) NOT NULL default ' ',
  "AAAAYHDO" varchar(2) NOT NULL default ' ',
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHDQ" varchar(5) NOT NULL default ' ',
  "AAAAYHDR" varchar(4) NOT NULL default ' ',
  "AAAAYHDS" varchar(1) NOT NULL default ' ',
  "AAAAYHDC" varchar(5) NOT NULL default ' ',
  "AAAAYHDD" varchar(4) NOT NULL default ' ',
  "AAAAYHDL" varchar(2) NOT NULL default ' ',
  "AAAAYHDM" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHDN","AAAAYHDO","AAAAYHLN","AAAAYHDQ","AAAAYHDR","AAAAYHDS","AAAAYHDC","AAAAYHDD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHDE" (
  "AAAAYHDN" varchar(6) NOT NULL default ' ',
  "AAAAYHDO" varchar(2) NOT NULL default ' ',
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHDQ" varchar(5) NOT NULL default ' ',
  "AAAAYHDR" varchar(4) NOT NULL default ' ',
  "AAAAYHDS" varchar(1) NOT NULL default ' ',
  "AAAAYHDL" varchar(2) NOT NULL default ' ',
  "AAAAYHDM" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHDN","AAAAYHDO","AAAAYHLN","AAAAYHDQ","AAAAYHDR","AAAAYHDS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHDT" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHEF" varchar(4) NOT NULL default ' ',
  "AAAAYHDW" varchar(1) NOT NULL default ' ',
  "AAAAYHDX" varchar(2) NOT NULL default '00',
  "AAAAYHDY" varchar(1) NOT NULL default '0',
  "AAAAYHDZ" varchar(2) NOT NULL default '00',
  "AAAAYHEA" varchar(3) NOT NULL default ' ',
  "AAAAYHEB" varchar(2) NOT NULL default '00',
  "AAAAYHEC" varchar(35) NOT NULL default ' ',
  "AAABHMNR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAAAYHEF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHEG" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHEL" varchar(3) NOT NULL default ' ',
  "AAAAYHLL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAAAYHEL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHEM" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHER" varchar(2) NOT NULL default ' ',
  "AAAAYHLL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAAAYHER")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHES" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYHEX" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAABKOTX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHFB" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHHJ" varchar(2) NOT NULL default ' ',
  "AAAAYHGS" varchar(1) NOT NULL default ' ',
  "AAAAYHFF" varchar(1) NOT NULL default ' ',
  "AAAAYHGP" varchar(4) NOT NULL default '0000',
  "AAAAYHFH" varchar(6) NOT NULL default '000000',
  "AAABKZAZ" varchar(5) NOT NULL default '00000',
  "AAAAYHFJ" varchar(4) NOT NULL default ' ',
  "AAAAYHFK" varchar(4) NOT NULL default ' ',
  "AAAAYHFL" varchar(24) NOT NULL default ' ',
  "AAAAYHFM" varchar(44) NOT NULL default ' ',
  "AAAAYHFN" varchar(4) NOT NULL default ' ',
  "AAAAYHFO" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAAAYHLN","AAAAYHHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHFR" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHHJ" varchar(2) NOT NULL default ' ',
  "AAABADEH" varchar(3) NOT NULL default ' ',
  "AAAAYHGP" varchar(4) NOT NULL default '0000',
  "AAAAYHFW" varchar(5) NOT NULL default ' ',
  "AAAAYHFX" varchar(5) NOT NULL default ' ',
  "AAAAYHGS" varchar(1) NOT NULL default ' ',
  "AAAAYHFZ" varchar(1) NOT NULL default ' ',
  "AAAAYHGA" varchar(6) NOT NULL default ' ',
  "AAAAYHGB" varchar(24) NOT NULL default ' ',
  "AAAAYHGC" varchar(11) NOT NULL default ' ',
  "AAAAYHGD" varchar(2) NOT NULL default ' ',
  "AAAAYHGE" decimal(11,2) NOT NULL default '0.00',
  "AAAAYHGF" decimal(11,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYHLN","AAAAYHHJ","AAABADEH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHGJ" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHHJ" varchar(2) NOT NULL default ' ',
  "AAAAYHHK" varchar(3) NOT NULL default ' ',
  "AAAAYHHL" varchar(8) NOT NULL default ' ',
  "AAAAYHHM" varchar(1) NOT NULL default ' ',
  "AAAAYHGP" varchar(4) NOT NULL default '0000',
  "AAAAYHGQ" varchar(5) NOT NULL default ' ',
  "AAAAYHGR" varchar(5) NOT NULL default ' ',
  "AAAAYHGS" varchar(1) NOT NULL default ' ',
  "AAAAYHGT" varchar(5) NOT NULL default ' ',
  "AAAAYHGU" varchar(1) NOT NULL default ' ',
  "AAAAYHGV" varchar(1) NOT NULL default ' ',
  "AAAAYHGW" varchar(6) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default ' ',
  "AAAAYHLS" varchar(2) NOT NULL default ' ',
  "AAABIFJQ" varchar(2) NOT NULL default ' ',
  "AAAAYHHA" varchar(24) NOT NULL default ' ',
  "AAAAYHHB" varchar(20) NOT NULL default ' ',
  "AAAAZDMZ" varchar(4) NOT NULL default ' ',
  "AAAAYHHD" varchar(5) NOT NULL default ' ',
  "AAAAYHHE" varchar(11) NOT NULL default ' ',
  "AAAAYHHF" decimal(16,2) NOT NULL default '0.00',
  "AAAAYHHG" varchar(31) NOT NULL default ' ',
  "AAAAYHHH" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAAAYHHJ","AAAAYHHK","AAAAYHHL","AAAAYHHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHHN" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHJY" varchar(6) NOT NULL default ' ',
  "AAAAYHJZ" varchar(4) NOT NULL default '0000',
  "AAAAYHHR" varchar(1) NOT NULL default ' ',
  "AAAAYHHS" varchar(8) NOT NULL default ' ',
  "AAAAYHHT" varchar(6) NOT NULL default ' ',
  "AAAAYHJF" varchar(80) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAYHJT" varchar(80) NOT NULL default ' ',
  "AAAAYHHX" varchar(8) NOT NULL default ' ',
  "AAAAYHHY" varchar(4) NOT NULL default '0000',
  "AAAAYHJW" varchar(40) NOT NULL default ' ',
  "AAAAYHIA" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAAAYHLN","AAAAYHJY","AAAAYHJZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHIE" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHJY" varchar(6) NOT NULL default ' ',
  "AAAAYHJZ" varchar(4) NOT NULL default '0000',
  "AAAAYHKA" varchar(2) NOT NULL default ' ',
  "AAAAYHJE" varchar(1) NOT NULL default ' ',
  "AAAAYHIK" varchar(3) NOT NULL default '000',
  "AAAAYHIL" varchar(30) NOT NULL default ' ',
  "AAAAYHJK" varchar(7) NOT NULL default '0000000',
  "AAAAYHIN" varchar(44) NOT NULL default ' ',
  "AAAAYHIO" varchar(2) NOT NULL default ' ',
  "AAAAYHIP" varchar(5) NOT NULL default '00000',
  "AAAAYHIQ" varchar(20) NOT NULL default ' ',
  "AAAAYHIR" varchar(150) NOT NULL default ' ',
  "AAAAYHIS" varchar(1) NOT NULL default ' ',
  "AAABHGWF" varchar(1) NOT NULL default ' ',
  "AAAAYHJW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAAAYHJY","AAAAYHJZ","AAAAYHKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHIZ" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHJY" varchar(6) NOT NULL default ' ',
  "AAAAYHJZ" varchar(4) NOT NULL default '0000',
  "AAAAYHKA" varchar(4) NOT NULL default '0000',
  "AAAAYHJE" varchar(1) NOT NULL default ' ',
  "AAAAYHJF" varchar(80) NOT NULL default ' ',
  "AAAAYHJG" varchar(1) NOT NULL default ' ',
  "AAAAYHJH" varchar(8) NOT NULL default '00000000',
  "AAABMNYU" varchar(4) NOT NULL default ' ',
  "AAAAYHJJ" varchar(44) NOT NULL default ' ',
  "AAAAYHJK" varchar(7) NOT NULL default '0000000',
  "AAAAYHJL" varchar(5) NOT NULL default '00000',
  "AAAAYHJM" varchar(1) NOT NULL default ' ',
  "AAABICGK" varchar(9) NOT NULL default '000000000',
  "AAABIAKJ" varchar(30) NOT NULL default ' ',
  "AAAAYHJP" varchar(8) NOT NULL default ' ',
  "AAAAYHJQ" varchar(6) NOT NULL default ' ',
  "AAAAYHJR" varchar(8) NOT NULL default ' ',
  "AAAAYHJS" varchar(6) NOT NULL default ' ',
  "AAAAYHJT" varchar(128) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAAAYHJV" varchar(1) NOT NULL default ' ',
  "AAAAYHJW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAAAYHJY","AAAAYHJZ","AAAAYHKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHKB" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHKP" varchar(8) NOT NULL default ' ',
  "AAAAYHKQ" varchar(6) NOT NULL default ' ',
  "AAAAYHKU" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAAAYHKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHKI" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHKP" varchar(8) NOT NULL default ' ',
  "AAAAYHKQ" varchar(6) NOT NULL default ' ',
  "AAAAYHKR" varchar(3) NOT NULL default ' ',
  "AAAAYHKU" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAAAYHKP","AAAAYHKQ","AAAAYHKR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHKS" (
  "AAAAYHKV" varchar(30) NOT NULL default ' ',
  "AAAAYHKU" varchar(200) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHKW" (
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAABMCRV" varchar(15) NOT NULL default ' ',
  "AAAAYHLF" varchar(3) NOT NULL default ' ',
  "AAAAYHLG" varchar(8) NOT NULL default '00000000',
  "AAAAYHLH" varchar(3) NOT NULL default '000',
  "AAAAYHLC" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYHLN","AAABMCRV","AAAAYHLF","AAAAYHLG","AAAAYHLH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHLI" (
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYHLN" varchar(6) NOT NULL default ' ',
  "AAAAYHLL" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRHS","AAAAYHLN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHLO" (
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAAAYHLS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABKOTX","AAAAYHLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBXCS" varchar(10) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(30) NOT NULL default ' ',
  "AAAAZNXD" varchar(2) NOT NULL default ' ',
  "AAABLQGL" varchar(30) NOT NULL default ' ',
  "AAABLEQN" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBXCS","AAABMOHM","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHMM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMOHM" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(42) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMOHM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHMU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  "AAAAYHTE" varchar(1) NOT NULL default ' ',
  "AAAAYHTH" varchar(4) NOT NULL default ' ',
  "AAABARRX" varchar(1) NOT NULL default ' ',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAAAYHTO" varchar(60) NOT NULL default ' ',
  "AAAAYHNG" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHTH" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABMKXW" varchar(5) NOT NULL default ' ',
  "AAABLEOT" varchar(8) NOT NULL default ' ',
  "AAAAYHNX" varchar(3) NOT NULL default ' ',
  "AAABMMJT" decimal(18,5) NOT NULL default '0.00000',
  "AAAAYHNZ" varchar(5) NOT NULL default ' ',
  "AAAAYHOA" varchar(5) NOT NULL default ' ',
  "AAAAYHOB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAYHTH","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHOI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHTH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAYHTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHOR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHTH" varchar(4) NOT NULL default ' ',
  "AAABMLMP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHVYP","AAABLECD","AAAAYHTH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABABKA" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABEPJZ" varchar(1) NOT NULL default ' ',
  "AAAAYHTE" varchar(1) NOT NULL default ' ',
  "AAAAYHTF" varchar(1) NOT NULL default ' ',
  "AAAAYHTG" varchar(1) NOT NULL default ' ',
  "AAAAYHPP" varchar(1) NOT NULL default ' ',
  "AAAAYHTH" varchar(4) NOT NULL default ' ',
  "AAAAYHPR" varchar(1) NOT NULL default ' ',
  "AAAAYHPS" varchar(1) NOT NULL default ' ',
  "AAAAYHTO" varchar(60) NOT NULL default ' ',
  "AAABEGDU" varchar(1) NOT NULL default ' ',
  "AAAAYHPV" varchar(3) NOT NULL default '000',
  "AAAAYHPW" varchar(1) NOT NULL default ' ',
  "AAAAYHPX" varchar(5) NOT NULL default ' ',
  "AAAAYHTR" varchar(1) NOT NULL default ' ',
  "AAABMCWR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAABABKA","AAABMKXW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAAAYHQO" varchar(2) NOT NULL default ' ',
  "AAAAYHWQ" varchar(4) NOT NULL default ' ',
  "AAAAYHQQ" varchar(30) NOT NULL default ' ',
  "AAAAYHQR" varchar(30) NOT NULL default ' ',
  "AAAAYHSV" varchar(4) NOT NULL default ' ',
  "AAABBHTC" varchar(2) NOT NULL default ' ',
  "AAABFXYE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAABMKXW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHVYP","AAABLECD","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHRM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHSV" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABFLSG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAYHSV","AAABMKXW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHSV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAYHSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHSV" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHVYP","AAABLECD","AAAAYHSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABABKA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLGEL" varchar(1) NOT NULL default ' ',
  "AAAAYHTE" varchar(1) NOT NULL default ' ',
  "AAAAYHTF" varchar(1) NOT NULL default ' ',
  "AAAAYHTG" varchar(1) NOT NULL default ' ',
  "AAAAYHTH" varchar(4) NOT NULL default ' ',
  "AAAAYHTI" varchar(30) NOT NULL default ' ',
  "AAAAYHTJ" varchar(4) NOT NULL default ' ',
  "AAAAYHTK" varchar(3) NOT NULL default '000',
  "AAAAYHTL" varchar(20) NOT NULL default ' ',
  "AAAAYHTM" varchar(2) NOT NULL default ' ',
  "AAAAYHTN" varchar(4) NOT NULL default ' ',
  "AAAAYHTO" varchar(60) NOT NULL default ' ',
  "AAAAYHTP" varchar(4) NOT NULL default ' ',
  "AAABBFZW" varchar(1) NOT NULL default ' ',
  "AAAAYHTR" varchar(1) NOT NULL default ' ',
  "AAAAYHTS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAABABKA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAABABKA" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHVYP","AAABLECD","AAABABKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHWQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAYHWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHUT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHWQ" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHVYP","AAABLECD","AAAAYHWQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHVF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHWQ" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQII" varchar(2) NOT NULL default ' ',
  "AAAAYHWI" varchar(2) NOT NULL default '00',
  "AAAAYHWJ" varchar(4) NOT NULL default ' ',
  "AAABFLSG" varchar(1) NOT NULL default ' ',
  "AAAAZLRQ" varchar(1) NOT NULL default ' ',
  "AAABEJKE" decimal(6,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAYHWQ","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYHWQ" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQII" varchar(2) NOT NULL default ' ',
  "AAAAYHWI" varchar(2) NOT NULL default '00',
  "AAAAYHWJ" varchar(4) NOT NULL default ' ',
  "AAABFLSG" varchar(1) NOT NULL default ' ',
  "AAAAZLRQ" varchar(1) NOT NULL default ' ',
  "AAABEJKE" decimal(6,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAYHWQ","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYHYO" varchar(4) NOT NULL default ' ',
  "AAAAYYWH" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPUL" varchar(8) NOT NULL default ' ',
  "AAAAYHXB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYHYO","AAAAYYWH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHXH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYHYO" varchar(4) NOT NULL default ' ',
  "AAAAYYWH" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAYHYO","AAAAYYWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYHYO" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYHXZ" varchar(1) NOT NULL default ' ',
  "AAAAYHYA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYHYO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHYF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYHYO" varchar(4) NOT NULL default ' ',
  "AAABMLPV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAYHYO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBXCS" varchar(10) NOT NULL default ' ',
  "AAABMRCK" varchar(42) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBXCS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHYX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYICV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYICV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHZE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYICV" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHVYP","AAAAYICV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYHZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYICV" varchar(4) NOT NULL default ' ',
  "AAAAYIBH" varchar(20) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAYIAL" varchar(30) NOT NULL default ' ',
  "AAAAYIBI" varchar(32) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYHZZ" varchar(1) NOT NULL default ' ',
  "AAABJSBE" varchar(2) NOT NULL default '00',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAYIBC" varchar(8) NOT NULL default '00000000',
  "AAABKNQC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYICV","AAAAYIBH","AAABMCRV","AAAAYIAL","AAAAYIBI","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYICV" varchar(4) NOT NULL default ' ',
  "AAAAYIBH" varchar(20) NOT NULL default ' ',
  "AAAAYIBI" varchar(32) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJSBE" varchar(2) NOT NULL default '00',
  "AAABKOUD" varchar(8) NOT NULL default '00000000',
  "AAABJTAJ" varchar(6) NOT NULL default '000000',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAYIBC" varchar(8) NOT NULL default '00000000',
  "AAABKNQC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYICV","AAAAYIBH","AAAAYIBI","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYICV" varchar(4) NOT NULL default ' ',
  "AAABJSBE" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAAAYIBR" varchar(4) NOT NULL default ' ',
  "AAABMQBR" varchar(4) NOT NULL default ' ',
  "AAABMRFO" varchar(2) NOT NULL default '00',
  "AAABDOOL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYICV","AAABJSBE","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYICA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYICV" varchar(4) NOT NULL default ' ',
  "AAABJSBE" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAABHVYP","AAAAYICV","AAABJSBE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYICM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYICV" varchar(4) NOT NULL default ' ',
  "AAABJFHC" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABHWLM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYICV","AAABJFHC","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYICY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABEEVF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABEEVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABEEVF" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABEEVF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBHCM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIEG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABEEVF" varchar(4) NOT NULL default ' ',
  "AAAAYIFM" varchar(4) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABEJKE" decimal(6,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABEEVF","AAAAYIFM","AAABBHCM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIEW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYIFM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYIFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIFD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYIFM" varchar(4) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAYIFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABEEVF" varchar(4) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABEEVF","AAABBHCM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIFZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBHCM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBHCM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIGL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBDTS" varchar(4) NOT NULL default ' ',
  "AAAAYIGU" varchar(8) NOT NULL default ' ',
  "AAAAYIGQ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBDTS","AAAAYIGU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIGV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAAAYIIT" varchar(2) NOT NULL default '00',
  "AAAAYIHA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZMRI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZMRI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIHQ" varchar(4) NOT NULL default ' ',
  "AAAAYVHG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIKJ" varchar(2) NOT NULL default ' ',
  "AAAAYIKK" varchar(4) NOT NULL default ' ',
  "AAAAYIHV" varchar(1) NOT NULL default ' ',
  "AAAAYIHW" varchar(1) NOT NULL default ' ',
  "AAAAYIIU" varchar(2) NOT NULL default '00',
  "AAAAZJRR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIKJ","AAAAYIKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIKJ" varchar(2) NOT NULL default ' ',
  "AAAAYIKK" varchar(4) NOT NULL default ' ',
  "AAABBHNP" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIKJ","AAAAYIKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIIT" varchar(2) NOT NULL default '00',
  "AAAAYIIU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYIIT","AAAAYIIU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYINL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIKJ" varchar(2) NOT NULL default ' ',
  "AAAAYIJH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIJK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIKJ" varchar(2) NOT NULL default ' ',
  "AAAAYIJO" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIKJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIKJ" varchar(2) NOT NULL default ' ',
  "AAAAYIKK" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGIHX" varchar(1) NOT NULL default ' ',
  "AAAAYIJZ" varchar(2) NOT NULL default '00',
  "AAAAYIKA" decimal(3,0) NOT NULL default '0',
  "AAAAYIKB" varchar(3) NOT NULL default ' ',
  "AAAAYIKC" decimal(3,0) NOT NULL default '0',
  "AAAAYIKD" varchar(3) NOT NULL default ' ',
  "AAAAYIKE" decimal(3,0) NOT NULL default '0',
  "AAAAYIKF" varchar(3) NOT NULL default ' ',
  "AAAAYIKG" decimal(3,0) NOT NULL default '0',
  "AAAAYIKH" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIKJ","AAAAYIKK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAAAYIKV" varchar(2) NOT NULL default '00',
  "AAAAYIKR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABGCHE","AAAAYIKV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIMB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYILB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIMB" varchar(2) NOT NULL default ' ',
  "AAABBFCQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYILJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHO" varchar(1) NOT NULL default ' ',
  "AAAAYIMB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZAYN" decimal(5,2) NOT NULL default '0.00',
  "AAAAZAYT" decimal(5,2) NOT NULL default '0.00',
  "AAABAPHM" decimal(3,0) NOT NULL default '0',
  "AAAAYILS" decimal(3,0) NOT NULL default '0',
  "AAAAZGVJ" decimal(3,0) NOT NULL default '0',
  "AAAAYILU" varchar(3) NOT NULL default ' ',
  "AAAAZGVK" decimal(3,0) NOT NULL default '0',
  "AAAAYILW" varchar(3) NOT NULL default ' ',
  "AAAAYILX" decimal(3,0) NOT NULL default '0',
  "AAAAYILY" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHO","AAAAYIMB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGP" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYIMI" varchar(2) NOT NULL default '00',
  "AAAAYIMJ" varchar(2) NOT NULL default '00',
  "AAAAYIMK" varchar(1) NOT NULL default ' ',
  "AAAAYIML" varchar(1) NOT NULL default ' ',
  "AAAAYIMM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIMQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJFGP" varchar(2) NOT NULL default ' ',
  "AAABAEQI" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABJFGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIMY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYINL" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYIND" varchar(1) NOT NULL default ' ',
  "AAAAYINE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYINL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYINI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAFSF" varchar(8) NOT NULL default ' ',
  "AAAAYINL" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAFSF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYINO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAAAYINU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABGLBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYISF" varchar(4) NOT NULL default '0000',
  "AAAAYISE" decimal(4,0) NOT NULL default '0',
  "AAAAZDYF" varchar(2) NOT NULL default ' ',
  "AAAAYIOM" varchar(2) NOT NULL default '00',
  "AAAAYION" varchar(2) NOT NULL default '00',
  "AAAAYIOO" varchar(2) NOT NULL default '00',
  "AAAAYIOP" varchar(2) NOT NULL default '00',
  "AAAAYIOQ" varchar(2) NOT NULL default '00',
  "AAAAYIOR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXG" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIXG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIPK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIXG" varchar(4) NOT NULL default ' ',
  "AAAAZNLM" varchar(15) NOT NULL default ' ',
  "AAAAYZBI" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIXG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJGBU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIPY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJGBU" varchar(1) NOT NULL default ' ',
  "AAABJGBR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABJGBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYIXG" varchar(4) NOT NULL default ' ',
  "AAABIKBY" varchar(4) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYIQQ" varchar(1) NOT NULL default ' ',
  "AAAAZDYE" varchar(8) NOT NULL default ' ',
  "AAAAZDYF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAAAYIXG","AAABIKBY","AAABGLBU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIRB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXG" varchar(4) NOT NULL default ' ',
  "AAABIKBY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJGBU" varchar(1) NOT NULL default ' ',
  "AAAAYIRI" varchar(1) NOT NULL default ' ',
  "AAAAYIRJ" decimal(3,0) NOT NULL default '0',
  "AAAAYIRK" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIXG","AAABIKBY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIRP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIXG" varchar(4) NOT NULL default ' ',
  "AAABIKBY" varchar(4) NOT NULL default ' ',
  "AAAAYIRU" varchar(15) NOT NULL default ' ',
  "AAAAYIRV" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIXG","AAABIKBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYISA" (
  "AAAAYISF" varchar(4) NOT NULL default '0000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYISE" decimal(4,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAYISF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYISH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYISV" varchar(4) NOT NULL default ' ',
  "AAAAYISK" varchar(1) NOT NULL default ' ',
  "AAAAYISL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYISV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYISO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYISV" varchar(4) NOT NULL default ' ',
  "AAABILTS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYISV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYISW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXT" varchar(4) NOT NULL default ' ',
  "AAAAYISZ" varchar(2) NOT NULL default ' ',
  "AAAAYIYW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYITD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIXT" varchar(4) NOT NULL default ' ',
  "AAABAQFE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIXT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYITL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXG" varchar(4) NOT NULL default ' ',
  "AAAAYIUH" varchar(5) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYIXG","AAAAYIUH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYITU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXX" varchar(4) NOT NULL default ' ',
  "AAAAYIXY" varchar(4) NOT NULL default ' ',
  "AAAAYIUH" varchar(5) NOT NULL default ' ',
  "AAAAYIXI" varchar(2) NOT NULL default ' ',
  "AAAAYIXJ" varchar(2) NOT NULL default ' ',
  "AAAAYIUB" varchar(1) NOT NULL default '0',
  "AAABKQOZ" varchar(8) NOT NULL default '00000000',
  "AAAAYIUD" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYIXX","AAAAYIXY","AAAAYIUH","AAAAYIXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIXI" varchar(2) NOT NULL default ' ',
  "AAABBFCQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIXI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIWB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIVB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIWB" varchar(6) NOT NULL default ' ',
  "AAABBARY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIWB" varchar(6) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABMMVO" varchar(7) NOT NULL default ' ',
  "AAAAZJOL" varchar(11) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIWB","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIVS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXV" varchar(4) NOT NULL default ' ',
  "AAAAYIXW" varchar(4) NOT NULL default ' ',
  "AAAAYIXX" varchar(4) NOT NULL default ' ',
  "AAAAYIXY" varchar(4) NOT NULL default ' ',
  "AAAAYIXI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYIWB" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIXV","AAAAYIXW","AAAAYIXX","AAAAYIXY","AAAAYIXI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIWJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYIXJ" varchar(2) NOT NULL default ' ',
  "AAAAYIWS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYIXJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXG" varchar(4) NOT NULL default ' ',
  "AAABIKBY" varchar(4) NOT NULL default ' ',
  "AAAAYIXI" varchar(2) NOT NULL default ' ',
  "AAAAYIXJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYIXE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIXG","AAABIKBY","AAAAYIXI","AAAAYIXJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIXL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIXV" varchar(4) NOT NULL default ' ',
  "AAAAYIXW" varchar(4) NOT NULL default ' ',
  "AAAAYIXX" varchar(4) NOT NULL default ' ',
  "AAAAYIXY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYIXT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIXV","AAAAYIXW","AAAAYIXX","AAAAYIXY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIYA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYIYW" varchar(4) NOT NULL default ' ',
  "AAAAYIYX" varchar(8) NOT NULL default ' ',
  "AAAAYIYY" varchar(2) NOT NULL default ' ',
  "AAAAYIYZ" varchar(2) NOT NULL default '00',
  "AAAAYIZA" varchar(2) NOT NULL default '00',
  "AAAAYIZB" varchar(2) NOT NULL default '00',
  "AAAAYIYL" varchar(8) NOT NULL default ' ',
  "AAAAYIYM" varchar(2) NOT NULL default ' ',
  "AAAAYIYN" varchar(2) NOT NULL default '00',
  "AAAAYIYO" varchar(2) NOT NULL default '00',
  "AAAAYIYP" varchar(2) NOT NULL default '00',
  "AAABAPHM" decimal(7,3) NOT NULL default '0.000',
  "AAAAYPKH" decimal(7,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAAAYIYW","AAAAYIYX","AAAAYIYY","AAAAYIYZ","AAAAYIZA","AAAAYIZB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYIZJ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYIZH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYIZJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIZL" (
  "AAAAYIZQ" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAHD" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAAAYIZQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYIZS" (
  "AAAAYORE" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLEV" decimal(9,2) NOT NULL default '0.00',
  "AAAAYLEW" decimal(9,2) NOT NULL default '0.00',
  "AAAAYLEX" decimal(9,2) NOT NULL default '0.00',
  "AAAAYLEY" decimal(9,2) NOT NULL default '0.00',
  "AAAAYLEZ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYORE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJAD" (
  "AAAAYJAJ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJAH" decimal(5,2) NOT NULL default '0.00',
  "AAAAYJAI" decimal(4,0) NOT NULL default '0',
  PRIMARY KEY ("AAAAYJAJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJAL" (
  "AAAAYJBF" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJAP" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJAQ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJAR" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJAS" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYJBF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJAV" (
  "AAAAYLIB" varchar(2) NOT NULL default ' ',
  "AAAAYJBF" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJBA" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJBB" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJBC" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJBD" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAAAYLIB","AAAAYJBF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYLIB" varchar(2) NOT NULL default ' ',
  "AAAAYJLJ" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYLIB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJBP" (
  "AAAAYLIB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJBT" varchar(1) NOT NULL default ' ',
  "AAAAYJLK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYLIB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAAAZNAJ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJCF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJDK" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMAYY" varchar(1) NOT NULL default ' ',
  "AAABHAOL" varchar(40) NOT NULL default ' ',
  "AAAAYJHJ" decimal(7,2) NOT NULL default '0.00',
  "AAAAYJCN" varchar(6) NOT NULL default ' ',
  "AAAAYJCO" varchar(8) NOT NULL default '00000000',
  "AAAAYJCP" decimal(9,2) NOT NULL default '0.00',
  "AAABAYKN" decimal(9,2) NOT NULL default '0.00',
  "AAABAZBW" decimal(7,4) NOT NULL default '0.0000',
  "AAABAZBX" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJCT" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJCU" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJCV" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJCW" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJCX" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJCY" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJCZ" varchar(6) NOT NULL default ' ',
  "AAAAYJDA" varchar(6) NOT NULL default ' ',
  "AAAAYJDB" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJDC" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJDD" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJDE" decimal(9,2) NOT NULL default '0.00',
  "AAABJCXQ" varchar(2) NOT NULL default '00',
  "AAABABZG" varchar(2) NOT NULL default '00',
  "AAAAYJGX" varchar(15) NOT NULL default ' ',
  "AAAAYJDI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYJDK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJDM" (
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYJDY" varchar(3) NOT NULL default ' ',
  "AAABFLSG" varchar(1) NOT NULL default ' ',
  "AAAAYJEA" varchar(9) NOT NULL default '000000000',
  "AAAAYJEB" varchar(9) NOT NULL default '000000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAZFN" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJDV" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABJFHB","AAABKFCZ","AAAAYJDY","AAABFLSG","AAAAYJEA","AAAAYJEB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJEC" (
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJEG" varchar(2) NOT NULL default '00',
  "AAAAYJEH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJGR" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYJGR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJGR" varchar(4) NOT NULL default ' ',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJEY" varchar(4) NOT NULL default ' ',
  "AAAAYJEZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYJGR","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJFE" (
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYJGB" varchar(9) NOT NULL default '000000000',
  "AAAAYJGC" varchar(9) NOT NULL default '000000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJFJ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJFY" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJFL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABKFCZ","AAAAYJGB","AAAAYJGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJFP" (
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYJGA" varchar(1) NOT NULL default ' ',
  "AAAAYJGB" varchar(9) NOT NULL default '000000000',
  "AAAAYJGC" varchar(9) NOT NULL default '000000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLMLH" varchar(1) NOT NULL default ' ',
  "AAAAYJFW" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJFX" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJFY" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABKFCZ","AAAAYJGA","AAAAYJGB","AAAAYJGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJGD" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYJGJ" varchar(2) NOT NULL default ' ',
  "AAAAYJGG" varchar(12) NOT NULL default ' ',
  "AAAAYJGH" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAYJGJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYJGR" varchar(4) NOT NULL default ' ',
  "AAAAYJGO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYJGR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJGS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJGX" varchar(9) NOT NULL default ' ',
  "AAAAYJGY" varchar(8) NOT NULL default '00000000',
  "AAABMAYY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSYA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJHD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHYMV" varchar(1) NOT NULL default ' ',
  "AAAAYJHJ" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJHK" varchar(8) NOT NULL default '00000000',
  "AAAAYJHL" varchar(4) NOT NULL default ' ',
  "AAAAYJJL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABDSYA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJHV" varchar(1) NOT NULL default ' ',
  "AAAAYJHW" varchar(1) NOT NULL default ' ',
  "AAAAYJHX" varchar(4) NOT NULL default ' ',
  "AAAAYJHY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSYA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJIC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJJL" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABIKCG" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYJJL","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJJL" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABAYQU" decimal(9,2) NOT NULL default '0.00',
  "AAABAYQS" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYJJL","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJJL" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAAAYJJI" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJJJ" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAYJJL","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJJL" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABAYQU" decimal(9,2) NOT NULL default '0.00',
  "AAABAYQS" decimal(9,2) NOT NULL default '0.00',
  "AAAAYJJI" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJJJ" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAYJJL","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJJS" varchar(4) NOT NULL default ' ',
  "AAAAYJJT" varchar(4) NOT NULL default ' ',
  "AAAAYJJU" varchar(4) NOT NULL default ' ',
  "AAAAYJJV" varchar(4) NOT NULL default ' ',
  "AAAAYJJW" varchar(4) NOT NULL default ' ',
  "AAAAYJJX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSYA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABIMUR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABDSYA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJKJ" (
  "AAAAYLHF" varchar(2) NOT NULL default '00',
  "AAAAYJKR" varchar(2) NOT NULL default ' ',
  "AAABAZFN" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJLC" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAAAYLHF","AAAAYJKR","AAABAZFN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJKU" (
  "AAAAYLIB" varchar(2) NOT NULL default ' ',
  "AAAAYJLC" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAHD" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYJLA" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAAAYLIB","AAAAYJLC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJLE" (
  "AAAAYLIB" varchar(2) NOT NULL default ' ',
  "AAAAYLIC" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJLJ" varchar(60) NOT NULL default ' ',
  "AAAAYJLK" varchar(1) NOT NULL default ' ',
  "AAAAYJLL" varchar(2) NOT NULL default ' ',
  "AAAAYJLM" varchar(1) NOT NULL default ' ',
  "AAAAYJLN" varchar(1) NOT NULL default ' ',
  "AAAAYJLO" varchar(1) NOT NULL default ' ',
  "AAAAYJLP" varchar(1) NOT NULL default ' ',
  "AAAAYJLQ" varchar(1) NOT NULL default ' ',
  "AAAAYJLR" varchar(1) NOT NULL default ' ',
  "AAAAYJLS" varchar(1) NOT NULL default ' ',
  "AAAAYJLT" varchar(1) NOT NULL default ' ',
  "AAAAYJLU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYLIB","AAAAYLIC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJLY" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABEPIE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABJFHB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJPC" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYJPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAAAYJPC" varchar(3) NOT NULL default ' ',
  "AAAAYJOS" varchar(7) NOT NULL default ' ',
  "AAAAYORE" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJPC" varchar(3) NOT NULL default ' ',
  "AAAAYJOS" varchar(7) NOT NULL default ' ',
  "AAAAYJNP" varchar(12) NOT NULL default ' ',
  "AAAAYJMW" varchar(6) NOT NULL default ' ',
  "AAAAYJMX" varchar(9) NOT NULL default ' ',
  "AAAAYJMY" varchar(16) NOT NULL default ' ',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAAAYJNA" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYJPC","AAAAYJOS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJNP" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYJNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJNP" varchar(12) NOT NULL default ' ',
  "AAABEHHL" varchar(4) NOT NULL default '0000',
  "AAAAYJNN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYJNP","AAABEHHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYORE" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOTX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJOA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAAAYJOI" varchar(4) NOT NULL default ' ',
  "AAAAYJOE" varchar(10) NOT NULL default ' ',
  "AAAAYJOF" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOTX","AAAAYJOI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYJPC" varchar(3) NOT NULL default ' ',
  "AAAAYJOS" varchar(7) NOT NULL default ' ',
  "AAABFQOM" varchar(1) NOT NULL default ' ',
  "AAABEHHJ" varchar(4) NOT NULL default '0000',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYJPC","AAAAYJOS","AAABFQOM","AAABEHHJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYJPC" varchar(3) NOT NULL default ' ',
  "AAAAYJOZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYJPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJPD" (
  "AAABBAVI" varchar(1) NOT NULL default ' ',
  "AAAAYJPM" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKFL" decimal(7,0) NOT NULL default '0',
  "AAAAYKFF" decimal(7,0) NOT NULL default '0',
  "AAAAYJPK" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABBAVI","AAAAYJPM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJPO" (
  "AAAAYLJM" varchar(1) NOT NULL default ' ',
  "AAAAYKFF" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKFL" varchar(8) NOT NULL default '00000000',
  "AAAAYJQD" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYLJM","AAAAYKFF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJPY" (
  "AAAAYKFF" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKFL" varchar(9) NOT NULL default '000000000',
  "AAAAYJQD" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYKFF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJQG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJQL" (
  "AAAAYLJO" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJQW" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAAAYLJO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJQS" (
  "AAAAYJQX" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJQW" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAAAYJQX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABJFGP" varchar(3) NOT NULL default ' ',
  "AAABLPWW" varchar(10) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABJFGP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHUCM" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJRN" decimal(6,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABHUCM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJRR" (
  "AAAAYJRW" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJRV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYJRW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJRY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYJSC" varchar(18) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAHO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJSG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHLTE" varchar(1) NOT NULL default ' ',
  "AAABGYLC" varchar(4) NOT NULL default ' ',
  "AAABGCHL" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHLTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJSP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAKU" varchar(2) NOT NULL default ' ',
  "AAABINFJ" varchar(6) NOT NULL default ' ',
  "AAABBRWV" varchar(1) NOT NULL default ' ',
  "AAAAYLJI" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLJO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABBAKU","AAABINFJ","AAABBRWV","AAAAYLJI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCADZ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABLOZE" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCADZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZZYW" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJTU" varchar(1) NOT NULL default ' ',
  "AAAAYJTV" varchar(10) NOT NULL default ' ',
  "AAAAYJTW" varchar(2) NOT NULL default ' ',
  "AAAAYJTX" varchar(1) NOT NULL default ' ',
  "AAAAYJTY" varchar(2) NOT NULL default '00',
  "AAAAYJTZ" varchar(2) NOT NULL default '00',
  "AAAAYJUA" varchar(2) NOT NULL default ' ',
  "AAAAYJUB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAZZYW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZZYW" varchar(2) NOT NULL default '00',
  "AAABGCUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAZZYW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABDSZT" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJUW" varchar(6) NOT NULL default '000000',
  "AAAAYJUX" varchar(3) NOT NULL default ' ',
  "AAAAYJUY" varchar(2) NOT NULL default ' ',
  "AAAAYJUZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABDSZT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABDSZT" varchar(2) NOT NULL default '00',
  "AAABGCUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABDSZT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJVO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAQJB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJVU" varchar(1) NOT NULL default ' ',
  "AAAAYJVV" varchar(2) NOT NULL default ' ',
  "AAAAYJVW" varchar(2) NOT NULL default '00',
  "AAAAYJVX" varchar(8) NOT NULL default ' ',
  "AAAAYJVY" varchar(2) NOT NULL default '00',
  "AAAAYJVZ" varchar(1) NOT NULL default ' ',
  "AAAAYJWA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAQJB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJWF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAQJB" varchar(2) NOT NULL default '00',
  "AAABGCUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAQJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJWP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWX" varchar(10) NOT NULL default ' ',
  "AAABAWWY" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYJYC" varchar(3) NOT NULL default ' ',
  "AAAAYJWW" varchar(6) NOT NULL default '000000',
  "AAAAYJWX" varchar(3) NOT NULL default ' ',
  "AAAAYJWY" varchar(2) NOT NULL default '00',
  "AAAAYJWZ" varchar(2) NOT NULL default ' ',
  "AAAAYJXA" varchar(2) NOT NULL default '00',
  "AAAAYJXB" varchar(2) NOT NULL default '00',
  "AAAAYJXC" varchar(2) NOT NULL default '00',
  "AAAAYJXD" varchar(2) NOT NULL default '00',
  "AAAAYJXE" varchar(2) NOT NULL default ' ',
  "AAAAYJXF" varchar(2) NOT NULL default '00',
  "AAAAYJXG" varchar(1) NOT NULL default ' ',
  "AAAAYJXH" varchar(2) NOT NULL default '00',
  "AAAAYJXI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWX","AAABAWWY","AAABKFCZ","AAAAYJYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWX" varchar(10) NOT NULL default ' ',
  "AAABAWWY" varchar(10) NOT NULL default ' ',
  "AAAAYJYC" varchar(3) NOT NULL default ' ',
  "AAABMRCK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAWWX","AAABAWWY","AAAAYJYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBGOP" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABCPET" decimal(16,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBGOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBGOP" varchar(3) NOT NULL default ' ',
  "AAABGCUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBGOP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAAAYJZC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBGOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYJZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWX" varchar(10) NOT NULL default ' ',
  "AAAAYKAW" varchar(2) NOT NULL default '00',
  "AAAAYKAX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYJZO" varchar(1) NOT NULL default ' ',
  "AAAAYJZP" varchar(2) NOT NULL default ' ',
  "AAAAYJZQ" varchar(2) NOT NULL default '00',
  "AAAAYJZR" varchar(8) NOT NULL default ' ',
  "AAAAYJZS" varchar(2) NOT NULL default '00',
  "AAAAYJZT" varchar(1) NOT NULL default ' ',
  "AAAAYJZU" varchar(2) NOT NULL default '00',
  "AAAAYJZV" varchar(2) NOT NULL default '00',
  "AAAAYJZW" varchar(2) NOT NULL default '00',
  "AAAAYJZX" varchar(2) NOT NULL default '00',
  "AAAAYJZY" varchar(2) NOT NULL default '00',
  "AAAAYJZZ" varchar(2) NOT NULL default '00',
  "AAAAYKAA" varchar(2) NOT NULL default '00',
  "AAAAYKAB" varchar(2) NOT NULL default '00',
  "AAAAYKAC" varchar(2) NOT NULL default '00',
  "AAAAYKAD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWX","AAAAYKAW","AAAAYKAX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKAK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWX" varchar(10) NOT NULL default ' ',
  "AAAAYKAW" varchar(2) NOT NULL default '00',
  "AAAAYKAX" varchar(2) NOT NULL default ' ',
  "AAABGCUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABAWWX","AAAAYKAW","AAAAYKAX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKAY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAAAYKBG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBGOU","AAAAYKBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBGOP" varchar(3) NOT NULL default ' ',
  "AAAAYKBR" varchar(1) NOT NULL default ' ',
  "AAABBGWH" varchar(1) NOT NULL default ' ',
  "AAABCPET" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBGOP","AAAAYKBR","AAABBGWH","AAABCPET")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKBU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAWWX" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLRNO" varchar(1) NOT NULL default ' ',
  "AAABBOPO" varchar(1) NOT NULL default ' ',
  "AAAAYKCC" varchar(2) NOT NULL default '00',
  "AAAAYKCD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAWWX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZDBQ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKCO" varchar(6) NOT NULL default '000000',
  "AAAAYKCP" varchar(3) NOT NULL default ' ',
  "AAAAYKCQ" varchar(2) NOT NULL default '00',
  "AAAAYKCR" varchar(2) NOT NULL default '00',
  "AAAAYKCS" varchar(2) NOT NULL default '00',
  "AAAAYKCT" varchar(2) NOT NULL default '00',
  "AAAAYKCU" varchar(2) NOT NULL default '00',
  "AAAAYKCV" varchar(2) NOT NULL default '00',
  "AAAAYKCW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAZDBQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZDBQ" varchar(2) NOT NULL default '00',
  "AAABGCUQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAZDBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYKDS" varchar(2) NOT NULL default ' ',
  "AAAAYKDT" varchar(2) NOT NULL default '00',
  "AAAAYKDU" varchar(2) NOT NULL default '00',
  "AAAAYKDV" varchar(2) NOT NULL default '00',
  "AAAAYKDW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABMCXS" varchar(1) NOT NULL default ' ',
  "AAAAYKFL" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKFF" varchar(8) NOT NULL default '00000000',
  "AAAAZXEZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABMCXS","AAAAYKFL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABCADZ" varchar(4) NOT NULL default ' ',
  "AAAAYKET" varchar(1) NOT NULL default ' ',
  "AAAAZGUT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAABCADZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKEY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAAAYLJM" varchar(1) NOT NULL default ' ',
  "AAAAYKFL" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKFF" varchar(8) NOT NULL default '00000000',
  "AAABAYVH" decimal(9,2) NOT NULL default '0.00',
  "AAAAZGYA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAEC","AAAAYLJM","AAAAYKFL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKFN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZXWL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZXWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKFS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZXWL" varchar(2) NOT NULL default ' ',
  "AAABLPUZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZXWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKGA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKGM" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKGF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKGM" varchar(3) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKGM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKHX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKGS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKHX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYMV" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKHB" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHYMV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKHF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKHZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKHZ" varchar(3) NOT NULL default ' ',
  "AAAAYKHO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKHZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKHZ" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKHX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKHZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKJC" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKIG" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKJC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKIK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZCRR" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZCRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZCRR" varchar(3) NOT NULL default ' ',
  "AAAAYKIT" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZCRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKIX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZCRR" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKJC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZCRR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKJS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKJS" varchar(3) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKKF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKJY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKKF" varchar(3) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKKF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHLV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAMFH" varchar(1) NOT NULL default ' ',
  "AAABAQOM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHLV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKKQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKKU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKPE" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKLT" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKLI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKLT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKLT" varchar(2) NOT NULL default ' ',
  "AAAAYKLQ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKLT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKLZ" varchar(2) NOT NULL default ' ',
  "AAABJDQU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKLZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKMA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKMM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKMM" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAAAYKXX" varchar(10) NOT NULL default '0000000000',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABHKSD" varchar(6) NOT NULL default '000000',
  "AAABHDEU" varchar(12) NOT NULL default ' ',
  "AAAAYKYB" varchar(1) NOT NULL default ' ',
  "AAAAYKYC" varchar(1) NOT NULL default ' ',
  "AAAAYKMW" varchar(8) NOT NULL default ' ',
  "AAAAYKMX" varchar(8) NOT NULL default ' ',
  "AAAAYKMY" varchar(40) NOT NULL default ' ',
  "AAAAYKMZ" varchar(40) NOT NULL default ' ',
  "AAAAYKYF" varchar(6) NOT NULL default '000000',
  "AAAAYKYG" varchar(6) NOT NULL default '000000',
  "AAAAYKYH" varchar(6) NOT NULL default '000000',
  "AAAAYKYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKNG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHYMV" varchar(2) NOT NULL default ' ',
  "AAAAYKNJ" varchar(1) NOT NULL default ' ',
  "AAAAYKNK" varchar(1) NOT NULL default ' ',
  "AAAAYKNL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHYMV" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHYMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKNW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHCQK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHCQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHCQK" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKOO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHCQK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHCQK" varchar(2) NOT NULL default '00',
  "AAAAYKOO" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHCQK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKOS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKPQ" varchar(4) NOT NULL default ' ',
  "AAAAYKPE" varchar(4) NOT NULL default ' ',
  "AAAAYKPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKOZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKPQ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKPE" varchar(4) NOT NULL default ' ',
  "AAAAYKPF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKPQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKPJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKPQ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKPR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKQP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYKQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKPW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKQP" varchar(2) NOT NULL default '00',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKQE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKQR" varchar(2) NOT NULL default ' ',
  "AAAAYKQP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYKQR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKQR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKQP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYKQR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIKBY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIKBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABIKBY" varchar(2) NOT NULL default ' ',
  "AAAAYKRC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABIKBY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKRG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKRP" varchar(4) NOT NULL default ' ',
  "AAAAYKRJ" varchar(60) NOT NULL default ' ',
  "AAAAYKRK" varchar(30) NOT NULL default ' ',
  "AAAAYKRL" varchar(10) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABKGBL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKRP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKRQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  "AAABJXCE" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABLYTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKRW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAQOM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAQOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKSB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAQOM" varchar(2) NOT NULL default ' ',
  "AAAAYKSF" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAQOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKSJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKTE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYKTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKSO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKTE" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKTB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKTE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKSX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKTE" varchar(2) NOT NULL default '00',
  "AAAAYKTB" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKTV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKTS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKTV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKTO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKTV" varchar(2) NOT NULL default '00',
  "AAAAYKTS" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKUX" varchar(2) NOT NULL default '00',
  "AAAAYKUJ" varchar(1) NOT NULL default ' ',
  "AAAAYKUK" varchar(1) NOT NULL default ' ',
  "AAAAYKUL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKUE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKUX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKUJ" varchar(1) NOT NULL default ' ',
  "AAAAYKUK" varchar(1) NOT NULL default ' ',
  "AAAAYKUL" varchar(1) NOT NULL default ' ',
  "AAAAYKUU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKUX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKUQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKUX" varchar(2) NOT NULL default '00',
  "AAAAYKUU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKUX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKUY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKVK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKVD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKVK" varchar(1) NOT NULL default ' ',
  "AAAAYKVH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKVK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKVL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKWB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKVY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKWB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYKWB" varchar(2) NOT NULL default '00',
  "AAAAYKVY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYKWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTIK" varchar(4) NOT NULL default ' ',
  "AAAAYKWP" varchar(2) NOT NULL default ' ',
  "AAAAYKWQ" varchar(2) NOT NULL default ' ',
  "AAAAYKWR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHTIK" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKWP" varchar(2) NOT NULL default ' ',
  "AAAAYKWQ" varchar(2) NOT NULL default ' ',
  "AAAAYKWR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHTIK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKWV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABHTIK" varchar(4) NOT NULL default ' ',
  "AAAAYKWZ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABHTIK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKXD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKXR" varchar(8) NOT NULL default ' ',
  "AAABMLPV" varchar(40) NOT NULL default ' ',
  "AAABLYTE" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYKXR" varchar(8) NOT NULL default ' ',
  "AAABAHTV" varchar(3) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  "AAAAZOPD" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYKXR","AAABAHTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWW" varchar(8) NOT NULL default ' ',
  "AAABMQHY" varchar(4) NOT NULL default '0000',
  "AAAAYKXX" varchar(10) NOT NULL default '0000000000',
  "AAABFSUI" varchar(8) NOT NULL default '00000000',
  "AAABHKSD" varchar(6) NOT NULL default '000000',
  "AAABHDEU" varchar(12) NOT NULL default ' ',
  "AAAAYKYB" varchar(1) NOT NULL default ' ',
  "AAAAYKYC" varchar(1) NOT NULL default ' ',
  "AAAAYKYD" varchar(8) NOT NULL default ' ',
  "AAAAYKYE" varchar(40) NOT NULL default ' ',
  "AAAAYKYF" varchar(6) NOT NULL default '000000',
  "AAAAYKYG" varchar(6) NOT NULL default '000000',
  "AAAAYKYH" varchar(6) NOT NULL default '000000',
  "AAAAYKYI" varchar(6) NOT NULL default '000000',
  "AAAAYKYJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCWW","AAABMQHY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKYN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAAAYKYS" varchar(1) NOT NULL default ' ',
  "AAABFVFF" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGCHD","AAABBGOU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKYY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZZGW" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKZAY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZZGW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKZH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKZM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDSYA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYKZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLAC" varchar(4) NOT NULL default '0000',
  "AAAAYLAD" varchar(4) NOT NULL default '0000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYKZW" varchar(1) NOT NULL default ' ',
  "AAAAYKZX" varchar(1) NOT NULL default ' ',
  "AAAAYKZY" varchar(1) NOT NULL default ' ',
  "AAAAYKZZ" varchar(1) NOT NULL default ' ',
  "AAAAYLAA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLAC","AAAAYLAD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLAU" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYLAU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYLAU" varchar(3) NOT NULL default ' ',
  "AAABBEWU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYLAU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLAV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAAAYLBV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLBC" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBAKL","AAAAYLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLBI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBAKL" varchar(4) NOT NULL default ' ',
  "AAAAYLBV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLBQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABBAKL","AAAAYLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAKU" varchar(2) NOT NULL default ' ',
  "AAABINFJ" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKGMO" decimal(9,2) NOT NULL default '0.00',
  "AAAAZGYA" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAKU","AAABINFJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLCJ" (
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABLMVF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYLDF" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLCZ" varchar(4) NOT NULL default ' ',
  "AAAAYLDA" varchar(1) NOT NULL default ' ',
  "AAABLJJO" varchar(1) NOT NULL default ' ',
  "AAABMLCP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYLDF","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLDI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDSYA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABDSYA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAKU" varchar(2) NOT NULL default ' ',
  "AAABINFJ" varchar(6) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXEZ" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAKU","AAABINFJ","AAABCAEC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLED" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCADZ" varchar(4) NOT NULL default ' ',
  "AAAAYLEO" varchar(1) NOT NULL default ' ',
  "AAABCAEC" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLEK" varchar(1) NOT NULL default ' ',
  "AAAAYLEL" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCADZ","AAAAYLEO","AAABCAEC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLER" (
  "AAAAYORE" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLEV" decimal(9,2) NOT NULL default '0.00',
  "AAAAYLEW" decimal(9,2) NOT NULL default '0.00',
  "AAAAYLEX" decimal(9,2) NOT NULL default '0.00',
  "AAAAYLEY" decimal(9,2) NOT NULL default '0.00',
  "AAAAYLEZ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYORE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBLIL" varchar(4) NOT NULL default ' ',
  "AAAAYLID" VARBINARY(255) NOT NULL,
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMQCD" varchar(4) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBLIL","AAAAYLID"(255),"AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAYLIB" varchar(2) NOT NULL default ' ',
  "AAAAYLIC" varchar(1) NOT NULL default ' ',
  "AAAAYLIG" varchar(1) NOT NULL default ' ',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAAAYLID" VARBINARY(255),
  "AAABJSBF" varchar(2) NOT NULL default '00',
  "AAAAYLIF" varchar(1) NOT NULL default ' ',
  "AAAAYLIH" varchar(1) NOT NULL default ' ',
  "AAAAYLII" varchar(2) NOT NULL default '00',
  "AAAAYLIJ" varchar(1) NOT NULL default ' ',
  "AAAAYLIK" varchar(1) NOT NULL default ' ',
  "AAAAYLIL" varchar(1) NOT NULL default ' ',
  "AAAAYLIM" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIN" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIO" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIP" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIR" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIS" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIT" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIU" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIV" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIW" decimal(15,2) NOT NULL default '0.00',
  "AAABLKLI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAYLIB","AAAAYLIC","AAAAYLIG","AAABBCSI","AAABAWCO","AAABAXJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLGZ" (
  "AAAAYLHF" varchar(2) NOT NULL default '00',
  "AAAAYLHT" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLHE" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYLHF","AAAAYLHT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLHI" (
  "AAAAYLIB" varchar(2) NOT NULL default ' ',
  "AAAAYLHT" varchar(2) NOT NULL default '00',
  "AAAAYLIC" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLHO" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYLHP" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYLHQ" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYLHR" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAAAYLIB","AAAAYLHT","AAAAYLIC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLIB" varchar(2) NOT NULL default ' ',
  "AAAAYLIC" varchar(1) NOT NULL default ' ',
  "AAAAYLID" VARBINARY(255),
  "AAABJSBF" varchar(2) NOT NULL default '00',
  "AAAAYLIF" varchar(1) NOT NULL default ' ',
  "AAAAYLIG" varchar(1) NOT NULL default ' ',
  "AAAAYLIH" varchar(1) NOT NULL default ' ',
  "AAAAYLII" varchar(2) NOT NULL default '00',
  "AAAAYLIJ" varchar(1) NOT NULL default ' ',
  "AAAAYLIK" varchar(1) NOT NULL default ' ',
  "AAAAYLIL" varchar(1) NOT NULL default ' ',
  "AAAAYLIM" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIN" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIO" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIP" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIR" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIS" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIT" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIU" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIV" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLIW" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLJA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLXTF" varchar(4) NOT NULL default '0000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAYLJE" varchar(10) NOT NULL default ' ',
  "AAABBAKU" varchar(2) NOT NULL default ' ',
  "AAABINFJ" varchar(6) NOT NULL default ' ',
  "AAABBRWV" varchar(1) NOT NULL default ' ',
  "AAAAYLJI" varchar(4) NOT NULL default ' ',
  "AAAAYLJJ" varchar(8) NOT NULL default '00000000',
  "AAAAYLJK" varchar(8) NOT NULL default '00000000',
  "AAAAYLJL" varchar(8) NOT NULL default '00000000',
  "AAAAYLJM" varchar(1) NOT NULL default ' ',
  "AAAAYLJN" varchar(3) NOT NULL default ' ',
  "AAAAYLJO" varchar(3) NOT NULL default '000',
  "AAABIWTA" varchar(1) NOT NULL default ' ',
  "AAAAYLJQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLJR" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLJS" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLJT" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLJU" decimal(15,2) NOT NULL default '0.00',
  "AAAAYSFV" varchar(1) NOT NULL default ' ',
  "AAAAYLJW" varchar(1) NOT NULL default ' ',
  "AAAAYLJX" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLJY" varchar(10) NOT NULL default ' ',
  "AAAAYLJZ" varchar(8) NOT NULL default '00000000',
  "AAAAYLKA" varchar(2) NOT NULL default ' ',
  "AAAAYLKB" varchar(6) NOT NULL default ' ',
  "AAAAYLKC" varchar(2) NOT NULL default ' ',
  "AAAAYLKD" varchar(3) NOT NULL default ' ',
  "AAAAYLKE" varchar(20) NOT NULL default ' ',
  "AAABBHOB" varchar(4) NOT NULL default '0000',
  "AAAAYLKG" varchar(8) NOT NULL default '00000000',
  "AAABMDEN" varchar(40) NOT NULL default ' ',
  "AAABJFLO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABLXTF","AAABMCRV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLKM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAAAYLKQ" varchar(1) NOT NULL default ' ',
  "AAAAYLKR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABDBGB" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLLE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYPNA" varchar(4) NOT NULL default ' ',
  "AAAAYPLA" varchar(4) NOT NULL default ' ',
  "AAAAYQUN" varchar(4) NOT NULL default ' ',
  "AAAAYNIU" varchar(4) NOT NULL default ' ',
  "AAAAYLOM" varchar(4) NOT NULL default ' ',
  "AAAAYPUN" varchar(5) NOT NULL default '00000',
  "AAAAYPPE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLLS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLTL" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLLX" varchar(4) NOT NULL default ' ',
  "AAAAYLLY" varchar(2) NOT NULL default '00',
  "AAAAYLLZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLTL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLTT" varchar(4) NOT NULL default ' ',
  "AAAAYLTL" varchar(2) NOT NULL default '00',
  "AAAAYMGV" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMGP" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMGQ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFH" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFI" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFJ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFK" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFL" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFM" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFN" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFO" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFP" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFQ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFR" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFS" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFT" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFU" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFV" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFW" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFX" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFY" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFZ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMGA" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYLTT","AAAAYLTL","AAAAYMGV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLNL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLTD" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLNQ" varchar(2) NOT NULL default '00',
  "AAAAYQMP" varchar(4) NOT NULL default ' ',
  "AAAAYLNS" varchar(2) NOT NULL default '00',
  "AAAAYLNT" varchar(2) NOT NULL default '00',
  "AAAAYLNU" varchar(2) NOT NULL default '00',
  "AAAAYLNV" varchar(2) NOT NULL default '00',
  "AAAAYLNW" varchar(2) NOT NULL default '00',
  "AAAAYLNX" varchar(2) NOT NULL default '00',
  "AAAAYLNY" varchar(2) NOT NULL default '00',
  "AAAAYLNZ" varchar(2) NOT NULL default '00',
  "AAAAYLOA" varchar(2) NOT NULL default '00',
  "AAAAYLOB" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYLTD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLOM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQMP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLOM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLOW" varchar(4) NOT NULL default ' ',
  "AAAAYLSV" varchar(4) NOT NULL default ' ',
  "AAABKQXT" varchar(1) NOT NULL default ' ',
  "AAAAYLPE" varchar(4) NOT NULL default ' ',
  "AAAAYLPM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLOW","AAAAYLSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLPE" varchar(4) NOT NULL default ' ',
  "AAAAYLPB" varchar(2) NOT NULL default ' ',
  "AAAAYLPC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLPE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLPM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLPK" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLPM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYLSV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLQK" varchar(4) NOT NULL default ' ',
  "AAAAYLPW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYLSV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLQK" varchar(4) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLQI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLQK","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLTD" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMLC" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLTD","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLQY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLSV" varchar(4) NOT NULL default ' ',
  "AAABJTTT" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLRE" varchar(4) NOT NULL default ' ',
  "AAAAYLRF" varchar(1) NOT NULL default ' ',
  "AAAAYLRG" varchar(4) NOT NULL default ' ',
  "AAAAYLRH" varchar(1) NOT NULL default ' ',
  "AAAAYLRI" varchar(4) NOT NULL default ' ',
  "AAAAYLRJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLSV","AAABJTTT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLSV" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLRT" varchar(4) NOT NULL default ' ',
  "AAAAYLRU" varchar(1) NOT NULL default ' ',
  "AAAAYLRV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLSV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYLTT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLSE" decimal(5,2) NOT NULL default '0.00',
  "AAAAYMNC" varchar(4) NOT NULL default ' ',
  "AAAAYLSG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYLTT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLSK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYLSV" varchar(4) NOT NULL default ' ',
  "AAAAYLSQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEON","AAABBEOO","AAAAYLSV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYLTD" varchar(2) NOT NULL default ' ',
  "AAAAYLTA" varchar(42) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYLTD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYLTL" varchar(2) NOT NULL default '00',
  "AAAAYLTI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYLTL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYLTT" varchar(4) NOT NULL default ' ',
  "AAAAYLTQ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYLTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLTU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYPTJ" varchar(5) NOT NULL default ' ',
  "AAAAYLZA" varchar(3) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQOG" decimal(15,2) NOT NULL default '0.00',
  "AAAAYQOH" decimal(15,2) NOT NULL default '0.00',
  "AAABKCIB" varchar(1) NOT NULL default ' ',
  "AAAAYQOJ" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYQOK" decimal(15,2) NOT NULL default '0.00',
  "AAAAYQOL" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLUL" varchar(4) NOT NULL default ' ',
  "AAAAYLUM" varchar(1) NOT NULL default ' ',
  "AAAAYLUN" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYLUO" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLUP" decimal(15,2) NOT NULL default '0.00',
  "AAAAYLUQ" varchar(4) NOT NULL default ' ',
  "AAAAYLUR" varchar(4) NOT NULL default ' ',
  "AAAAYLUS" varchar(4) NOT NULL default ' ',
  "AAAAYQRV" varchar(4) NOT NULL default ' ',
  "AAAAYLUU" varchar(4) NOT NULL default ' ',
  "AAAAYLUV" varchar(4) NOT NULL default ' ',
  "AAAAYLUW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYPPQ","AAAAYPTJ","AAAAYLZA","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLVG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYPTJ" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMEW" varchar(1) NOT NULL default ' ',
  "AAAAYLVQ" varchar(4) NOT NULL default ' ',
  "AAAAYLVR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYPPQ","AAAAYPTJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLVZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYPTJ" varchar(5) NOT NULL default ' ',
  "AAAAYLZA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLWJ" varchar(4) NOT NULL default ' ',
  "AAAAYLXK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYPPQ","AAAAYPTJ","AAAAYLZA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYLXK" varchar(4) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYLXD" varchar(4) NOT NULL default ' ',
  "AAAAYLXE" decimal(8,2) NOT NULL default '0.00',
  "AAAAYLXF" varchar(4) NOT NULL default ' ',
  "AAAAYLXG" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYLXK","AAABBEOP","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLXP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYPTJ" varchar(5) NOT NULL default ' ',
  "AAAAYLZA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQPP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYPPQ","AAAAYPTJ","AAAAYLZA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLYI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYPTJ" varchar(5) NOT NULL default ' ',
  "AAAAYLZA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMEO" varchar(2) NOT NULL default ' ',
  "AAAAYLYT" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYPPQ","AAAAYPTJ","AAAAYLZA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPUY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPSO" varchar(2) NOT NULL default '00',
  "AAAAYPSP" varchar(2) NOT NULL default '00',
  "AAAAYLZL" varchar(2) NOT NULL default '00',
  "AAAAYLZM" varchar(2) NOT NULL default '00',
  "AAAAYLZN" varchar(2) NOT NULL default '00',
  "AAAAYLZO" varchar(2) NOT NULL default '00',
  "AAAAYLZP" varchar(2) NOT NULL default '00',
  "AAAAYLZQ" varchar(2) NOT NULL default '00',
  "AAAAYLZR" varchar(2) NOT NULL default '00',
  "AAAAYLZS" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYPUY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYLZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYMEO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMBA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYPPQ","AAAAYMEO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYMBA" varchar(4) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMAV" varchar(4) NOT NULL default ' ',
  "AAABFLSG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYMBA","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMBD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYQXI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEON","AAABBEOO","AAAAYPPQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMBP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMEW" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYMEW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMBX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYPTJ" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMCF" varchar(3) NOT NULL default '000',
  "AAAAYMCG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYPPQ","AAAAYPTJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMCU" varchar(4) NOT NULL default ' ',
  "AAAAYMCV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYPPQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMDB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYPTJ" varchar(5) NOT NULL default ' ',
  "AAAAYQWZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEON","AAABBEOO","AAAAYPPQ","AAAAYPTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMDP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPUY" varchar(2) NOT NULL default ' ',
  "AAAAYMDV" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEON","AAABBEOO","AAAAYPUY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYMEO" varchar(2) NOT NULL default ' ',
  "AAAAYMEI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEON","AAABBEOO","AAAAYPPQ","AAAAYMEO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYMEW" varchar(1) NOT NULL default ' ',
  "AAAAYMET" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYMEW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMEX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMOE" varchar(4) NOT NULL default ' ',
  "AAAAYMPC" varchar(2) NOT NULL default '00',
  "AAAAYMGV" varchar(2) NOT NULL default '00',
  "AAAAYMND" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMGP" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMGQ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFH" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFI" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFJ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFK" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFL" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFM" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFN" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFO" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFP" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFQ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFR" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFS" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFT" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFU" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFV" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFW" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFX" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFY" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMFZ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMGA" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYMOE","AAAAYMPC","AAAAYMGV","AAAAYMND","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMOE" varchar(4) NOT NULL default ' ',
  "AAAAYMMV" varchar(2) NOT NULL default '00',
  "AAAAYMGV" varchar(2) NOT NULL default '00',
  "AAAAYMND" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMGP" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMGQ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMGR" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYMOE","AAAAYMMV","AAAAYMGV","AAAAYMND","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMGY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMOM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMHD" varchar(2) NOT NULL default '00',
  "AAAAYMHE" varchar(2) NOT NULL default '00',
  "AAAAYMHF" varchar(2) NOT NULL default '00',
  "AAAAYMHG" varchar(2) NOT NULL default '00',
  "AAAAYMHH" varchar(2) NOT NULL default '00',
  "AAAAYMHI" varchar(2) NOT NULL default '00',
  "AAAAYMHJ" varchar(2) NOT NULL default '00',
  "AAAAYMHK" varchar(2) NOT NULL default '00',
  "AAAAYMHL" varchar(2) NOT NULL default '00',
  "AAAAYMHM" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYMOM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMHQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMLC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQMP" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYMLC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMHZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYODM" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMIG" varchar(4) NOT NULL default ' ',
  "AAAAYMIH" varchar(2) NOT NULL default '00',
  "AAAAYMII" decimal(6,4) NOT NULL default '0.0000',
  "AAAAYMIJ" varchar(2) NOT NULL default '00',
  "AAAAYMIK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYODM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMIW" varchar(4) NOT NULL default ' ',
  "AAAAYMIX" varchar(4) NOT NULL default ' ',
  "AAAAYMIY" varchar(4) NOT NULL default ' ',
  "AAAAYMIZ" varchar(4) NOT NULL default ' ',
  "AAAAYMJA" varchar(4) NOT NULL default ' ',
  "AAAAYMJB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMOE" varchar(4) NOT NULL default ' ',
  "AAAAYMPC" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMJM" varchar(2) NOT NULL default '00',
  "AAAAYMJN" varchar(2) NOT NULL default '00',
  "AAAAYMJO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYMOE","AAAAYMPC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPUW" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMJY" varchar(2) NOT NULL default '00',
  "AAAAYMJZ" varchar(2) NOT NULL default '00',
  "AAAAYMKA" varchar(2) NOT NULL default '00',
  "AAAAYMKB" varchar(2) NOT NULL default '00',
  "AAAAYMKC" varchar(2) NOT NULL default '00',
  "AAAAYMKD" varchar(2) NOT NULL default '00',
  "AAAAYMKE" varchar(2) NOT NULL default '00',
  "AAAAYMKF" varchar(2) NOT NULL default '00',
  "AAAAYMKG" varchar(2) NOT NULL default '00',
  "AAAAYMKH" varchar(2) NOT NULL default '00',
  "AAAAYMKI" varchar(2) NOT NULL default '00',
  "AAAAYMKJ" varchar(2) NOT NULL default '00',
  "AAAAYMKK" varchar(2) NOT NULL default '00',
  "AAAAYMKL" varchar(2) NOT NULL default '00',
  "AAAAYMKM" varchar(2) NOT NULL default '00',
  "AAAAYMKN" varchar(2) NOT NULL default '00',
  "AAAAYMKO" varchar(2) NOT NULL default '00',
  "AAAAYMKP" varchar(2) NOT NULL default '00',
  "AAAAYMKQ" varchar(2) NOT NULL default '00',
  "AAAAYMKR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYPUW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYMLC" varchar(4) NOT NULL default ' ',
  "AAAAYMKZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYMLC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYMME" varchar(2) NOT NULL default '00',
  "AAAAYMLH" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYMME")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYMMV" varchar(2) NOT NULL default '00',
  "AAAAYMLP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYMMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMME" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPXL" varchar(4) NOT NULL default ' ',
  "AAAAYMLZ" varchar(4) NOT NULL default ' ',
  "AAAAYMNC" varchar(4) NOT NULL default ' ',
  "AAAAYMMB" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMMC" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAYMME","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMMG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMMV" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMNC" varchar(4) NOT NULL default ' ',
  "AAAAYMND" varchar(2) NOT NULL default ' ',
  "AAAAYPXL" varchar(4) NOT NULL default ' ',
  "AAAAYMNG" varchar(4) NOT NULL default ' ',
  "AAAAYMNH" varchar(4) NOT NULL default ' ',
  "AAAAYMNF" varchar(4) NOT NULL default ' ',
  "AAAAYMMR" varchar(4) NOT NULL default ' ',
  "AAAAYMMS" varchar(4) NOT NULL default ' ',
  "AAAAYMNJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYMMV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMPC" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMNC" varchar(4) NOT NULL default ' ',
  "AAAAYMND" varchar(2) NOT NULL default ' ',
  "AAAAYPXL" varchar(4) NOT NULL default ' ',
  "AAAAYMNF" varchar(4) NOT NULL default ' ',
  "AAAAYMNG" varchar(4) NOT NULL default ' ',
  "AAAAYMNH" varchar(4) NOT NULL default ' ',
  "AAAAYMNI" varchar(4) NOT NULL default ' ',
  "AAAAYMNJ" varchar(4) NOT NULL default ' ',
  "AAAAYMNK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYMPC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMOE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMNT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYMOE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYMOE" varchar(4) NOT NULL default ' ',
  "AAAAYMOB" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYMOE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMOF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYMOM" varchar(2) NOT NULL default ' ',
  "AAAAYMOJ" varchar(42) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYMOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPUW" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMOV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYMPC" varchar(2) NOT NULL default '00',
  "AAAAYMOZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYMPC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYMTX" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMQC" varchar(2) NOT NULL default ' ',
  "AAAAYMPL" varchar(1) NOT NULL default ' ',
  "AAAAYMPM" varchar(2) NOT NULL default '00',
  "AAAAYMQG" varchar(1) NOT NULL default ' ',
  "AAAAYQPP" varchar(2) NOT NULL default ' ',
  "AAAAYMQI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYRCV","AAAAYMTX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYMTX" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMQC" varchar(2) NOT NULL default ' ',
  "AAAAYPBA" varchar(2) NOT NULL default '00',
  "AAAAYPBB" varchar(3) NOT NULL default '000',
  "AAABFBBL" varchar(3) NOT NULL default '000',
  "AAAAYMQG" varchar(1) NOT NULL default ' ',
  "AAABGGED" varchar(1) NOT NULL default ' ',
  "AAAAYMQI" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYRCV","AAAAYMTX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMQO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYMTX" varchar(8) NOT NULL default ' ',
  "AAAAYPCI" varchar(4) NOT NULL default ' ',
  "AAAAYMRD" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMQW" varchar(5) NOT NULL default ' ',
  "AAAAYQZZ" varchar(1) NOT NULL default ' ',
  "AAAAYPCE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYMTX","AAAAYPCI","AAAAYMRD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMRF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYMTX" varchar(8) NOT NULL default ' ',
  "AAAAYMRR" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYRCV","AAAAYMTX","AAAAYMRR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMRT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYMTX" varchar(8) NOT NULL default ' ',
  "AAAAYPBJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOWM" varchar(6) NOT NULL default ' ',
  "AAAAYOWN" varchar(4) NOT NULL default ' ',
  "AAAAYOWO" varchar(8) NOT NULL default ' ',
  "AAABBQFF" varchar(5) NOT NULL default ' ',
  "AAAAYOWQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYRCV","AAAAYMTX","AAAAYPBJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMSM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAAAYMSQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEOP","AAABGLBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYMTA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMTG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOPS" varchar(2) NOT NULL default ' ',
  "AAAAYMTJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAOPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMTM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYMTX" varchar(8) NOT NULL default ' ',
  "AAAAYMTS" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPES","AAAAYRCV","AAAAYMTX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNAQ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYNAQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYNAQ" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKWRL" varchar(2) NOT NULL default '00',
  "AAAAYMUQ" varchar(1) NOT NULL default ' ',
  "AAAAYMUR" decimal(5,2) NOT NULL default '0.00',
  "AAAAYMUS" decimal(4,2) NOT NULL default '0.00',
  "AAAAYMUT" decimal(12,2) NOT NULL default '0.00',
  "AAAAYMUU" decimal(4,2) NOT NULL default '0.00',
  "AAAAYMUV" varchar(4) NOT NULL default ' ',
  "AAABAAGY" varchar(4) NOT NULL default ' ',
  "AAAAYMVR" varchar(4) NOT NULL default ' ',
  "AAAAYMUY" varchar(4) NOT NULL default ' ',
  "AAAAYMUZ" varchar(4) NOT NULL default ' ',
  "AAAAYMVA" varchar(4) NOT NULL default ' ',
  "AAAAYMVB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYNAQ","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYMZW" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMVQ" varchar(1) NOT NULL default '0',
  "AAAAYMVR" varchar(4) NOT NULL default ' ',
  "AAAAYMVS" varchar(1) NOT NULL default '0',
  "AAAAYMVT" varchar(4) NOT NULL default ' ',
  "AAAAZDQQ" varchar(1) NOT NULL default '0',
  "AAAAYMVV" varchar(1) NOT NULL default '0',
  "AAAAYMVW" varchar(4) NOT NULL default ' ',
  "AAAAYMVX" varchar(4) NOT NULL default ' ',
  "AAAAYMVY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYMZW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMWL" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMWJ" decimal(11,6) NOT NULL default '0.000000',
  PRIMARY KEY ("AAABMRIL","AAAAYMWL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMWN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYMZW" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMWW" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYMZW","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYNAI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMYB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYNAI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMXT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYMYB" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMXZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYMYB","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMYE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYNAI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYMYL" varchar(1) NOT NULL default ' ',
  "AAAAYMYM" varchar(2) NOT NULL default ' ',
  "AAAAYMYN" varchar(2) NOT NULL default ' ',
  "AAAAYMYO" varchar(2) NOT NULL default ' ',
  "AAAAYMYP" varchar(2) NOT NULL default ' ',
  "AAAAYMYQ" varchar(2) NOT NULL default ' ',
  "AAAAYMYR" varchar(2) NOT NULL default ' ',
  "AAAAYMYS" varchar(2) NOT NULL default ' ',
  "AAAAYMYT" varchar(2) NOT NULL default ' ',
  "AAAAYMYU" varchar(2) NOT NULL default ' ',
  "AAAAYMYV" varchar(2) NOT NULL default ' ',
  "AAAAYMYW" varchar(2) NOT NULL default ' ',
  "AAAAYMYX" varchar(2) NOT NULL default ' ',
  "AAAAYMYY" varchar(2) NOT NULL default ' ',
  "AAAAYMYZ" varchar(2) NOT NULL default ' ',
  "AAAAYMZA" varchar(2) NOT NULL default ' ',
  "AAAAYMZB" varchar(2) NOT NULL default ' ',
  "AAAAYMZC" varchar(2) NOT NULL default ' ',
  "AAAAYMZD" varchar(2) NOT NULL default ' ',
  "AAAAYMZE" varchar(2) NOT NULL default ' ',
  "AAAAYMZF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYNAI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMZL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYMZW" varchar(2) NOT NULL default ' ',
  "AAAAYMZR" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEON","AAABBEOO","AAAAYMZW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYMZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYNAI" varchar(2) NOT NULL default ' ',
  "AAABBGAT" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEON","AAABBEOO","AAAAYNAI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNAJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYNAQ" varchar(2) NOT NULL default ' ',
  "AAAAYNAN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYNAQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMKG" varchar(8) NOT NULL default '00000000',
  "AAAAYNGL" varchar(6) NOT NULL default '000000',
  "AAAAYNEK" varchar(2) NOT NULL default '00',
  "AAAAYNIV" varchar(4) NOT NULL default ' ',
  "AAAAYNIS" varchar(2) NOT NULL default ' ',
  "AAAAYNCD" varchar(2) NOT NULL default '00',
  "AAAAYNAZ" varchar(1) NOT NULL default ' ',
  "AAAAYNBA" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBB" decimal(15,2) NOT NULL default '0.00',
  "AAAAYNBC" decimal(12,2) NOT NULL default '0.00',
  "AAAAYNBD" decimal(12,2) NOT NULL default '0.00',
  "AAAAYNBE" varchar(4) NOT NULL default '0000',
  "AAAAYNBF" decimal(12,2) NOT NULL default '0.00',
  "AAAAYNBG" decimal(12,3) NOT NULL default '0.000',
  "AAAAYNBH" decimal(12,2) NOT NULL default '0.00',
  "AAAAYNBI" decimal(12,2) NOT NULL default '0.00',
  "AAAAYNBJ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYNBK" varchar(6) NOT NULL default '000000',
  "AAAAYNBL" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBM" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBN" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBO" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBP" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBR" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBS" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBT" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBU" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBV" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNBW" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBMKG","AAAAYNGL","AAAAYNEK","AAAAYNIV","AAAAYNIS","AAAAYNCD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNIU" varchar(4) NOT NULL default ' ',
  "AAAAYNIV" varchar(4) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNJF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYNIU","AAAAYNIV","AAABBEON","AAABBEOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNCT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNJF" varchar(4) NOT NULL default ' ',
  "AAAAYNDK" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNCZ" varchar(3) NOT NULL default '000',
  "AAAAYNDA" varchar(3) NOT NULL default '000',
  "AAAAYNDB" varchar(1) NOT NULL default ' ',
  "AAAAYNDC" varchar(4) NOT NULL default ' ',
  "AAAAYNDD" varchar(1) NOT NULL default ' ',
  "AAAAYNDE" varchar(4) NOT NULL default ' ',
  "AAAAYNDF" varchar(1) NOT NULL default ' ',
  "AAAAYNDG" varchar(4) NOT NULL default ' ',
  "AAAAYNDH" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYNJF","AAAAYNDK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNJF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYNJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNJF" varchar(4) NOT NULL default ' ',
  "AAAAYNEJ" varchar(4) NOT NULL default ' ',
  "AAAAYNEK" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNDY" varchar(2) NOT NULL default '00',
  "AAAAYNDZ" varchar(4) NOT NULL default ' ',
  "AAAAYNEA" varchar(4) NOT NULL default ' ',
  "AAAAYNEB" varchar(4) NOT NULL default ' ',
  "AAAAYNEX" varchar(4) NOT NULL default ' ',
  "AAAAYNED" varchar(2) NOT NULL default '00',
  "AAAAYNEE" varchar(1) NOT NULL default ' ',
  "AAAAYNEF" varchar(2) NOT NULL default '00',
  "AAAAYNEG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYNJF","AAAAYNEJ","AAAAYNEK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNEM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNJF" varchar(4) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNLA" varchar(4) NOT NULL default ' ',
  "AAAAYNET" varchar(2) NOT NULL default ' ',
  "AAAAYNEU" varchar(4) NOT NULL default ' ',
  "AAAAYNEV" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNEW" varchar(4) NOT NULL default ' ',
  "AAAAYNEX" varchar(4) NOT NULL default ' ',
  "AAAAYNEY" varchar(4) NOT NULL default ' ',
  "AAAAYNEZ" varchar(4) NOT NULL default ' ',
  "AAAAYNFA" varchar(4) NOT NULL default ' ',
  "AAAAYNFB" varchar(4) NOT NULL default ' ',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAAAZEFX" varchar(4) NOT NULL default ' ',
  "AAAAZEFY" varchar(4) NOT NULL default ' ',
  "AAAAZEFZ" varchar(4) NOT NULL default ' ',
  "AAAAZEGA" varchar(4) NOT NULL default ' ',
  "AAAAYNFH" varchar(4) NOT NULL default ' ',
  "AAAAYNFI" varchar(4) NOT NULL default ' ',
  "AAAAYNFJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYNJF","AAABGCHD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNGL" varchar(6) NOT NULL default '000000',
  "AAAAYNIV" varchar(4) NOT NULL default ' ',
  "AAAAYNIS" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNFV" varchar(8) NOT NULL default '00000000',
  "AAAAYNFW" varchar(10) NOT NULL default ' ',
  "AAAAYOYU" varchar(10) NOT NULL default ' ',
  "AAAAYNFY" varchar(8) NOT NULL default '00000000',
  "AAAAYNFZ" varchar(10) NOT NULL default ' ',
  "AAAAYNGA" varchar(1) NOT NULL default ' ',
  "AAAAYNGB" varchar(2) NOT NULL default '00',
  "AAAAYNGC" varchar(1) NOT NULL default ' ',
  "AAAAYNGD" varchar(2) NOT NULL default '00',
  "AAAAYNGE" varchar(1) NOT NULL default ' ',
  "AAAAYNGF" varchar(2) NOT NULL default '00',
  "AAAAYNGG" varchar(1) NOT NULL default ' ',
  "AAAAYNGH" varchar(2) NOT NULL default '00',
  "AAAAYNGI" varchar(1) NOT NULL default ' ',
  "AAAAYNGJ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYNGL","AAAAYNIV","AAAAYNIS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNIU" varchar(4) NOT NULL default ' ',
  "AAAAYNQD" varchar(2) NOT NULL default '00',
  "AAAAYNPI" varchar(2) NOT NULL default '00',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNGX" varchar(4) NOT NULL default ' ',
  "AAAAYNGY" decimal(12,2) NOT NULL default '0.00',
  "AAAAYNGZ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYNHA" decimal(12,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYNIU","AAAAYNQD","AAAAYNPI","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYNIU" varchar(4) NOT NULL default ' ',
  "AAAAYNIV" varchar(4) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPQT" varchar(4) NOT NULL default ' ',
  "AAAAYPQU" varchar(4) NOT NULL default ' ',
  "AAABILQC" varchar(30) NOT NULL default ' ',
  "AAAAYPQW" varchar(1) NOT NULL default ' ',
  "AAAAYPQX" varchar(2) NOT NULL default ' ',
  "AAAAYZTE" varchar(25) NOT NULL default ' ',
  "AAAAYPQZ" varchar(4) NOT NULL default ' ',
  "AAAAYPRA" varchar(4) NOT NULL default ' ',
  "AAABILQD" varchar(30) NOT NULL default ' ',
  "AAAAYPRC" varchar(1) NOT NULL default ' ',
  "AAAAYPRD" varchar(2) NOT NULL default ' ',
  "AAAAYZTF" varchar(25) NOT NULL default ' ',
  "AAAAYPRF" varchar(4) NOT NULL default ' ',
  "AAAAYPRG" varchar(4) NOT NULL default ' ',
  "AAABILQE" varchar(30) NOT NULL default ' ',
  "AAAAYPRI" varchar(1) NOT NULL default ' ',
  "AAAAYPRJ" varchar(2) NOT NULL default ' ',
  "AAAAYPRK" varchar(25) NOT NULL default ' ',
  "AAAAYPRL" varchar(4) NOT NULL default ' ',
  "AAAAYPRM" varchar(4) NOT NULL default ' ',
  "AAABILQF" varchar(30) NOT NULL default ' ',
  "AAAAYPRO" varchar(1) NOT NULL default ' ',
  "AAAAYPRP" varchar(2) NOT NULL default ' ',
  "AAAAYPRQ" varchar(25) NOT NULL default ' ',
  "AAAAYPRR" varchar(4) NOT NULL default ' ',
  "AAAAYPRS" varchar(4) NOT NULL default ' ',
  "AAABILQG" varchar(30) NOT NULL default ' ',
  "AAAAYPRU" varchar(1) NOT NULL default ' ',
  "AAAAYPRV" varchar(2) NOT NULL default ' ',
  "AAAAYPRW" varchar(25) NOT NULL default ' ',
  "AAAAYNIS" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYNIU","AAAAYNIV","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYNJF" varchar(4) NOT NULL default ' ',
  "AAAAYNJC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYNJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNJG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABAWCC" varchar(4) NOT NULL default ' ',
  "AAAAYNQD" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNJO" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJP" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJR" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJS" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJT" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJU" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJV" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJW" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJX" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJY" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNJZ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNKA" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNKB" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABAWCC","AAAAYNQD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNKI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNXM" varchar(2) NOT NULL default '00',
  "AAAAYOBL" varchar(2) NOT NULL default '00',
  "AAAAYNTM" decimal(6,3) NOT NULL default '0.000',
  "AAAAYNTN" decimal(6,3) NOT NULL default '0.000',
  "AAAAYNKT" varchar(4) NOT NULL default ' ',
  "AAAAYNKU" varchar(4) NOT NULL default ' ',
  "AAAAYNKV" varchar(4) NOT NULL default ' ',
  "AAAAYNKW" varchar(4) NOT NULL default ' ',
  "AAAAYNKX" varchar(4) NOT NULL default ' ',
  "AAAAYNKY" varchar(4) NOT NULL default ' ',
  "AAAAYNKZ" varchar(4) NOT NULL default ' ',
  "AAAAYNLA" varchar(4) NOT NULL default ' ',
  "AAAAYNLB" varchar(4) NOT NULL default ' ',
  "AAAAYNLC" varchar(4) NOT NULL default ' ',
  "AAAAYNLD" varchar(4) NOT NULL default ' ',
  "AAAAYNLE" varchar(4) NOT NULL default ' ',
  "AAAAYNLF" varchar(4) NOT NULL default ' ',
  "AAAAYNLG" varchar(4) NOT NULL default ' ',
  "AAAAYNLH" varchar(4) NOT NULL default ' ',
  "AAAAYNLI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNLO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAZBEI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNLU" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAAAZBEI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNLZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYOBT" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNMH" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNMI" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNMJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNMK" varchar(3) NOT NULL default '000',
  "AAAAYNML" varchar(4) NOT NULL default ' ',
  "AAAAYOAO" varchar(2) NOT NULL default ' ',
  "AAAAYOAQ" varchar(1) NOT NULL default ' ',
  "AAAAYNMO" varchar(4) NOT NULL default ' ',
  "AAAAYOAV" varchar(1) NOT NULL default ' ',
  "AAAAYOAT" varchar(2) NOT NULL default ' ',
  "AAAAYNMR" varchar(4) NOT NULL default ' ',
  "AAAAYOBA" varchar(1) NOT NULL default ' ',
  "AAAAYOAY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYOBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYOBT" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNNI" varchar(3) NOT NULL default '000',
  "AAAAYNNJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNNK" varchar(2) NOT NULL default ' ',
  "AAAAYNNL" varchar(2) NOT NULL default ' ',
  "AAAAYNNM" varchar(1) NOT NULL default ' ',
  "AAAAYNNN" varchar(1) NOT NULL default ' ',
  "AAAAYNNO" varchar(1) NOT NULL default ' ',
  "AAAAYNNP" varchar(1) NOT NULL default ' ',
  "AAAAYNNQ" varchar(1) NOT NULL default ' ',
  "AAAAYNNR" varchar(1) NOT NULL default ' ',
  "AAAAYNNS" varchar(1) NOT NULL default ' ',
  "AAAAYNNT" varchar(1) NOT NULL default ' ',
  "AAAAYNNU" varchar(4) NOT NULL default ' ',
  "AAAAYOAO" varchar(2) NOT NULL default ' ',
  "AAAAYOAQ" varchar(1) NOT NULL default ' ',
  "AAAAYNNX" varchar(4) NOT NULL default ' ',
  "AAAAYOAT" varchar(2) NOT NULL default ' ',
  "AAAAYOAV" varchar(1) NOT NULL default ' ',
  "AAAAYNOA" varchar(3) NOT NULL default '000',
  "AAAAYQQV" varchar(4) NOT NULL default ' ',
  "AAAAYOAY" varchar(2) NOT NULL default ' ',
  "AAAAYOBA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYOBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNOK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYOBT" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNOS" varchar(4) NOT NULL default ' ',
  "AAAAZBEI" varchar(2) NOT NULL default ' ',
  "AAAAZTYI" varchar(1) NOT NULL default ' ',
  "AAAAYNXT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYOBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNPC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNPI" varchar(2) NOT NULL default '00',
  "AAAAYOBT" varchar(1) NOT NULL default ' ',
  "AAAAYNPK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYNQD" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNPX" decimal(5,2) NOT NULL default '0.00',
  "AAAAYNPY" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYNQD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNQF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYOBT" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNQN" varchar(3) NOT NULL default '000',
  "AAAAYNXO" varchar(2) NOT NULL default ' ',
  "AAAAYNQP" varchar(4) NOT NULL default ' ',
  "AAAAYNXP" varchar(1) NOT NULL default ' ',
  "AAAAYNQR" varchar(3) NOT NULL default '000',
  "AAAAYNXR" varchar(2) NOT NULL default ' ',
  "AAAAYNQT" varchar(4) NOT NULL default ' ',
  "AAAAYNXS" varchar(1) NOT NULL default ' ',
  "AAAAYNQV" varchar(3) NOT NULL default '000',
  "AAAAYNVH" varchar(2) NOT NULL default ' ',
  "AAAAYNQX" varchar(4) NOT NULL default ' ',
  "AAAAYNYL" varchar(1) NOT NULL default ' ',
  "AAAAYNQZ" varchar(3) NOT NULL default '000',
  "AAAAYNYP" varchar(2) NOT NULL default ' ',
  "AAAAYNRB" varchar(4) NOT NULL default ' ',
  "AAAAYNYQ" varchar(1) NOT NULL default ' ',
  "AAAAYNRD" varchar(3) NOT NULL default '000',
  "AAAAYNYS" varchar(2) NOT NULL default ' ',
  "AAAAYNRF" varchar(4) NOT NULL default ' ',
  "AAAAYNYT" varchar(1) NOT NULL default ' ',
  "AAAAYNRH" varchar(2) NOT NULL default '00',
  "AAAAYNRI" varchar(4) NOT NULL default ' ',
  "AAAAYNZL" varchar(1) NOT NULL default ' ',
  "AAAAYNRK" varchar(3) NOT NULL default '000',
  "AAAAYNRL" varchar(4) NOT NULL default ' ',
  "AAAAYNVZ" varchar(1) NOT NULL default ' ',
  "AAAAYNXU" varchar(2) NOT NULL default ' ',
  "AAAAYNXV" varchar(2) NOT NULL default ' ',
  "AAAAYNXW" varchar(1) NOT NULL default ' ',
  "AAAAYNXX" varchar(1) NOT NULL default ' ',
  "AAAAYNXY" varchar(1) NOT NULL default ' ',
  "AAAAYNXZ" varchar(1) NOT NULL default ' ',
  "AAAAYNYA" varchar(1) NOT NULL default ' ',
  "AAAAYNYB" varchar(1) NOT NULL default ' ',
  "AAAAYNYC" varchar(1) NOT NULL default ' ',
  "AAAAYNYD" varchar(1) NOT NULL default ' ',
  "AAAAYNYE" varchar(1) NOT NULL default ' ',
  "AAAAYNYF" varchar(1) NOT NULL default ' ',
  "AAAAYNYG" varchar(1) NOT NULL default ' ',
  "AAAAYNYH" varchar(1) NOT NULL default ' ',
  "AAAAYNYI" varchar(1) NOT NULL default ' ',
  "AAAAYNYJ" varchar(1) NOT NULL default ' ',
  "AAAAYNSD" varchar(4) NOT NULL default ' ',
  "AAAAYNWD" varchar(1) NOT NULL default ' ',
  "AAAAYNSF" varchar(1) NOT NULL default '0',
  "AAAAYNSG" varchar(1) NOT NULL default '0',
  "AAAAYNSH" varchar(4) NOT NULL default ' ',
  "AAAAYNSI" varchar(2) NOT NULL default ' ',
  "AAAAYNSJ" varchar(2) NOT NULL default ' ',
  "AAAAYNSK" varchar(2) NOT NULL default ' ',
  "AAAAYNSL" varchar(2) NOT NULL default ' ',
  "AAAAYNSM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYOBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNST" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNXM" varchar(2) NOT NULL default '00',
  "AAAAYOBL" varchar(2) NOT NULL default '00',
  "AAAAYNXT" varchar(1) NOT NULL default ' ',
  "AAAAYNTD" varchar(3) NOT NULL default '000',
  "AAAAYNTE" varchar(4) NOT NULL default ' ',
  "AAAAYNXP" varchar(1) NOT NULL default ' ',
  "AAAAYNTG" varchar(3) NOT NULL default '000',
  "AAAAYNTH" varchar(3) NOT NULL default '000',
  "AAAAYNTI" varchar(3) NOT NULL default '000',
  "AAAAYNWR" varchar(2) NOT NULL default '00',
  "AAAAYNWS" varchar(4) NOT NULL default ' ',
  "AAAAYNXS" varchar(1) NOT NULL default ' ',
  "AAAAYNTM" decimal(6,3) NOT NULL default '0.000',
  "AAAAYNTN" decimal(6,3) NOT NULL default '0.000',
  "AAAAYQRO" varchar(4) NOT NULL default ' ',
  "AAAAYNTP" varchar(4) NOT NULL default ' ',
  "AAAAYNTQ" varchar(3) NOT NULL default '000',
  "AAAAYNTR" varchar(3) NOT NULL default '000',
  "AAAAYNTS" varchar(4) NOT NULL default ' ',
  "AAAAYNYL" varchar(1) NOT NULL default ' ',
  "AAAAYNTU" varchar(3) NOT NULL default '000',
  "AAAAYNTV" varchar(4) NOT NULL default ' ',
  "AAAAYNYQ" varchar(1) NOT NULL default ' ',
  "AAAAYNTX" varchar(3) NOT NULL default '000',
  "AAAAYNTY" varchar(4) NOT NULL default ' ',
  "AAAAYNYT" varchar(1) NOT NULL default ' ',
  "AAAAYNUA" varchar(3) NOT NULL default '000',
  "AAAAYNUB" varchar(4) NOT NULL default ' ',
  "AAAAYNZL" varchar(1) NOT NULL default ' ',
  "AAAAYNUD" varchar(3) NOT NULL default '000',
  "AAAAYNUE" varchar(4) NOT NULL default ' ',
  "AAAAYNVZ" varchar(1) NOT NULL default ' ',
  "AAAAYNUG" varchar(3) NOT NULL default '000',
  "AAAAYNUH" varchar(4) NOT NULL default ' ',
  "AAAAYNWD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNUO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYOBT" varchar(1) NOT NULL default ' ',
  "AAAAYOBU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOAL" varchar(3) NOT NULL default '000',
  "AAAAYOAM" varchar(3) NOT NULL default '000',
  "AAAAYNXO" varchar(2) NOT NULL default ' ',
  "AAAAYOAP" varchar(4) NOT NULL default ' ',
  "AAAAYNXP" varchar(1) NOT NULL default ' ',
  "AAAAYOAR" varchar(3) NOT NULL default '000',
  "AAAAYNXR" varchar(2) NOT NULL default ' ',
  "AAAAYOAU" varchar(4) NOT NULL default ' ',
  "AAAAYNXS" varchar(1) NOT NULL default ' ',
  "AAAAYOAW" varchar(3) NOT NULL default '000',
  "AAAAYNVH" varchar(2) NOT NULL default ' ',
  "AAAAYOAZ" varchar(4) NOT NULL default ' ',
  "AAAAYNYL" varchar(1) NOT NULL default ' ',
  "AAAAYOBB" varchar(3) NOT NULL default '000',
  "AAAAYNYP" varchar(2) NOT NULL default ' ',
  "AAAAYOBE" varchar(4) NOT NULL default ' ',
  "AAAAYNYQ" varchar(1) NOT NULL default ' ',
  "AAAAYOBG" varchar(3) NOT NULL default '000',
  "AAAAYNYS" varchar(2) NOT NULL default ' ',
  "AAAAYOBJ" varchar(4) NOT NULL default ' ',
  "AAAAYNYT" varchar(1) NOT NULL default ' ',
  "AAAAYNVS" varchar(3) NOT NULL default '000',
  "AAAAYNZP" varchar(2) NOT NULL default ' ',
  "AAAAYNZS" varchar(4) NOT NULL default ' ',
  "AAAAYNZL" varchar(1) NOT NULL default ' ',
  "AAAAYNVW" varchar(3) NOT NULL default '000',
  "AAAAYNZQ" varchar(2) NOT NULL default ' ',
  "AAAAYNZT" varchar(4) NOT NULL default ' ',
  "AAAAYNVZ" varchar(1) NOT NULL default ' ',
  "AAAAYNWA" varchar(3) NOT NULL default '000',
  "AAAAYNZR" varchar(2) NOT NULL default ' ',
  "AAAAYNZU" varchar(4) NOT NULL default ' ',
  "AAAAYNWD" varchar(1) NOT NULL default ' ',
  "AAAAYNWE" varchar(3) NOT NULL default '000',
  "AAAAYNWF" varchar(2) NOT NULL default ' ',
  "AAAAYNWG" varchar(4) NOT NULL default ' ',
  "AAAAYNWH" varchar(1) NOT NULL default ' ',
  "AAAAYNWI" varchar(3) NOT NULL default '000',
  "AAAAYNWJ" varchar(2) NOT NULL default ' ',
  "AAAAYNWK" varchar(4) NOT NULL default ' ',
  "AAAAYNWL" varchar(1) NOT NULL default ' ',
  "AAAAYNWM" varchar(4) NOT NULL default ' ',
  "AAAAYNWN" varchar(1) NOT NULL default ' ',
  "AAAAYNWO" varchar(4) NOT NULL default ' ',
  "AAAAYNWP" varchar(4) NOT NULL default ' ',
  "AAAAYNWQ" varchar(2) NOT NULL default '00',
  "AAAAYNWR" varchar(2) NOT NULL default '00',
  "AAAAYNWS" varchar(4) NOT NULL default ' ',
  "AAAAYNWT" varchar(1) NOT NULL default ' ',
  "AAAAYNWU" varchar(4) NOT NULL default ' ',
  "AAAAYQRO" varchar(4) NOT NULL default ' ',
  "AAAAYNXT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYOBT","AAAAYOBU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYNXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYOBT" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYNXM" varchar(2) NOT NULL default '00',
  "AAAAYNXN" varchar(4) NOT NULL default ' ',
  "AAAAYNXO" varchar(2) NOT NULL default ' ',
  "AAAAYNXP" varchar(1) NOT NULL default ' ',
  "AAAAYNXQ" varchar(4) NOT NULL default ' ',
  "AAAAYNXR" varchar(2) NOT NULL default ' ',
  "AAAAYNXS" varchar(1) NOT NULL default ' ',
  "AAAAYNXT" varchar(1) NOT NULL default ' ',
  "AAAAYNXU" varchar(2) NOT NULL default ' ',
  "AAAAYNXV" varchar(2) NOT NULL default ' ',
  "AAAAYNXW" varchar(1) NOT NULL default ' ',
  "AAAAYNXX" varchar(1) NOT NULL default ' ',
  "AAAAYNXY" varchar(1) NOT NULL default ' ',
  "AAAAYNXZ" varchar(1) NOT NULL default ' ',
  "AAAAYNYA" varchar(1) NOT NULL default ' ',
  "AAAAYNYB" varchar(1) NOT NULL default ' ',
  "AAAAYNYC" varchar(1) NOT NULL default ' ',
  "AAAAYNYD" varchar(1) NOT NULL default ' ',
  "AAAAYNYE" varchar(1) NOT NULL default ' ',
  "AAAAYNYF" varchar(1) NOT NULL default ' ',
  "AAAAYNYG" varchar(1) NOT NULL default ' ',
  "AAAAYNYH" varchar(1) NOT NULL default ' ',
  "AAAAYNYI" varchar(1) NOT NULL default ' ',
  "AAAAYNYJ" varchar(1) NOT NULL default ' ',
  "AAAAYNYK" varchar(4) NOT NULL default ' ',
  "AAAAYNYL" varchar(1) NOT NULL default ' ',
  "AAAAYNYM" varchar(2) NOT NULL default '00',
  "AAAAYNYN" varchar(3) NOT NULL default '000',
  "AAAAYNYO" varchar(4) NOT NULL default ' ',
  "AAAAYNYP" varchar(2) NOT NULL default ' ',
  "AAAAYNYQ" varchar(1) NOT NULL default ' ',
  "AAAAYNYR" varchar(4) NOT NULL default ' ',
  "AAAAYNYS" varchar(2) NOT NULL default ' ',
  "AAAAYNYT" varchar(1) NOT NULL default ' ',
  "AAAAYNYU" varchar(2) NOT NULL default ' ',
  "AAAAYNYV" varchar(2) NOT NULL default ' ',
  "AAAAYNYW" varchar(1) NOT NULL default ' ',
  "AAAAYNYX" varchar(1) NOT NULL default ' ',
  "AAAAYNYY" varchar(1) NOT NULL default ' ',
  "AAAAYNYZ" varchar(1) NOT NULL default ' ',
  "AAAAYNZA" varchar(1) NOT NULL default ' ',
  "AAAAYNZB" varchar(1) NOT NULL default ' ',
  "AAAAYNZC" varchar(1) NOT NULL default ' ',
  "AAAAYNZD" varchar(1) NOT NULL default ' ',
  "AAAAYNZE" varchar(1) NOT NULL default ' ',
  "AAAAYNZF" varchar(1) NOT NULL default ' ',
  "AAAAYNZG" varchar(1) NOT NULL default ' ',
  "AAAAYNZH" varchar(1) NOT NULL default ' ',
  "AAAAYNZI" varchar(1) NOT NULL default ' ',
  "AAAAYNZJ" varchar(1) NOT NULL default ' ',
  "AAAAYNZK" varchar(4) NOT NULL default ' ',
  "AAAAYNZL" varchar(1) NOT NULL default ' ',
  "AAAAYOAM" varchar(3) NOT NULL default '000',
  "AAAAYOAR" varchar(3) NOT NULL default '000',
  "AAAAYOAW" varchar(3) NOT NULL default '000',
  "AAAAYNZP" varchar(2) NOT NULL default ' ',
  "AAAAYNZQ" varchar(2) NOT NULL default ' ',
  "AAAAYNZR" varchar(2) NOT NULL default ' ',
  "AAAAYNZS" varchar(4) NOT NULL default ' ',
  "AAAAYNZT" varchar(4) NOT NULL default ' ',
  "AAAAYNZU" varchar(4) NOT NULL default ' ',
  "AAAAYQRO" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYOBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOAC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYOBT" varchar(1) NOT NULL default ' ',
  "AAAAYOBU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOAL" varchar(3) NOT NULL default '000',
  "AAAAYOAM" varchar(3) NOT NULL default '000',
  "AAAAYOAN" varchar(3) NOT NULL default '000',
  "AAAAYOAO" varchar(2) NOT NULL default ' ',
  "AAAAYOAP" varchar(4) NOT NULL default ' ',
  "AAAAYOAQ" varchar(1) NOT NULL default ' ',
  "AAAAYOAR" varchar(3) NOT NULL default '000',
  "AAAAYOAS" varchar(3) NOT NULL default '000',
  "AAAAYOAT" varchar(2) NOT NULL default ' ',
  "AAAAYOAU" varchar(4) NOT NULL default ' ',
  "AAAAYOAV" varchar(1) NOT NULL default ' ',
  "AAAAYOAW" varchar(3) NOT NULL default '000',
  "AAAAYOAX" varchar(3) NOT NULL default '000',
  "AAAAYOAY" varchar(2) NOT NULL default ' ',
  "AAAAYOAZ" varchar(4) NOT NULL default ' ',
  "AAAAYOBA" varchar(1) NOT NULL default ' ',
  "AAAAYOBB" varchar(3) NOT NULL default '000',
  "AAAAYOBC" varchar(3) NOT NULL default '000',
  "AAAAYOBD" varchar(2) NOT NULL default ' ',
  "AAAAYOBE" varchar(4) NOT NULL default ' ',
  "AAAAYOBF" varchar(1) NOT NULL default ' ',
  "AAAAYOBG" varchar(3) NOT NULL default '000',
  "AAAAYOBH" varchar(3) NOT NULL default '000',
  "AAAAYOBI" varchar(2) NOT NULL default ' ',
  "AAAAYOBJ" varchar(4) NOT NULL default ' ',
  "AAAAYOBK" varchar(1) NOT NULL default ' ',
  "AAAAYOBL" varchar(3) NOT NULL default '000',
  "AAAAYOBM" varchar(3) NOT NULL default '000',
  "AAAAYOBN" varchar(3) NOT NULL default '000',
  "AAAAYOBO" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAAAYOBT","AAAAYOBU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOBW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOGC" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQVI" varchar(4) NOT NULL default ' ',
  "AAAAYOEZ" varchar(4) NOT NULL default ' ',
  "AAAAYOCT" varchar(4) NOT NULL default ' ',
  "AAAAYOCE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYOGC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOCI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOCT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXD" varchar(5) NOT NULL default ' ',
  "AAAAYQVS" varchar(3) NOT NULL default ' ',
  "AAAAYOCQ" varchar(4) NOT NULL default ' ',
  "AAAAYQRG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYOCT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOCV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYODM" varchar(2) NOT NULL default '00',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYODV" varchar(4) NOT NULL default ' ',
  "AAAAYODE" varchar(4) NOT NULL default ' ',
  "AAAAYODF" decimal(6,4) NOT NULL default '0.0000',
  "AAAAYODG" varchar(4) NOT NULL default ' ',
  "AAAAYODY" decimal(6,4) NOT NULL default '0.0000',
  "AAAAYODZ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYODM","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYODP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPVC" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYODV" varchar(4) NOT NULL default ' ',
  "AAAAYODW" varchar(4) NOT NULL default ' ',
  "AAABLTUT" varchar(4) NOT NULL default ' ',
  "AAAAYODY" decimal(6,4) NOT NULL default '0.0000',
  "AAAAYODZ" varchar(4) NOT NULL default ' ',
  "AAAAYOEA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPVC","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOEZ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXD" varchar(5) NOT NULL default ' ',
  "AAAAYQVS" varchar(3) NOT NULL default ' ',
  "AAAAYOEN" decimal(9,2) NOT NULL default '0.00',
  "AAAAYOEO" varchar(1) NOT NULL default ' ',
  "AAAAYOEP" varchar(4) NOT NULL default ' ',
  "AAAAYOEQ" varchar(4) NOT NULL default ' ',
  "AAAAYOER" varchar(4) NOT NULL default ' ',
  "AAAAYOES" varchar(4) NOT NULL default ' ',
  "AAAAYOET" varchar(4) NOT NULL default ' ',
  "AAAAYOEU" varchar(4) NOT NULL default ' ',
  "AAAAYOEV" varchar(4) NOT NULL default ' ',
  "AAAAYOEW" varchar(4) NOT NULL default ' ',
  "AAAAYOEX" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYOEZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOFB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOFS" varchar(4) NOT NULL default ' ',
  "AAAAYOFT" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOFH" varchar(4) NOT NULL default ' ',
  "AAAAYOFI" varchar(4) NOT NULL default ' ',
  "AAAAYOFJ" varchar(4) NOT NULL default ' ',
  "AAAAYOFK" varchar(4) NOT NULL default ' ',
  "AAAAYOFL" varchar(4) NOT NULL default ' ',
  "AAAAYOFM" decimal(5,2) NOT NULL default '0.00',
  "AAAAYOFN" decimal(15,2) NOT NULL default '0.00',
  "AAAAYOFO" decimal(15,2) NOT NULL default '0.00',
  "AAAAYOFP" varchar(4) NOT NULL default ' ',
  "AAAAYOFQ" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYOFS","AAAAYOFT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYOGC" varchar(4) NOT NULL default ' ',
  "AAAAYOFZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYOGC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOGD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPVC" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAAAYOGI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPVC","AAABDTRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOGN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOMQ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOGS" varchar(2) NOT NULL default ' ',
  "AAAAYOGT" varchar(2) NOT NULL default ' ',
  "AAAAYOGU" varchar(2) NOT NULL default ' ',
  "AAAAYOGV" varchar(2) NOT NULL default ' ',
  "AAAAYOGW" varchar(2) NOT NULL default ' ',
  "AAAAYOGX" varchar(2) NOT NULL default ' ',
  "AAAAYOGY" varchar(2) NOT NULL default ' ',
  "AAAAYOGZ" varchar(2) NOT NULL default ' ',
  "AAAAYOHA" varchar(2) NOT NULL default ' ',
  "AAAAYOHB" varchar(2) NOT NULL default ' ',
  "AAAAYOHC" varchar(2) NOT NULL default ' ',
  "AAAAYOHD" varchar(2) NOT NULL default ' ',
  "AAAAYOHE" varchar(2) NOT NULL default ' ',
  "AAAAYOHF" varchar(2) NOT NULL default ' ',
  "AAAAYOHG" varchar(2) NOT NULL default ' ',
  "AAAAYOHH" varchar(2) NOT NULL default ' ',
  "AAAAYOHI" varchar(2) NOT NULL default ' ',
  "AAAAYOHJ" varchar(2) NOT NULL default ' ',
  "AAAAYOHK" varchar(2) NOT NULL default ' ',
  "AAAAYOHL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYOMQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCKE" varchar(8) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOHU" varchar(2) NOT NULL default '00',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABHIPZ" varchar(3) NOT NULL default ' ',
  "AAAAYOHX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCKE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCKE" varchar(8) NOT NULL default ' ',
  "AAAAYONA" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABBCKE","AAAAYONA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCKE" varchar(8) NOT NULL default ' ',
  "AAAAYONA" varchar(6) NOT NULL default ' ',
  "AAAAYOJM" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPXL" varchar(4) NOT NULL default ' ',
  "AAAAYOIT" varchar(4) NOT NULL default ' ',
  "AAAAYQPP" varchar(2) NOT NULL default ' ',
  "AAAAYPXO" varchar(4) NOT NULL default ' ',
  "AAAAYPXP" decimal(15,2) NOT NULL default '0.00',
  "AAAAYOIX" varchar(4) NOT NULL default ' ',
  "AAAAYPYI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCKE","AAAAYONA","AAAAYOJM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOJE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPVB" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCKE" varchar(8) NOT NULL default ' ',
  "AAAAYONA" varchar(6) NOT NULL default ' ',
  "AAAAYOJM" varchar(3) NOT NULL default ' ',
  "AAAAYOJN" varchar(4) NOT NULL default ' ',
  "AAAAYOJO" varchar(4) NOT NULL default ' ',
  "AAABBDYQ" varchar(4) NOT NULL default ' ',
  "AAAAYPYX" varchar(4) NOT NULL default ' ',
  "AAAAYQAF" varchar(4) NOT NULL default ' ',
  "AAAAYPYZ" varchar(4) NOT NULL default ' ',
  "AAAAYPZA" varchar(4) NOT NULL default ' ',
  "AAAAYPZB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPVB","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOJZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPVB" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBCKE" varchar(8) NOT NULL default ' ',
  "AAAAYONA" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPVB","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCKE" varchar(8) NOT NULL default ' ',
  "AAAAYONA" varchar(6) NOT NULL default ' ',
  "AAAAYQAF" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPZZ" varchar(4) NOT NULL default ' ',
  "AAAAYQAA" varchar(6) NOT NULL default ' ',
  "AAAAYOKU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCKE","AAAAYONA","AAAAYQAF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQAA" varchar(6) NOT NULL default ' ',
  "AAAAYPFZ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOLG" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAYQAA","AAAAYPFZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOLL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYOLS" varchar(1) NOT NULL default '0',
  "AAABJGBR" varchar(42) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYOLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOMQ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYOMQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYONQ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYONQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYOMQ" varchar(2) NOT NULL default ' ',
  "AAABMDXM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYOMQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBCKE" varchar(8) NOT NULL default ' ',
  "AAAAYONA" varchar(6) NOT NULL default ' ',
  "AAAAYOMW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBCKE","AAAAYONA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYONB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBCKE" varchar(8) NOT NULL default ' ',
  "AAAAYONF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBCKE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYONJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYONQ" varchar(4) NOT NULL default ' ',
  "AAAAYONN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYONQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYONR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPVB" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAAAYONW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPVB","AAABDTRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYOOF" varchar(3) NOT NULL default '000',
  "AAAAYOOG" varchar(3) NOT NULL default '000',
  "AAAAYOOH" varchar(3) NOT NULL default '000',
  "AAAAYOOI" varchar(2) NOT NULL default '00',
  "AAAAYOOJ" varchar(3) NOT NULL default '000',
  "AAAAYOOK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYRCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOOO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPCE" varchar(30) NOT NULL default ' ',
  "AAAAYOPA" varchar(3) NOT NULL default '000',
  "AAAAYPCK" varchar(10) NOT NULL default ' ',
  "AAAAYPCI" varchar(4) NOT NULL default ' ',
  "AAAAYOOU" varchar(10) NOT NULL default ' ',
  "AAAAYPNN" varchar(2) NOT NULL default ' ',
  "AAAAYOOW" varchar(16) NOT NULL default ' ',
  "AAAAYOOX" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPCE","AAAAYOPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOPB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOQH" varchar(4) NOT NULL default '0000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABDTRC" varchar(3) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYOPI" varchar(1) NOT NULL default ' ',
  "AAAAZEMR" varchar(1) NOT NULL default ' ',
  "AAAAYOYU" varchar(10) NOT NULL default ' ',
  "AAAAYPEJ" varchar(1) NOT NULL default ' ',
  "AAAAYOYX" varchar(2) NOT NULL default ' ',
  "AAAAYOYY" varchar(2) NOT NULL default ' ',
  "AAAAYOPO" varchar(2) NOT NULL default '00',
  "AAABAKLE" decimal(15,2) NOT NULL default '0.00',
  "AAAAYOPQ" varchar(2) NOT NULL default '00',
  "AAAAYOPR" varchar(2) NOT NULL default '00',
  "AAAAYOPS" varchar(2) NOT NULL default '00',
  "AAAAYOPT" varchar(2) NOT NULL default '00',
  "AAAAYOPU" varchar(2) NOT NULL default '00',
  "AAAAYOPV" varchar(2) NOT NULL default '00',
  "AAAAYOPW" varchar(2) NOT NULL default '00',
  "AAAAYOPX" varchar(2) NOT NULL default '00',
  "AAAAYOPY" varchar(2) NOT NULL default '00',
  "AAAAYOPZ" varchar(2) NOT NULL default '00',
  "AAAAYOQA" varchar(3) NOT NULL default '000',
  "AAAAYOZS" varchar(2) NOT NULL default ' ',
  "AAAAYOQC" decimal(15,2) NOT NULL default '0.00',
  "AAAAYOQD" varchar(3) NOT NULL default '000',
  "AAAAYOQE" varchar(3) NOT NULL default '000',
  "AAAAYOQF" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYOQH","AAABMCRV","AAABDTRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOQK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYOZS" varchar(2) NOT NULL default ' ',
  "AAAAYOQO" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYOZS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOQS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAAAYOVF" varchar(4) NOT NULL default ' ',
  "AAAAYOQW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYORS" varchar(10) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAAAYORE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYORS","AAABDTRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYORJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYORS" varchar(10) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAAAYORN" varchar(3) NOT NULL default '000',
  "AAAAYORO" varchar(3) NOT NULL default '000',
  "AAAAYORP" varchar(3) NOT NULL default '000',
  "AAAAYORQ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAYORS","AAABDTRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYORU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOYU" varchar(10) NOT NULL default ' ',
  "AAAAYOSL" varchar(10) NOT NULL default ' ',
  "AAAAYOSM" varchar(25) NOT NULL default ' ',
  "AAAAYPAO" varchar(8) NOT NULL default '00000000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYPDT" varchar(5) NOT NULL default ' ',
  "AAAAYPAF" varchar(8) NOT NULL default '00000000',
  "AAABKCJR" decimal(15,2) NOT NULL default '0.00',
  "AAAAYPAH" decimal(9,0) NOT NULL default '0',
  "AAAAYPAI" decimal(9,0) NOT NULL default '0',
  "AAAAYPAJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAYPAK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYOYU","AAAAYOSL","AAAAYOSM","AAAAYPAO","AAAAYPES","AAAAYRCV","AAAAYPDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPLA" varchar(4) NOT NULL default ' ',
  "AAAAYOTD" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOYU" varchar(10) NOT NULL default ' ',
  "AAAAYOWC" varchar(2) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYOTA" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPLA","AAAAYOTD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOTF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPLA" varchar(4) NOT NULL default ' ',
  "AAAAYOYU" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOTL" varchar(5) NOT NULL default ' ',
  "AAAAYOTM" varchar(18) NOT NULL default ' ',
  "AAAAYOTN" varchar(18) NOT NULL default ' ',
  "AAAAYOTO" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYPLA","AAAAYOYU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOTT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPLA" varchar(4) NOT NULL default ' ',
  "AAAAYOUC" varchar(7) NOT NULL default ' ',
  "AAAAYOVZ" varchar(15) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYRBA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYPLA","AAAAYOUC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOUD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOWC" varchar(2) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYOWC","AAABMQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPLA" varchar(4) NOT NULL default ' ',
  "AAAAYOUT" varchar(4) NOT NULL default ' ',
  "AAABARAV" varchar(35) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPLA","AAAAYOUT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOUU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABAOPS" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPEJ" varchar(1) NOT NULL default ' ',
  "AAAAYOYX" varchar(2) NOT NULL default ' ',
  "AAAAYOYY" varchar(2) NOT NULL default ' ',
  "AAAAYOVF" varchar(4) NOT NULL default ' ',
  "AAAAYOVG" varchar(3) NOT NULL default '000',
  "AAAAYOVH" varchar(1) NOT NULL default ' ',
  "AAAAYOVI" varchar(1) NOT NULL default ' ',
  "AAAAZDHM" varchar(1) NOT NULL default ' ',
  "AAAAYOVK" varchar(1) NOT NULL default ' ',
  "AAAAYOVL" varchar(10) NOT NULL default '0000000000',
  "AAAAYOVM" varchar(2) NOT NULL default ' ',
  "AAAAYOVN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABAOPS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYOWC" varchar(2) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYOVZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYOWC","AAABMQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYPDT" varchar(5) NOT NULL default ' ',
  "AAAAYPBJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOWM" varchar(6) NOT NULL default ' ',
  "AAAAYOWN" varchar(4) NOT NULL default ' ',
  "AAAAYOWO" varchar(5) NOT NULL default ' ',
  "AAABBQFF" varchar(5) NOT NULL default ' ',
  "AAAAYOWQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYRCV","AAAAYPDT","AAAAYPBJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAAAYOZS" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOXE" varchar(1) NOT NULL default ' ',
  "AAAAYOXF" varchar(4) NOT NULL default ' ',
  "AAAAYOXG" varchar(4) NOT NULL default ' ',
  "AAAAYOXH" varchar(4) NOT NULL default ' ',
  "AAAAYOXI" varchar(4) NOT NULL default ' ',
  "AAAAYOXJ" varchar(4) NOT NULL default ' ',
  "AAAAYOXK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEOP","AAAAYOZS","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAZLWW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYOYU" varchar(10) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAYPEJ" varchar(1) NOT NULL default ' ',
  "AAAAYOYX" varchar(2) NOT NULL default ' ',
  "AAAAYOYY" varchar(2) NOT NULL default ' ',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYPDT" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYOYK" varchar(8) NOT NULL default '00000000',
  "AAAAYOYL" varchar(8) NOT NULL default '00000000',
  "AAAAYOYM" decimal(15,2) NOT NULL default '0.00',
  "AAAAYOYN" decimal(15,2) NOT NULL default '0.00',
  "AAAAYOYO" varchar(6) NOT NULL default '000000',
  "AAAAYOYP" varchar(6) NOT NULL default '000000',
  "AAAAYOYQ" varchar(6) NOT NULL default '000000',
  "AAAAYOYR" varchar(6) NOT NULL default '000000',
  "AAAAYOYS" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYOYU","AAABMCRV","AAAAYPEJ","AAAAYOYX","AAAAYOYY","AAAAYPES","AAAAYRCV","AAAAYPDT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYOZG" varchar(3) NOT NULL default '000',
  "AAAAYOZH" varchar(3) NOT NULL default ' ',
  "AAAAZZVO" varchar(20) NOT NULL default ' ',
  "AAABMJWN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPES")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYPDT" varchar(5) NOT NULL default ' ',
  "AAAAYPCI" varchar(4) NOT NULL default ' ',
  "AAAAYOZS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYRCV","AAAAYPDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYOZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPLA" varchar(4) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAYPAO" varchar(8) NOT NULL default '00000000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYPDT" varchar(5) NOT NULL default ' ',
  "AAAAYPAF" varchar(8) NOT NULL default '00000000',
  "AAABKCJR" decimal(15,2) NOT NULL default '0.00',
  "AAAAYPAH" decimal(9,0) NOT NULL default '0',
  "AAAAYPAI" decimal(9,0) NOT NULL default '0',
  "AAAAYPAJ" decimal(15,2) NOT NULL default '0.00',
  "AAAAYPAK" varchar(50) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPLA","AAABMCRV","AAAAYPAO","AAAAYPES","AAAAYRCV","AAAAYPDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPAS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYPDT" varchar(5) NOT NULL default ' ',
  "AAAAYPBJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPBA" varchar(2) NOT NULL default '00',
  "AAAAYPBB" varchar(3) NOT NULL default '000',
  "AAABFBBL" varchar(3) NOT NULL default '000',
  "AAABDRYY" varchar(2) NOT NULL default ' ',
  "AAABGGED" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYRCV","AAAAYPDT","AAAAYPBJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPBL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYRCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPBS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYPDT" varchar(5) NOT NULL default ' ',
  "AAAAYPCI" varchar(4) NOT NULL default ' ',
  "AAAAYPCJ" varchar(2) NOT NULL default ' ',
  "AAAAYPCK" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQZS" varchar(1) NOT NULL default ' ',
  "AAAAYQZZ" varchar(1) NOT NULL default ' ',
  "AAAAYPCD" varchar(1) NOT NULL default ' ',
  "AAAAYPCE" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYPDT","AAAAYPCI","AAAAYPCJ","AAAAYPCK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPCM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYRBA" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPCS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYPDT" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPES","AAAAYPDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPCZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPEJ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPDE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAAAYPDH" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGLBU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPDK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYPDT" varchar(5) NOT NULL default ' ',
  "AAAAYPDP" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPES","AAAAYPDT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPDU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYPDY" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMQNB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPEC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPEJ" varchar(1) NOT NULL default ' ',
  "AAAAYPEG" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPEJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPEK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPES" varchar(10) NOT NULL default ' ',
  "AAAAYRCV" varchar(2) NOT NULL default ' ',
  "AAAAYPEP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPES","AAAAYRCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPLI" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYPLI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPFC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYYPM" varchar(4) NOT NULL default ' ',
  "AAAAYPFP" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPFI" varchar(4) NOT NULL default ' ',
  "AAAAYPFJ" varchar(4) NOT NULL default ' ',
  "AAAAYPFK" varchar(4) NOT NULL default ' ',
  "AAAAYPYI" varchar(4) NOT NULL default ' ',
  "AAAAYPFY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYYPM","AAAAYPFP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPFY" varchar(4) NOT NULL default ' ',
  "AAAAYPFZ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYPFY","AAAAYPFZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYYPM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPGG" varchar(4) NOT NULL default ' ',
  "AAAAYPGH" varchar(4) NOT NULL default ' ',
  "AAAAYPGI" varchar(4) NOT NULL default ' ',
  "AAAAYPGJ" varchar(4) NOT NULL default ' ',
  "AAAAYPGK" varchar(4) NOT NULL default ' ',
  "AAAAYPGL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYYPM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPGP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNHW" varchar(3) NOT NULL default ' ',
  "AAAAYPLS" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAZNHW","AAAAYPLS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPGZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNHW" varchar(3) NOT NULL default ' ',
  "AAAAYPLS" varchar(5) NOT NULL default ' ',
  "AAAAYPIA" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPXL" varchar(4) NOT NULL default ' ',
  "AAAAYPHH" varchar(4) NOT NULL default ' ',
  "AAAAYQPP" varchar(2) NOT NULL default ' ',
  "AAAAYPXO" varchar(4) NOT NULL default ' ',
  "AAAAYPXP" decimal(15,2) NOT NULL default '0.00',
  "AAAAYPHL" varchar(4) NOT NULL default ' ',
  "AAAAYPYI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZNHW","AAAAYPLS","AAAAYPIA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPVA" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZNHW" varchar(3) NOT NULL default ' ',
  "AAAAYPLS" varchar(5) NOT NULL default ' ',
  "AAAAYPIA" varchar(3) NOT NULL default ' ',
  "AAAAYPIB" varchar(4) NOT NULL default ' ',
  "AAAAYPIC" varchar(4) NOT NULL default ' ',
  "AAAAYPID" varchar(4) NOT NULL default ' ',
  "AAAAYPYX" varchar(4) NOT NULL default ' ',
  "AAAAYQAF" varchar(4) NOT NULL default ' ',
  "AAAAYPYZ" varchar(4) NOT NULL default ' ',
  "AAAAYPZA" varchar(4) NOT NULL default ' ',
  "AAAAYPZB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPVA","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPIN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPVA" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZNHW" varchar(3) NOT NULL default ' ',
  "AAAAYPLS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPVA","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPIZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZNHW" varchar(3) NOT NULL default ' ',
  "AAAAYPLS" varchar(5) NOT NULL default ' ',
  "AAAAYQAF" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPZZ" varchar(4) NOT NULL default ' ',
  "AAAAYQAA" varchar(6) NOT NULL default ' ',
  "AAAAYPJI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZNHW","AAAAYPLS","AAAAYQAF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPJO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPJY" varchar(4) NOT NULL default ' ',
  "AAAAYPJZ" varchar(4) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZXSE" varchar(4) NOT NULL default ' ',
  "AAABFLSG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPJY","AAAAYPJZ","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPKC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPKS" varchar(16) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPKH" varchar(60) NOT NULL default ' ',
  "AAAAYPKI" varchar(5) NOT NULL default '00000',
  "AAAAYPKJ" varchar(25) NOT NULL default ' ',
  "AAAAZNHW" varchar(3) NOT NULL default ' ',
  "AAAAYPKL" varchar(10) NOT NULL default ' ',
  "AAAAYPKM" varchar(35) NOT NULL default ' ',
  "AAABAPGQ" varchar(14) NOT NULL default ' ',
  "AAAAYPKO" varchar(25) NOT NULL default ' ',
  "AAABLSBY" varchar(8) NOT NULL default '00000000',
  "AAAAZNXD" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYPKS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPLA" varchar(4) NOT NULL default ' ',
  "AAAAYPKX" varchar(25) NOT NULL default ' ',
  "AAABCAKN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPLA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPLI" varchar(4) NOT NULL default ' ',
  "AAAAYPLF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPLI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZNHW" varchar(3) NOT NULL default ' ',
  "AAAAYPLS" varchar(5) NOT NULL default ' ',
  "AAAAYPLO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAZNHW","AAAAYPLS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPLT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPVA" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAAAYPLY" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPVA","AAABDTRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPMD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQPP" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPMI" varchar(1) NOT NULL default ' ',
  "AAAAYPMJ" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYQPP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPNA" varchar(4) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABMCTG" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQQA" varchar(4) NOT NULL default ' ',
  "AAAAYPMW" varchar(4) NOT NULL default ' ',
  "AAAAYPMX" varchar(2) NOT NULL default ' ',
  "AAAAYPMY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPNA","AAABBEON","AAABBEOO","AAABMCTG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPNQ" varchar(4) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPNL" varchar(4) NOT NULL default ' ',
  "AAAAYPNM" varchar(5) NOT NULL default ' ',
  "AAAAYPNN" varchar(2) NOT NULL default ' ',
  "AAAAYPNO" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYPNQ","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPQI" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYPQI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPWB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYPWB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPOJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPQI" varchar(10) NOT NULL default ' ',
  "AAAAYPWB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAAAYPTJ" varchar(5) NOT NULL default ' ',
  "AAAAYPTP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPQI","AAAAYPWB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPOW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPPE" varchar(4) NOT NULL default ' ',
  "AAAAYPQI" varchar(10) NOT NULL default ' ',
  "AAAAYPWB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYPPE","AAAAYPQI","AAAAYPWB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPPQ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPPQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPPS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPWJ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZYCU" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAAAYPWJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPQB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPQI" varchar(10) NOT NULL default ' ',
  "AAAAYPQF" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPQI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPQJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(4) NOT NULL default '0000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPQT" varchar(4) NOT NULL default ' ',
  "AAAAYPQU" varchar(4) NOT NULL default ' ',
  "AAABILQC" varchar(30) NOT NULL default ' ',
  "AAAAYPQW" varchar(1) NOT NULL default ' ',
  "AAAAYPQX" varchar(2) NOT NULL default ' ',
  "AAAAYZTE" varchar(25) NOT NULL default ' ',
  "AAAAYPQZ" varchar(4) NOT NULL default ' ',
  "AAAAYPRA" varchar(4) NOT NULL default ' ',
  "AAABILQD" varchar(30) NOT NULL default ' ',
  "AAAAYPRC" varchar(1) NOT NULL default ' ',
  "AAAAYPRD" varchar(2) NOT NULL default ' ',
  "AAAAYZTF" varchar(25) NOT NULL default ' ',
  "AAAAYPRF" varchar(4) NOT NULL default ' ',
  "AAAAYPRG" varchar(4) NOT NULL default ' ',
  "AAABILQE" varchar(30) NOT NULL default ' ',
  "AAAAYPRI" varchar(1) NOT NULL default ' ',
  "AAAAYPRJ" varchar(2) NOT NULL default ' ',
  "AAAAYPRK" varchar(25) NOT NULL default ' ',
  "AAAAYPRL" varchar(4) NOT NULL default ' ',
  "AAAAYPRM" varchar(4) NOT NULL default ' ',
  "AAABILQF" varchar(30) NOT NULL default ' ',
  "AAAAYPRO" varchar(1) NOT NULL default ' ',
  "AAAAYPRP" varchar(2) NOT NULL default ' ',
  "AAAAYPRQ" varchar(25) NOT NULL default ' ',
  "AAAAYPRR" varchar(4) NOT NULL default ' ',
  "AAAAYPRS" varchar(4) NOT NULL default ' ',
  "AAABILQG" varchar(30) NOT NULL default ' ',
  "AAAAYPRU" varchar(1) NOT NULL default ' ',
  "AAAAYPRV" varchar(2) NOT NULL default ' ',
  "AAAAYPRW" varchar(25) NOT NULL default ' ',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAABAQKE" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAABLUVE","AAABBEON","AAABBEOO","AAABBEOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPSO" varchar(2) NOT NULL default '00',
  "AAAAYPSP" varchar(2) NOT NULL default '00',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYPTJ" varchar(5) NOT NULL default ' ',
  "AAAAYPUC" varchar(10) NOT NULL default ' ',
  "AAAAYPTR" varchar(30) NOT NULL default ' ',
  "AAAAYPTP" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYQXL","AAAAYPTJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPUC" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPTP" varchar(1) NOT NULL default ' ',
  "AAAAYPTQ" varchar(5) NOT NULL default '00000',
  "AAAAYPTR" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPUC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPTV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPUC" varchar(10) NOT NULL default ' ',
  "AAAAYPUN" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYPUC","AAAAYPUN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPUN" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  "AAAAYPUL" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYPUN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPVT" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPUW" varchar(2) NOT NULL default ' ',
  "AAAAYPUX" varchar(2) NOT NULL default ' ',
  "AAAAYPUY" varchar(2) NOT NULL default ' ',
  "AAAAYQBZ" varchar(2) NOT NULL default ' ',
  "AAAAYPVA" varchar(2) NOT NULL default ' ',
  "AAAAYPVB" varchar(2) NOT NULL default ' ',
  "AAAAYPVC" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAAAYPVT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAAAYPVT" varchar(2) NOT NULL default ' ',
  "AAAAYPVO" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBEON","AAABBEOO","AAAAYPVT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPWB" varchar(4) NOT NULL default ' ',
  "AAAAYPVY" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYPWJ" varchar(1) NOT NULL default ' ',
  "AAAAZEEC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYPWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQBH" varchar(6) NOT NULL default ' ',
  "AAAAYQBI" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYQBH","AAAAYQBI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYQAY" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABMQNB","AAAAYQAY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYQAY" varchar(5) NOT NULL default ' ',
  "AAABAZTX" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPXL" varchar(4) NOT NULL default ' ',
  "AAAAYPXM" varchar(4) NOT NULL default ' ',
  "AAAAYQPP" varchar(2) NOT NULL default ' ',
  "AAAAYPXO" varchar(4) NOT NULL default ' ',
  "AAAAYPXP" decimal(15,2) NOT NULL default '0.00',
  "AAAAYPXQ" varchar(4) NOT NULL default ' ',
  "AAAAYPYI" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQNB","AAAAYQAY","AAABAZTX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYPYI" varchar(4) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPYD" decimal(15,2) NOT NULL default '0.00',
  "AAABKCIB" varchar(1) NOT NULL default ' ',
  "AAAAYPYF" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYPYG" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYPYI","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQBZ" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYQAY" varchar(5) NOT NULL default ' ',
  "AAABAZTX" varchar(3) NOT NULL default ' ',
  "AAAAYPYU" varchar(4) NOT NULL default ' ',
  "AAAAYPYV" varchar(4) NOT NULL default ' ',
  "AAAAYPYW" varchar(4) NOT NULL default ' ',
  "AAAAYPYX" varchar(4) NOT NULL default ' ',
  "AAAAYQAF" varchar(4) NOT NULL default ' ',
  "AAAAYPYZ" varchar(4) NOT NULL default ' ',
  "AAAAYPZA" varchar(4) NOT NULL default ' ',
  "AAAAYPZB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYQBZ","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQBZ" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYQAY" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYQBZ","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYPZS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYQAY" varchar(5) NOT NULL default ' ',
  "AAAAYQAF" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYPZZ" varchar(4) NOT NULL default ' ',
  "AAAAYQAA" varchar(6) NOT NULL default ' ',
  "AAAAYQAB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQNB","AAAAYQAY","AAAAYQAF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQBQ" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYQBQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQAP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAAAYQAY" varchar(5) NOT NULL default ' ',
  "AAAAYQAU" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMQNB","AAAAYQAY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQAZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYQBH" varchar(6) NOT NULL default ' ',
  "AAAAYQBI" varchar(6) NOT NULL default '000000',
  "AAAAYQBE" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYQBH","AAAAYQBI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQBJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYQBQ" varchar(6) NOT NULL default ' ',
  "AAAAYQBN" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYQBQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYQBZ" varchar(2) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAAAYQBW" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYQBZ","AAABDTRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQCB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAAAYQNX" varchar(2) NOT NULL default '00',
  "AAAAYQKM" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYYHV" decimal(5,2) NOT NULL default '0.00',
  "AAAAYQGH" decimal(5,2) NOT NULL default '0.00',
  "AAAAYQKZ" varchar(3) NOT NULL default '000',
  "AAABFLSE" varchar(1) NOT NULL default ' ',
  "AAAAYQGL" varchar(4) NOT NULL default ' ',
  "AAABFLSF" varchar(1) NOT NULL default ' ',
  "AAAAYQGN" varchar(4) NOT NULL default ' ',
  "AAAAYQGO" varchar(1) NOT NULL default ' ',
  "AAAAYQGP" varchar(4) NOT NULL default ' ',
  "AAAAYQGQ" varchar(1) NOT NULL default ' ',
  "AAAAYQGR" varchar(4) NOT NULL default ' ',
  "AAAAYQGS" varchar(1) NOT NULL default ' ',
  "AAAAYQGT" varchar(4) NOT NULL default ' ',
  "AAAAYQKA" varchar(2) NOT NULL default '00',
  "AAAAYQKB" varchar(3) NOT NULL default '000',
  "AAAAYQKC" varchar(2) NOT NULL default '00',
  "AAAAYQKD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAAAYQNX","AAAAYQKM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQDN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAAAYQNX" varchar(2) NOT NULL default '00',
  "AAAAYQKM" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYQJX" varchar(2) NOT NULL default '00',
  "AAAAYQKZ" varchar(3) NOT NULL default '000',
  "AAAAYQJZ" decimal(12,2) NOT NULL default '0.00',
  "AAABAOXW" decimal(12,2) NOT NULL default '0.00',
  "AAAAYQHB" varchar(1) NOT NULL default ' ',
  "AAAAYQEE" varchar(1) NOT NULL default ' ',
  "AAAAYQEF" varchar(1) NOT NULL default ' ',
  "AAAAYQKA" varchar(2) NOT NULL default '00',
  "AAAAYQKB" varchar(3) NOT NULL default '000',
  "AAAAYQKC" varchar(2) NOT NULL default '00',
  "AAAAYQKD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAAAYQNX","AAAAYQKM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQET" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAAAYQNX" varchar(2) NOT NULL default '00',
  "AAAAYQKM" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYQJX" decimal(4,2) NOT NULL default '0.00',
  "AAAAYQKZ" varchar(3) NOT NULL default '000',
  "AAAAYQJZ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYQKA" varchar(2) NOT NULL default '00',
  "AAAAYQKB" varchar(3) NOT NULL default '000',
  "AAAAYQKC" varchar(2) NOT NULL default '00',
  "AAAAYQKD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAAAYQNX","AAAAYQKM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQFV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAAAYQNX" varchar(2) NOT NULL default '00',
  "AAAAYQKM" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYQGH" decimal(5,2) NOT NULL default '0.00',
  "AAAAYQJX" varchar(2) NOT NULL default '00',
  "AAAAYQKZ" varchar(3) NOT NULL default '000',
  "AAABFLSE" varchar(1) NOT NULL default ' ',
  "AAAAYQGL" varchar(4) NOT NULL default ' ',
  "AAABFLSF" varchar(1) NOT NULL default ' ',
  "AAAAYQGN" varchar(4) NOT NULL default ' ',
  "AAAAYQGO" varchar(1) NOT NULL default ' ',
  "AAAAYQGP" varchar(4) NOT NULL default ' ',
  "AAAAYQGQ" varchar(1) NOT NULL default ' ',
  "AAAAYQGR" varchar(4) NOT NULL default ' ',
  "AAAAYQGS" varchar(1) NOT NULL default ' ',
  "AAAAYQGT" varchar(4) NOT NULL default ' ',
  "AAAAZIEB" decimal(12,2) NOT NULL default '0.00',
  "AAAAYQGV" varchar(2) NOT NULL default '00',
  "AAAAYQGW" varchar(4) NOT NULL default ' ',
  "AAAAYQKA" varchar(2) NOT NULL default '00',
  "AAAAYQKB" varchar(3) NOT NULL default '000',
  "AAAAYQKC" varchar(2) NOT NULL default '00',
  "AAAAYQKD" varchar(2) NOT NULL default ' ',
  "AAAAYQHB" varchar(1) NOT NULL default ' ',
  "AAAAYQKE" varchar(4) NOT NULL default ' ',
  "AAAAYQHD" varchar(8) NOT NULL default ' ',
  "AAAAYQHE" varchar(2) NOT NULL default ' ',
  "AAAAYQHF" varchar(8) NOT NULL default ' ',
  "AAAAYQHG" varchar(2) NOT NULL default ' ',
  "AAAAYQHH" varchar(8) NOT NULL default ' ',
  "AAAAYQHI" varchar(2) NOT NULL default ' ',
  "AAAAYQHJ" varchar(8) NOT NULL default ' ',
  "AAAAYQHK" varchar(2) NOT NULL default ' ',
  "AAAAYQHL" varchar(8) NOT NULL default ' ',
  "AAAAYQHM" varchar(2) NOT NULL default ' ',
  "AAAAYQHN" varchar(8) NOT NULL default ' ',
  "AAAAYQHO" varchar(2) NOT NULL default ' ',
  "AAAAYQHP" varchar(8) NOT NULL default ' ',
  "AAAAYQHQ" varchar(2) NOT NULL default ' ',
  "AAAAYQHR" varchar(8) NOT NULL default ' ',
  "AAAAYQHS" varchar(2) NOT NULL default ' ',
  "AAAAYQHT" varchar(8) NOT NULL default ' ',
  "AAAAYQHU" varchar(2) NOT NULL default ' ',
  "AAAAYQHV" varchar(8) NOT NULL default ' ',
  "AAAAYQHW" varchar(2) NOT NULL default ' ',
  "AAAAYQHX" varchar(8) NOT NULL default ' ',
  "AAAAYQHY" varchar(2) NOT NULL default ' ',
  "AAAAYQHZ" varchar(8) NOT NULL default ' ',
  "AAAAYQIA" varchar(2) NOT NULL default ' ',
  "AAAAYQIB" varchar(8) NOT NULL default ' ',
  "AAAAYQIC" varchar(2) NOT NULL default ' ',
  "AAAAYQID" varchar(8) NOT NULL default ' ',
  "AAAAYQIE" varchar(2) NOT NULL default ' ',
  "AAAAYQIF" decimal(5,2) NOT NULL default '0.00',
  "AAAAYQIG" varchar(1) NOT NULL default ' ',
  "AAAAYQIH" decimal(12,2) NOT NULL default '0.00',
  "AAAAYQII" varchar(4) NOT NULL default ' ',
  "AAAAYQIJ" varchar(1) NOT NULL default ' ',
  "AAAAYQIK" varchar(4) NOT NULL default ' ',
  "AAAAYQIL" varchar(4) NOT NULL default ' ',
  "AAAAYQIM" varchar(4) NOT NULL default ' ',
  "AAAAYQIN" varchar(4) NOT NULL default ' ',
  "AAAAYQIO" varchar(1) NOT NULL default ' ',
  "AAAAYQIP" varchar(1) NOT NULL default ' ',
  "AAAAYQIQ" varchar(1) NOT NULL default ' ',
  "AAAAYQIR" varchar(1) NOT NULL default ' ',
  "AAAAYQIS" varchar(1) NOT NULL default ' ',
  "AAAAYQIT" varchar(1) NOT NULL default ' ',
  "AAAAYQIU" varchar(1) NOT NULL default ' ',
  "AAAAYQIV" varchar(1) NOT NULL default ' ',
  "AAAAYQIW" varchar(1) NOT NULL default ' ',
  "AAAAYQIX" varchar(1) NOT NULL default ' ',
  "AAAAYQIY" varchar(1) NOT NULL default ' ',
  "AAAAYQIZ" varchar(1) NOT NULL default ' ',
  "AAAAYQJA" varchar(1) NOT NULL default ' ',
  "AAAAYQJB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAAAYQNX","AAAAYQKM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQJL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAAAYQNX" varchar(2) NOT NULL default '00',
  "AAAAYQKM" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYQJX" varchar(2) NOT NULL default '00',
  "AAAAYQKZ" varchar(3) NOT NULL default '000',
  "AAAAYQJZ" decimal(12,2) NOT NULL default '0.00',
  "AAAAYQKA" varchar(2) NOT NULL default '00',
  "AAAAYQKB" varchar(3) NOT NULL default '000',
  "AAAAYQKC" varchar(2) NOT NULL default '00',
  "AAAAYQKD" varchar(2) NOT NULL default ' ',
  "AAAAYQKE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAAAYQNX","AAAAYQKM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAAAYQNX" varchar(2) NOT NULL default '00',
  "AAAAYQLK" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQKZ" varchar(3) NOT NULL default '000',
  "AAAAYQLA" varchar(8) NOT NULL default ' ',
  "AAAAYQLB" varchar(2) NOT NULL default ' ',
  "AAAAYQLC" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAAAYQNX","AAAAYQLK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYQLW" decimal(12,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQME" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQMP" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQMJ" varchar(2) NOT NULL default '00',
  "AAAAYQMK" varchar(2) NOT NULL default '00',
  "AAAAYQML" varchar(2) NOT NULL default '00',
  "AAAAYQMM" varchar(1) NOT NULL default ' ',
  "AAAAYQMN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYQMP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQNP" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQNX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYQNP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQNA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQNX" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYQNX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQNI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYQNP" varchar(3) NOT NULL default ' ',
  "AAAAYQNM" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYQNP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQNQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYQNX" varchar(2) NOT NULL default '00',
  "AAABBAHF" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYQNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQNY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXD" varchar(5) NOT NULL default ' ',
  "AAAAYQVS" varchar(3) NOT NULL default ' ',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQOG" decimal(15,2) NOT NULL default '0.00',
  "AAAAYQOH" decimal(15,2) NOT NULL default '0.00',
  "AAABKCIB" varchar(1) NOT NULL default ' ',
  "AAAAYQOJ" decimal(7,4) NOT NULL default '0.0000',
  "AAAAYQOK" decimal(15,2) NOT NULL default '0.00',
  "AAAAYQOL" decimal(15,2) NOT NULL default '0.00',
  "AAAAYQOM" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYQXL","AAAAYQXD","AAAAYQVS","AAABDTRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQPD" varchar(2) NOT NULL default '00',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXD" varchar(5) NOT NULL default ' ',
  "AAAAYQVS" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQPB" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYQPD","AAAAYQXL","AAAAYQXD","AAAAYQVS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQPI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXD" varchar(5) NOT NULL default ' ',
  "AAAAYQVS" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQPP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYQXL","AAAAYQXD","AAAAYQVS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQPV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQUN" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQQA" varchar(4) NOT NULL default ' ',
  "AAAAYQQB" varchar(4) NOT NULL default ' ',
  "AAAAYQQC" varchar(4) NOT NULL default ' ',
  "AAABLTOM" varchar(4) NOT NULL default ' ',
  "AAAAYQQE" varchar(4) NOT NULL default ' ',
  "AAAAYQQF" varchar(4) NOT NULL default ' ',
  "AAAAYQQG" varchar(4) NOT NULL default ' ',
  "AAABBHNF" decimal(5,2) NOT NULL default '0.00',
  "AAAAYQQI" decimal(5,2) NOT NULL default '0.00',
  "AAAAYQQJ" varchar(2) NOT NULL default '00',
  "AAAAYQQK" varchar(4) NOT NULL default ' ',
  "AAAAYQQL" varchar(4) NOT NULL default ' ',
  "AAAAYQQM" varchar(4) NOT NULL default ' ',
  "AAAAYQQN" varchar(4) NOT NULL default ' ',
  "AAAAYQQO" varchar(2) NOT NULL default '00',
  "AAAAYQQP" varchar(4) NOT NULL default ' ',
  "AAAAYQQQ" varchar(4) NOT NULL default ' ',
  "AAAAYQQR" varchar(2) NOT NULL default '00',
  "AAAAYQQS" varchar(2) NOT NULL default '00',
  "AAAAYQQT" varchar(4) NOT NULL default ' ',
  "AAAAYQQU" varchar(4) NOT NULL default ' ',
  "AAAAYQQV" varchar(4) NOT NULL default ' ',
  "AAAAYQQW" varchar(4) NOT NULL default ' ',
  "AAAAYQQX" varchar(4) NOT NULL default ' ',
  "AAAAYQQY" varchar(4) NOT NULL default ' ',
  "AAAAYQQZ" varchar(4) NOT NULL default ' ',
  "AAAAYQRA" varchar(4) NOT NULL default ' ',
  "AAAAYQRB" varchar(4) NOT NULL default ' ',
  "AAAAYQRC" varchar(4) NOT NULL default ' ',
  "AAAAYQRD" varchar(4) NOT NULL default ' ',
  "AAAAYQRE" varchar(4) NOT NULL default ' ',
  "AAAAYQRF" varchar(4) NOT NULL default ' ',
  "AAAAYQRG" varchar(4) NOT NULL default ' ',
  "AAAAYQRH" varchar(4) NOT NULL default ' ',
  "AAAAYQRI" varchar(4) NOT NULL default ' ',
  "AAAAYQRJ" varchar(4) NOT NULL default ' ',
  "AAAAYQRK" varchar(4) NOT NULL default ' ',
  "AAAAYQRL" varchar(4) NOT NULL default ' ',
  "AAAAYQRM" varchar(4) NOT NULL default ' ',
  "AAAAYQRN" varchar(4) NOT NULL default ' ',
  "AAAAYQRO" varchar(4) NOT NULL default ' ',
  "AAAAYQRP" varchar(4) NOT NULL default ' ',
  "AAAAYQRQ" varchar(4) NOT NULL default ' ',
  "AAAAYQRR" varchar(4) NOT NULL default ' ',
  "AAAAYQRS" varchar(4) NOT NULL default ' ',
  "AAAAYQRT" varchar(4) NOT NULL default ' ',
  "AAAAYQRU" varchar(4) NOT NULL default ' ',
  "AAAAYQRV" varchar(4) NOT NULL default ' ',
  "AAAAYQRW" varchar(4) NOT NULL default ' ',
  "AAAAYQRX" varchar(4) NOT NULL default ' ',
  "AAAAYQRY" varchar(4) NOT NULL default ' ',
  "AAAAYQRZ" varchar(4) NOT NULL default ' ',
  "AAAAYQSA" varchar(4) NOT NULL default ' ',
  "AAAAYQSB" varchar(4) NOT NULL default ' ',
  "AAAAYQSC" varchar(4) NOT NULL default ' ',
  "AAAAYQSD" varchar(4) NOT NULL default ' ',
  "AAAAYQSE" varchar(4) NOT NULL default ' ',
  "AAAAYQSF" varchar(4) NOT NULL default ' ',
  "AAAAYQSG" varchar(4) NOT NULL default ' ',
  "AAAAYQSH" varchar(4) NOT NULL default ' ',
  "AAAAYQSI" varchar(4) NOT NULL default ' ',
  "AAAAYQSJ" varchar(4) NOT NULL default ' ',
  "AAAAYQSK" varchar(4) NOT NULL default ' ',
  "AAAAYQSL" varchar(4) NOT NULL default ' ',
  "AAAAYQSM" varchar(4) NOT NULL default ' ',
  "AAAAYQSN" varchar(4) NOT NULL default ' ',
  "AAAAYQSO" varchar(4) NOT NULL default ' ',
  "AAAAYQSP" varchar(4) NOT NULL default ' ',
  "AAAAYQSQ" varchar(4) NOT NULL default ' ',
  "AAAAYQSR" varchar(4) NOT NULL default ' ',
  "AAAAYQSS" varchar(4) NOT NULL default ' ',
  "AAAAYQST" varchar(4) NOT NULL default ' ',
  "AAAAYQSU" varchar(4) NOT NULL default ' ',
  "AAAAYQSV" varchar(4) NOT NULL default ' ',
  "AAAAYQSW" varchar(4) NOT NULL default ' ',
  "AAAAYQSX" varchar(4) NOT NULL default ' ',
  "AAAAYQSY" varchar(4) NOT NULL default ' ',
  "AAAAYQSZ" varchar(4) NOT NULL default ' ',
  "AAAAYQTA" varchar(4) NOT NULL default ' ',
  "AAAAYQTB" varchar(4) NOT NULL default ' ',
  "AAAAYQTC" varchar(4) NOT NULL default ' ',
  "AAAAYQTD" varchar(4) NOT NULL default ' ',
  "AAAAYQTE" varchar(4) NOT NULL default ' ',
  "AAAAYQTF" varchar(4) NOT NULL default ' ',
  "AAAAYQTG" varchar(4) NOT NULL default ' ',
  "AAAAYQTH" varchar(4) NOT NULL default ' ',
  "AAAAYQTI" varchar(4) NOT NULL default ' ',
  "AAAAYQTJ" varchar(4) NOT NULL default ' ',
  "AAAAYQTK" varchar(4) NOT NULL default ' ',
  "AAAAYQTL" varchar(4) NOT NULL default ' ',
  "AAAAYQTM" varchar(4) NOT NULL default ' ',
  "AAAAYQTN" varchar(4) NOT NULL default ' ',
  "AAAAYQTO" varchar(4) NOT NULL default ' ',
  "AAAAYQTP" varchar(4) NOT NULL default ' ',
  "AAAAYQTQ" varchar(4) NOT NULL default ' ',
  "AAAAYQTR" varchar(4) NOT NULL default ' ',
  "AAAAYQTS" varchar(4) NOT NULL default ' ',
  "AAAAYQTT" varchar(4) NOT NULL default ' ',
  "AAAAYQTU" varchar(4) NOT NULL default ' ',
  "AAAAYQTV" varchar(4) NOT NULL default ' ',
  "AAAAYQTW" varchar(4) NOT NULL default ' ',
  "AAAAYQTX" varchar(4) NOT NULL default ' ',
  "AAAAYQTY" varchar(4) NOT NULL default ' ',
  "AAAAYQTZ" varchar(4) NOT NULL default ' ',
  "AAAAYQUA" varchar(4) NOT NULL default ' ',
  "AAAAYQUB" varchar(4) NOT NULL default ' ',
  "AAAAYQUC" varchar(4) NOT NULL default ' ',
  "AAAAYQUD" varchar(4) NOT NULL default ' ',
  "AAAAYQUE" varchar(4) NOT NULL default ' ',
  "AAAAYQUF" varchar(4) NOT NULL default ' ',
  "AAAAYQUG" varchar(4) NOT NULL default ' ',
  "AAAAYQUH" varchar(4) NOT NULL default ' ',
  "AAAAYQUI" varchar(4) NOT NULL default ' ',
  "AAAAYQUJ" varchar(4) NOT NULL default ' ',
  "AAAAYQUK" varchar(4) NOT NULL default ' ',
  "AAAAYQUL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYQUN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQUP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQVI" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXD" varchar(5) NOT NULL default ' ',
  "AAAAYQVS" varchar(3) NOT NULL default ' ',
  "AAAAYQUX" varchar(4) NOT NULL default ' ',
  "AAAAYQUY" varchar(4) NOT NULL default ' ',
  "AAAAYQUZ" varchar(4) NOT NULL default ' ',
  "AAAAYQVA" varchar(4) NOT NULL default ' ',
  "AAAAYQVB" varchar(4) NOT NULL default ' ',
  "AAAAYQVC" varchar(4) NOT NULL default ' ',
  "AAAAYQVD" varchar(4) NOT NULL default ' ',
  "AAAAYQVE" varchar(4) NOT NULL default ' ',
  "AAAAYQVF" varchar(4) NOT NULL default ' ',
  "AAAAYQVG" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYQVI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQVK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXD" varchar(5) NOT NULL default ' ',
  "AAAAYQVS" varchar(3) NOT NULL default ' ',
  "AAAAYQVT" varchar(4) NOT NULL default ' ',
  "AAAAYQVU" varchar(4) NOT NULL default ' ',
  "AAAAYQVV" varchar(4) NOT NULL default ' ',
  "AAAAYQVW" varchar(4) NOT NULL default ' ',
  "AAAAYQVX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXD" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYQXL","AAAAYQXD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQWM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYQXL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXD" varchar(5) NOT NULL default ' ',
  "AAAAYQWZ" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYQXL","AAAAYQXD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQXE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYQXL" varchar(2) NOT NULL default '00',
  "AAAAYQXI" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYQXL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQXM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQXT" varchar(3) NOT NULL default ' ',
  "AAAAYQXU" varchar(3) NOT NULL default '000',
  "AAAAYQXV" varchar(1) NOT NULL default ' ',
  "AAAAYQXW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQYC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQYH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  "AAABBAET" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYRCU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQYP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  "AAAAYRCV" varchar(15) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYRCU","AAAAYRCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQYW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  "AAAAYRCV" varchar(15) NOT NULL default ' ',
  "AAAAYQZB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYRCU","AAAAYRCV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYQZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  "AAAAYRCV" varchar(15) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLVJB" varchar(4) NOT NULL default '0000',
  "AAAAYQZY" varchar(4) NOT NULL default ' ',
  "AAAAYQZZ" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYQZP" varchar(1) NOT NULL default ' ',
  "AAAAYQZQ" varchar(10) NOT NULL default ' ',
  "AAABDISV" varchar(30) NOT NULL default ' ',
  "AAAAYQZS" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYRCU","AAAAYRCV","AAABKFCZ","AAABLVJB","AAAAYQZY","AAAAYQZZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  "AAAAYRCV" varchar(15) NOT NULL default ' ',
  "AAAAYRAL" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYRAH" decimal(10,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYRCU","AAAAYRCV","AAAAYRAL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  "AAAAYRCV" varchar(15) NOT NULL default ' ',
  "AAABLVJB" varchar(4) NOT NULL default '0000',
  "AAAAYRBA" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYRAV" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAYRCU","AAAAYRCV","AAABLVJB","AAAAYRBA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRBC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  "AAAAYRCV" varchar(15) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLVJB" varchar(4) NOT NULL default '0000',
  "AAABDTRC" varchar(5) NOT NULL default '00000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGGKN" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYRCU","AAAAYRCV","AAABKFCZ","AAABLVJB","AAABDTRC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRBR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  "AAAAYRCV" varchar(15) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLVJB" varchar(4) NOT NULL default '0000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAGKE" varchar(2) NOT NULL default '00',
  "AAAAYRBZ" varchar(2) NOT NULL default ' ',
  "AAAAYRCA" varchar(2) NOT NULL default ' ',
  "AAAAYRCB" smallint(6) NOT NULL default '0',
  "AAAAYRCC" varchar(1) NOT NULL default ' ',
  "AAAAYRCD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYRCU","AAAAYRCV","AAABKFCZ","AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRCJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYRCU" varchar(40) NOT NULL default ' ',
  "AAAAYRCV" varchar(15) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLVJB" varchar(4) NOT NULL default '0000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMDXM" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYRCU","AAAAYRCV","AAABKFCZ","AAABLVJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRCY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYREU" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYREU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYREU" varchar(4) NOT NULL default ' ',
  "AAABLPWW" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYREU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRDL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYREV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYREV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRDQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAAAYSRU" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRDY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYREV" varchar(1) NOT NULL default ' ',
  "AAABLPWW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYREV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYREG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYREU" varchar(4) NOT NULL default ' ',
  "AAAAYREV" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYREM" varchar(2) NOT NULL default ' ',
  "AAAAYREN" varchar(2) NOT NULL default ' ',
  "AAAAYREO" varchar(2) NOT NULL default ' ',
  "AAAAYREP" decimal(5,2) NOT NULL default '0.00',
  "AAAAYREQ" decimal(13,2) NOT NULL default '0.00',
  "AAAAYRER" decimal(5,2) NOT NULL default '0.00',
  "AAAAYRES" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYREU","AAAAYREV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYREX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYSTT" varchar(2) NOT NULL default ' ',
  "AAAAYSQJ" varchar(2) NOT NULL default ' ',
  "AAAAYSRB" varchar(2) NOT NULL default ' ',
  "AAAAYRZL" varchar(2) NOT NULL default ' ',
  "AAAAYSDH" varchar(2) NOT NULL default ' ',
  "AAAAYRFG" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRFK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSJF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRFP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSJF" varchar(2) NOT NULL default ' ',
  "AAAAZKCC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYSJF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSJF" varchar(2) NOT NULL default ' ',
  "AAAAYSJG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSJF","AAAAYSJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSJF" varchar(2) NOT NULL default ' ',
  "AAAAYSJG" varchar(2) NOT NULL default ' ',
  "AAAAZKCC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYSJF","AAAAYSJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(2) NOT NULL default ' ',
  "AAAAYSJU" decimal(5,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSJP" decimal(5,2) NOT NULL default '0.00',
  "AAAAYSJQ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAAAYSJU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRHA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(2) NOT NULL default ' ',
  "AAAAYSJT" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSJQ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAAAYSJT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRHL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSJT" varchar(3) NOT NULL default ' ',
  "AAABBAWM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYSJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRHT" (
  "AAAAYSLL" varchar(1) NOT NULL default ' ',
  "AAAAYSLM" varchar(1) NOT NULL default ' ',
  "AAAAYSLN" varchar(1) NOT NULL default ' ',
  "AAAAYSLO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYSLL","AAAAYSLM","AAAAYSLN","AAAAYSLO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRIC" (
  "AAAAYSLL" varchar(1) NOT NULL default ' ',
  "AAAAYSLM" varchar(1) NOT NULL default ' ',
  "AAAAYSLN" varchar(1) NOT NULL default ' ',
  "AAAAYSLO" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSLY" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYSLL","AAAAYSLM","AAAAYSLN","AAAAYSLO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRIP" (
  "AAAAYSMA" varchar(1) NOT NULL default ' ',
  "AAAAYSMB" varchar(1) NOT NULL default ' ',
  "AAAAYSMC" varchar(1) NOT NULL default ' ',
  "AAAAYSMD" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYSMA","AAAAYSMB","AAAAYSMC","AAAAYSMD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRIY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRJG" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYRJD" varchar(2) NOT NULL default '00',
  "AAAAYRJE" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYRJG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAVI" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBAVI" varchar(2) NOT NULL default ' ',
  "AAAAYRJR" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBAVI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRJV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSTT" varchar(2) NOT NULL default ' ',
  "AAAAYSRQ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYRKB" decimal(9,2) NOT NULL default '0.00',
  "AAAAYRKC" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYSTT","AAAAYSRQ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRKH" (
  "AAAAYSRQ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAYSRQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRKK" (
  "AAAAYSRR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAYSRR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRKN" (
  "AAAAYSMA" varchar(1) NOT NULL default ' ',
  "AAAAYSMB" varchar(1) NOT NULL default ' ',
  "AAAAYSMC" varchar(1) NOT NULL default ' ',
  "AAAAYSMD" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSLY" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYSMA","AAAAYSMB","AAAAYSMC","AAAAYSMD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRLA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSNN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRLF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSNN" varchar(2) NOT NULL default ' ',
  "AAAAYSNI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYSNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRLN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYSNW" varchar(1) NOT NULL default ' ',
  "AAAAYSNS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAAAYSNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRLV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYSNW" varchar(1) NOT NULL default ' ',
  "AAAAYSNT" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFHB","AAAAYSNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRMR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYRMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRMK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYRMR" varchar(2) NOT NULL default ' ',
  "AAAAYRMO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYRMR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRMS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSTU" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRMX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSTU" varchar(2) NOT NULL default ' ',
  "AAAAYSOC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYSTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRNF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAMW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRNK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBAMW" varchar(1) NOT NULL default ' ',
  "AAAAYSRH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBAMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBAMW" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABBAMW","AAABEPKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYROB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBAMW" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(2) NOT NULL default ' ',
  "AAAAZFIY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABJFHB","AAABBAMW","AAABEPKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYRX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYROS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABAYRX" varchar(2) NOT NULL default ' ',
  "AAABBFZS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABAYRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRPA" (
  "AAABLOXS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLOXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRPD" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABLOXS" varchar(1) NOT NULL default ' ',
  "AAAAYSXG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAABLOXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRPJ" (
  "AAAAYSXR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYSXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRPM" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSXR" varchar(1) NOT NULL default ' ',
  "AAAAYSXO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAYSXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRPS" (
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSML" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSMM" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSMN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYSWJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRQC" (
  "AAAAYSXR" varchar(1) NOT NULL default ' ',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSMY" decimal(5,2) NOT NULL default '0.00',
  "AAAAYUOG" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYSXR","AAAAYSWJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRQN" (
  "AAAAYSUG" varchar(1) NOT NULL default ' ',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSUD" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYSUG","AAAAYSWJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRQX" (
  "AAAAYSWH" varchar(2) NOT NULL default ' ',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSUP" decimal(9,2) NOT NULL default '0.00',
  "AAABKQPU" decimal(5,2) NOT NULL default '0.00',
  "AAAAYSUR" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSWC" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAAAYSWH","AAAAYSWJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRRK" (
  "AAAAYSWH" varchar(2) NOT NULL default ' ',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVE" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVG" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVH" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVJ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVK" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVL" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSWC" varchar(1) NOT NULL default '0',
  "AAABKQPU" decimal(5,2) NOT NULL default '0.00',
  "AAAAZXNL" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYSWH","AAAAYSWJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRSE" (
  "AAAAYSWH" varchar(2) NOT NULL default ' ',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAABBGAF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSWC" varchar(1) NOT NULL default '0',
  "AAAAZXNL" decimal(9,2) NOT NULL default '0.00',
  "AAABKQPU" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYSWH","AAAAYSWJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRSR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSJT" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRSW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYRX" varchar(2) NOT NULL default ' ',
  "AAAAYSNN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYRX","AAAAYSNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRTD" (
  "AAAAYRTM" varchar(1) NOT NULL default ' ',
  "AAAAYRTF" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYRTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRTH" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYRTM" varchar(1) NOT NULL default ' ',
  "AAAAYRTK" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAYRTM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRTN" (
  "AAAAYRTV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYRTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRTQ" (
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYRTV" varchar(1) NOT NULL default ' ',
  "AAAAYRTT" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMLQA","AAAAYRTV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRVP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYRVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRUB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYRVP" varchar(2) NOT NULL default ' ',
  "AAAAYRUF" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYRVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRUJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(2) NOT NULL default ' ',
  "AAAAYSJE" varchar(4) NOT NULL default ' ',
  "AAAAYSJF" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSJB" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAAAYSJE","AAAAYSJF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRUW" (
  "AAAAYSWH" varchar(2) NOT NULL default ' ',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAABBGAF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSWC" varchar(1) NOT NULL default '0',
  "AAAAZXNL" decimal(9,2) NOT NULL default '0.00',
  "AAABKQPU" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYSWH","AAAAYSWJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRVJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYRX" varchar(2) NOT NULL default ' ',
  "AAAAYRVP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYRX","AAAAYRVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYRWA" varchar(1) NOT NULL default ' ',
  "AAAAYRWB" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABEETX" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBHDK" decimal(9,2) NOT NULL default '0.00',
  "AAAAYRWW" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABHVYL","AAABEETX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSRB" varchar(2) NOT NULL default ' ',
  "AAAAYRXJ" varchar(20) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABLCDW" varchar(3) NOT NULL default ' ',
  "AAAAYRYR" varchar(30) NOT NULL default ' ',
  "AAAAYRYS" varchar(30) NOT NULL default ' ',
  "AAAAYRYT" varchar(30) NOT NULL default ' ',
  "AAAAYRYU" varchar(40) NOT NULL default ' ',
  "AAAAYRYV" varchar(40) NOT NULL default ' ',
  "AAAAYSSS" varchar(20) NOT NULL default ' ',
  "AAAAYSST" varchar(40) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAAAYRYZ" varchar(60) NOT NULL default ' ',
  "AAAAYRZA" varchar(60) NOT NULL default ' ',
  "AAAAYRZB" varchar(60) NOT NULL default ' ',
  "AAAAYRZC" varchar(80) NOT NULL default ' ',
  "AAAAYRZD" varchar(80) NOT NULL default ' ',
  "AAABMCTJ" varchar(5) NOT NULL default ' ',
  "AAAAZMCM" varchar(16) NOT NULL default ' ',
  "AAAAYRZE" varchar(5) NOT NULL default ' ',
  "AAAAYRZH" varchar(2) NOT NULL default ' ',
  "AAAAYRZI" varchar(10) NOT NULL default ' ',
  "AAAAYRZJ" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSRB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRYJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYRZL" varchar(2) NOT NULL default ' ',
  "AAAAYRYM" varchar(20) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABLCDW" varchar(3) NOT NULL default ' ',
  "AAAAYRYR" varchar(30) NOT NULL default ' ',
  "AAAAYRYS" varchar(30) NOT NULL default ' ',
  "AAAAYRYT" varchar(30) NOT NULL default ' ',
  "AAAAYRYU" varchar(40) NOT NULL default ' ',
  "AAAAYRYV" varchar(40) NOT NULL default ' ',
  "AAAAYSSS" varchar(20) NOT NULL default ' ',
  "AAAAYSST" varchar(40) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAAAYRYZ" varchar(60) NOT NULL default ' ',
  "AAAAYRZA" varchar(60) NOT NULL default ' ',
  "AAAAYRZB" varchar(60) NOT NULL default ' ',
  "AAAAYRZC" varchar(80) NOT NULL default ' ',
  "AAAAYRZD" varchar(80) NOT NULL default ' ',
  "AAAAYRZE" varchar(5) NOT NULL default ' ',
  "AAAAYRZF" varchar(10) NOT NULL default ' ',
  "AAAAYRZG" varchar(12) NOT NULL default ' ',
  "AAAAYRZH" varchar(2) NOT NULL default ' ',
  "AAAAYRZI" varchar(10) NOT NULL default ' ',
  "AAAAYRZJ" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYRZL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRZM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSRC" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSQW" varchar(24) NOT NULL default ' ',
  "AAAAYSQX" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYRZW" (
  "AAAAYSAC" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSAA" varchar(3) NOT NULL default '000',
  "AAABBGAF" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAAAYSAC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSAE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAUV" varchar(2) NOT NULL default ' ',
  "AAAAYSAN" varchar(10) NOT NULL default '0000000000',
  "AAABBCLO" varchar(80) NOT NULL default ' ',
  "AAAAYSAJ" varchar(80) NOT NULL default ' ',
  "AAAAYSBA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAUV","AAAAYSAN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSAO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSBA" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYSBA" varchar(1) NOT NULL default ' ',
  "AAAAYSAX" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYSBA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSBG" varchar(3) NOT NULL default '000',
  "AAAAYSBE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSBG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSBH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABAPFQ" varchar(8) NOT NULL default '00000000',
  "AAAAYSBN" varchar(2) NOT NULL default '00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYSBQ" varchar(1) NOT NULL default ' ',
  "AAABAQOM" varchar(2) NOT NULL default ' ',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOTX","AAABBCSI","AAABAWCO","AAABAPFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSBY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSFM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSFM" varchar(2) NOT NULL default ' ',
  "AAAAYSCH" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYSFM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAAAYSFM" varchar(2) NOT NULL default ' ',
  "AAAAYSCQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSDH" varchar(2) NOT NULL default ' ',
  "AAAAYSCX" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSDH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSDA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSDH" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSDF" decimal(13,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYSDH","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSDN" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSDN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSDO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSDU" varchar(1) NOT NULL default ' ',
  "AAAAYSDV" varchar(3) NOT NULL default ' ',
  "AAABMCNE" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSDU","AAAAYSDV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSDW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAAAYSDZ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSEC" (
  "AAABAYBM" varchar(1) NOT NULL default ' ',
  "AAAAYSEJ" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSEH" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABAYBM","AAAAYSEJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSEL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSFV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSEQ" decimal(7,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYSFV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSGM" varchar(1) NOT NULL default ' ',
  "AAAAYSGN" varchar(4) NOT NULL default ' ',
  "AAAAYSFE" varchar(2) NOT NULL default '00',
  "AAAAYSEZ" varchar(2) NOT NULL default '00',
  "AAABLLOQ" decimal(7,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYSGM","AAAAYSGN","AAAAYSFE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSFM" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSGN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSFM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSFO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSFV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSGN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSFV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSFX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYSGM" varchar(1) NOT NULL default ' ',
  "AAAAYSGN" varchar(4) NOT NULL default ' ',
  "AAAAYSGC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYSGM","AAAAYSGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSGH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSGM" varchar(1) NOT NULL default ' ',
  "AAAAYSGN" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSGM","AAAAYSGN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSGO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMWL" varchar(1) NOT NULL default ' ',
  "AAABLOXS" varchar(1) NOT NULL default ' ',
  "AAAAYSGS" varchar(1) NOT NULL default ' ',
  "AAAAYSGT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSGW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAABBMWL" varchar(1) NOT NULL default ' ',
  "AAAAYSHA" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAABBMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSHQ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSHJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSHQ" varchar(1) NOT NULL default ' ',
  "AAAAYSHN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYSHQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSHZ" varchar(4) NOT NULL default ' ',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAABBICM" varchar(2) NOT NULL default ' ',
  "AAABBICN" varchar(4) NOT NULL default ' ',
  "AAABLQAJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSHZ","AAABMGVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSIB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYSII" varchar(4) NOT NULL default ' ',
  "AAAAYSIF" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYSII")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSIJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSJF" varchar(2) NOT NULL default ' ',
  "AAAAYSJG" varchar(2) NOT NULL default ' ',
  "AAAAZKCC" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYSJF","AAAAYSJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSIT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(2) NOT NULL default ' ',
  "AAAAYSJE" varchar(4) NOT NULL default ' ',
  "AAAAYSJF" varchar(2) NOT NULL default ' ',
  "AAAAYSJG" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSJB" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAAAYSJE","AAAAYSJF","AAAAYSJG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSJI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGMEA" varchar(2) NOT NULL default ' ',
  "AAAAYSJT" varchar(3) NOT NULL default ' ',
  "AAAAYSJU" decimal(5,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSJP" decimal(5,2) NOT NULL default '0.00',
  "AAAAYSJQ" tinyint(3) unsigned NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABGMEA","AAAAYSJT","AAAAYSJU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSJW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYSKD" varchar(3) NOT NULL default ' ',
  "AAABBAWM" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYSKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABLCDW" varchar(3) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABLVHJ" varchar(25) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAAAYSPI" varchar(50) NOT NULL default ' ',
  "AAAAYSSS" varchar(20) NOT NULL default ' ',
  "AAAAYSST" varchar(40) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMREX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSKU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMGVL" varchar(15) NOT NULL default ' ',
  "AAAAYSKX" varchar(70) NOT NULL default ' ',
  "AAAAYSKY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMGVL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSLB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSLL" varchar(1) NOT NULL default ' ',
  "AAAAYSLM" varchar(1) NOT NULL default ' ',
  "AAAAYSLN" varchar(1) NOT NULL default ' ',
  "AAAAYSLO" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSLY" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYSLL","AAAAYSLM","AAAAYSLN","AAAAYSLO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSLQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSMA" varchar(1) NOT NULL default ' ',
  "AAAAYSMB" varchar(1) NOT NULL default ' ',
  "AAAAYSMC" varchar(1) NOT NULL default ' ',
  "AAAAYSMD" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSLY" decimal(17,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYSMA","AAAAYSMB","AAAAYSMC","AAAAYSMD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSMF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSML" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSMM" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSMN" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ","AAAAYSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYSXR" varchar(1) NOT NULL default ' ',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSMY" decimal(5,2) NOT NULL default '0.00',
  "AAAAYUOG" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ","AAAAYSXR","AAAAYSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYRX" varchar(2) NOT NULL default ' ',
  "AAAAYSNN" varchar(2) NOT NULL default ' ',
  "AAAAYSNI" varchar(60) NOT NULL default ' ',
  "AAAAYSNJ" varchar(2) NOT NULL default ' ',
  "AAAAYSNK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYRX","AAAAYSNN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYSNW" varchar(1) NOT NULL default ' ',
  "AAAAYSNS" varchar(1) NOT NULL default ' ',
  "AAAAYSNT" varchar(24) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAAAYSNW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSNX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAZVON" varchar(5) NOT NULL default ' ',
  "AAAAYSTU" varchar(2) NOT NULL default ' ',
  "AAAAYSOC" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAZVON","AAAAYSTU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSOH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSQJ" varchar(2) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABLCDW" varchar(3) NOT NULL default ' ',
  "AAABLVHI" varchar(30) NOT NULL default ' ',
  "AAABLVHJ" varchar(30) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAAAYSSR" varchar(50) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAAAYSOT" varchar(14) NOT NULL default ' ',
  "AAAAYSOU" varchar(11) NOT NULL default ' ',
  "AAAAYSOV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSOY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSQJ" varchar(2) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABLCDW" varchar(3) NOT NULL default ' ',
  "AAABLVHI" varchar(30) NOT NULL default ' ',
  "AAABLVHJ" varchar(30) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAAAYSPI" varchar(50) NOT NULL default ' ',
  "AAAAYSSS" varchar(20) NOT NULL default ' ',
  "AAAAYSST" varchar(40) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSQJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSPO" (
  "AAABIKBY" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(13,2) NOT NULL default '0.00',
  "AAAAYSWJ" decimal(13,2) NOT NULL default '0.00',
  "AAAAYSPU" decimal(6,2) NOT NULL default '0.00',
  "AAAAYSPV" decimal(6,2) NOT NULL default '0.00',
  "AAABMPWL" varchar(5) NOT NULL default ' ',
  "AAABKWVR" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABIKBY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSQA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSQJ" varchar(2) NOT NULL default ' ',
  "AAAAYSTU" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSTQ" decimal(6,3) NOT NULL default '0.000',
  "AAAAYSTR" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYSQJ","AAAAYSTU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSQM" (
  "AAABAQOM" varchar(2) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABAQOM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSRB" varchar(2) NOT NULL default ' ',
  "AAAAYSRC" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSQW" varchar(24) NOT NULL default ' ',
  "AAAAYSQX" varchar(24) NOT NULL default ' ',
  "AAAAYSQY" varchar(15) NOT NULL default ' ',
  "AAAAYSQZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSRB","AAAAYSRC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAMW" varchar(1) NOT NULL default ' ',
  "AAAAYSRH" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBAMW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSRK" (
  "AAAAYSRU" varchar(1) NOT NULL default '0',
  "AAAAYSRV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSRP" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSRQ" varchar(2) NOT NULL default '00',
  "AAAAYSRR" varchar(2) NOT NULL default '00',
  "AAAAYSRS" varchar(2) NOT NULL default '00',
  "AAAAYSRT" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAYSRU","AAAAYSRV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABBAMW" varchar(1) NOT NULL default ' ',
  "AAABEPKB" varchar(2) NOT NULL default ' ',
  "AAAAZFIY" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFHB","AAABBAMW","AAABEPKB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSTT" varchar(2) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  "AAABLCDW" varchar(3) NOT NULL default ' ',
  "AAABLVHI" varchar(30) NOT NULL default ' ',
  "AAABLVHJ" varchar(30) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAAAYSSR" varchar(50) NOT NULL default ' ',
  "AAAAYSSS" varchar(20) NOT NULL default ' ',
  "AAAAYSST" varchar(40) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAAAYSSV" varchar(8) NOT NULL default ' ',
  "AAAAYSSW" varchar(8) NOT NULL default ' ',
  "AAAAYSSX" varchar(5) NOT NULL default ' ',
  "AAAAYSSY" varchar(6) NOT NULL default ' ',
  "AAAAYSSZ" varchar(8) NOT NULL default ' ',
  "AAAAYSTA" varchar(8) NOT NULL default ' ',
  "AAAAYSTB" varchar(8) NOT NULL default ' ',
  "AAAAYSTC" varchar(1) NOT NULL default ' ',
  "AAAAYSTD" varchar(1) NOT NULL default ' ',
  "AAAAYSTE" varchar(1) NOT NULL default ' ',
  "AAAAYSTF" varchar(1) NOT NULL default ' ',
  "AAAAYSTG" varchar(1) NOT NULL default ' ',
  "AAAAYSTH" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYSTT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSTK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYSTT" varchar(2) NOT NULL default ' ',
  "AAAAYSTU" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSTQ" decimal(6,3) NOT NULL default '0.000',
  "AAAAYSTR" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYSTT","AAAAYSTU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSTW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYSUG" varchar(1) NOT NULL default ' ',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSUD" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ","AAAAYSUG","AAAAYSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSUI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYSWH" varchar(2) NOT NULL default ' ',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSUP" decimal(9,2) NOT NULL default '0.00',
  "AAABKQPU" decimal(5,2) NOT NULL default '0.00',
  "AAAAYSUR" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSWC" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ","AAAAYSWH","AAAAYSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSUX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYSWH" varchar(2) NOT NULL default ' ',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSVE" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVG" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVH" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVJ" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVK" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSVL" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSWC" varchar(1) NOT NULL default '0',
  "AAAAZXNL" decimal(9,2) NOT NULL default '0.00',
  "AAABKQPU" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ","AAAAYSWH","AAAAYSWI","AAAAYSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSVU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYSWH" varchar(2) NOT NULL default ' ',
  "AAAAYSWI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSWJ" decimal(9,2) NOT NULL default '0.00',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBGAF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYSWC" varchar(1) NOT NULL default '0',
  "AAAAZXNL" decimal(9,2) NOT NULL default '0.00',
  "AAABKQPU" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABKFCZ","AAAAYSWH","AAAAYSWI","AAAAYSWJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYSWU" varchar(1) NOT NULL default ' ',
  "AAAAYSWV" varchar(1) NOT NULL default ' ',
  "AAAAYSWQ" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAABJFHB","AAAAYSWU","AAAAYSWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSWW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAYRX" varchar(2) NOT NULL default ' ',
  "AAABBFZS" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAYRX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSXC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABLOXS" varchar(1) NOT NULL default ' ',
  "AAAAYSXG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABLOXS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYSXR" varchar(1) NOT NULL default ' ',
  "AAAAYSXO" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYSXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSXS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAAAYTVW" varchar(2) NOT NULL default '00',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSXZ" varchar(2) NOT NULL default ' ',
  "AAAAYSYA" varchar(2) NOT NULL default '00',
  "AAAAYSYB" varchar(2) NOT NULL default '00',
  "AAAAYSYC" varchar(2) NOT NULL default '00',
  "AAAAYSYD" varchar(2) NOT NULL default '00',
  "AAAAYSYE" varchar(2) NOT NULL default '00',
  "AAAAYSYF" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABGAMR","AAAAYTVW","AAABBGOU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSYL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAZHIP" varchar(3) NOT NULL default ' ',
  "AAAAZHIQ" varchar(3) NOT NULL default ' ',
  "AAABBARI" varchar(11) NOT NULL default ' ',
  "AAAAYSYS" varchar(9) NOT NULL default ' ',
  "AAAAZHIT" varchar(9) NOT NULL default ' ',
  "AAAAYSYU" varchar(2) NOT NULL default ' ',
  "AAAAYSYV" varchar(4) NOT NULL default ' ',
  "AAAAYSYW" varchar(4) NOT NULL default ' ',
  "AAAAYSYX" varchar(9) NOT NULL default ' ',
  "AAAAYSYY" varchar(10) NOT NULL default ' ',
  "AAAAZHIY" varchar(2) NOT NULL default '00',
  "AAAAZHIZ" varchar(2) NOT NULL default '00',
  "AAAAZMDU" varchar(4) NOT NULL default ' ',
  "AAAAZLEX" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSZG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYSZQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGLBT" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYSZV" varchar(2) NOT NULL default '00',
  "AAAAYTPG" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABGLBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTAA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYTUO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTAL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAAAYTUO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTAT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAAAZXWL" varchar(2) NOT NULL default ' ',
  "AAAAYTUO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABJFGL","AAAAZXWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTUO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTBG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYTUO" varchar(2) NOT NULL default ' ',
  "AAAAYTBK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYTUO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTBO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLECD" varchar(4) NOT NULL default ' ',
  "AAAAYTCA" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYTBW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABLECD","AAAAYTCA","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAAAZHNU" varchar(1) NOT NULL default ' ',
  "AAABBGSQ" varchar(1) NOT NULL default ' ',
  "AAAAYTDE" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABGTXN" varchar(8) NOT NULL default '00000000',
  "AAAAZHNO" decimal(15,2) NOT NULL default '0.00',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLQVJ","AAAAZHNU","AAABBGSQ","AAAAYTDE","AAABHVYL","AAABGTXN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTCX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTDE" varchar(3) NOT NULL default '000',
  "AAAAZLNX" varchar(4) NOT NULL default ' ',
  "AAAAYTSD" varchar(15) NOT NULL default ' ',
  "AAAAYUFL" varchar(16) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTDE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAAAZHNU" varchar(1) NOT NULL default ' ',
  "AAABAXBU" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAAAYTDN" decimal(15,2) NOT NULL default '0.00',
  "AAAAYTDO" decimal(15,2) NOT NULL default '0.00',
  "AAAAYTDP" decimal(15,2) NOT NULL default '0.00',
  "AAAAYTDQ" decimal(15,2) NOT NULL default '0.00',
  "AAAAYTDR" decimal(15,2) NOT NULL default '0.00',
  "AAAAYTDS" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLQVJ","AAAAZHNU","AAABAXBU","AAABBGCX","AAABAWGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTEA" (
  "AAAAZLUQ" varchar(4) NOT NULL default ' ',
  "AAABAYXR" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAZLUQ","AAABAYXR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTEI" (
  "AAAAYTEK" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYTEK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTEL" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZLUQ" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAZLUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTER" (
  "AAAAZLNX" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZLNX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTHD" varchar(2) NOT NULL default ' ',
  "AAABEXGG" varchar(10) NOT NULL default '0000000000',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABBFOJ" varchar(4) NOT NULL default ' ',
  "AAABAXKZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTHD","AAABEXGG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTFE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTHD" varchar(2) NOT NULL default ' ',
  "AAABEXGG" varchar(10) NOT NULL default '0000000000',
  "AAAAYTGD" varchar(2) NOT NULL default ' ',
  "AAAAYUFK" varchar(4) NOT NULL default '0000',
  "AAABLXWK" varchar(10) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTHD","AAABEXGG","AAAAYTGD","AAAAYUFK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTHD" varchar(2) NOT NULL default ' ',
  "AAABEXGG" varchar(10) NOT NULL default '0000000000',
  "AAAAYTGD" varchar(2) NOT NULL default ' ',
  "AAAAYUFK" varchar(4) NOT NULL default '0000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUGS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTHD","AAABEXGG","AAAAYTGD","AAAAYUFK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTGG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTHD" varchar(2) NOT NULL default ' ',
  "AAABAWCC" varchar(4) NOT NULL default ' ',
  "AAAAYTGR" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABEXGG" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAAAYTHD","AAABAWCC","AAAAYTGR","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTGT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTHD" varchar(2) NOT NULL default ' ',
  "AAABEXGG" varchar(10) NOT NULL default '0000000000',
  "AAAAZNAJ" varchar(2) NOT NULL default ' ',
  "AAAAYTHX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUFK" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAAAYTHD","AAABEXGG","AAAAZNAJ","AAAAYTHX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTHI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTHX" varchar(2) NOT NULL default ' ',
  "AAABFJVZ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTHX","AAABFJVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTHP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMRHS" varchar(1) NOT NULL default ' ',
  "AAAAYTHX" varchar(2) NOT NULL default ' ',
  "AAABFJVZ" varchar(2) NOT NULL default ' ',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMRHS","AAAAYTHX","AAABFJVZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTHZ" (
  "AAAAYTKD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYTKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTIC" (
  "AAAAYTPS" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYTPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTIF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYTJP" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYTJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTIM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYTJP" varchar(2) NOT NULL default ' ',
  "AAABHPIX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAYTJP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYTJP" varchar(2) NOT NULL default ' ',
  "AAAAYTJQ" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYTJP","AAAAYTJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTJF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYTJP" varchar(2) NOT NULL default ' ',
  "AAAAYTJQ" varchar(2) NOT NULL default ' ',
  "AAABHPIX" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAAAYTJP","AAAAYTJQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTJR" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZLWY" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  "AAABLPWW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAZLWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTJY" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYTKD" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAYTKD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTKE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCJL" varchar(6) NOT NULL default ' ',
  "AAABBCJM" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABBCJH" varchar(1) NOT NULL default ' ',
  "AAAAYTKK" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBCJL","AAABBCJM","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTKP" (
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
CREATE TABLE "AAAAYTLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYTMJ" varchar(6) NOT NULL default ' ',
  "AAAAZAZL" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYTMJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTLK" (
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
CREATE TABLE "AAAAYTLX" (
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
  "AAAAYTMK" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYTMM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTMN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTMV" varchar(6) NOT NULL default ' ',
  "AAAAYXIN" varchar(3) NOT NULL default '000',
  "AAAAYTMR" varchar(1) NOT NULL default ' ',
  "AAABAEST" varchar(1) NOT NULL default ' ',
  "AAAAYTMT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTMV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTMW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBCJT" varchar(2) NOT NULL default ' ',
  "AAABBCJR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBCJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTNC" (
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
CREATE TABLE "AAAAYTNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGBL" varchar(4) NOT NULL default ' ',
  "AAABEHCE" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGBL","AAABEHCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTNZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGBL" varchar(4) NOT NULL default ' ',
  "AAABEHCE" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBGBL","AAABEHCE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTOJ" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABAYXR" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABAYXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTOP" (
  "AAABAYXR" varchar(4) NOT NULL default ' ',
  "AAABBGBL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABAYXR","AAABBGBL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTOX" (
  "AAAAYTPG" varchar(4) NOT NULL default '0000',
  "AAAAYTOZ" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAAAYTPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTPB" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYTPG" varchar(4) NOT NULL default '0000',
  "AAAAYTPE" varchar(67) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAYTPG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTPH" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBGBL" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABBGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTPN" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYTPS" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAYTPS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTPT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAYTQN" varchar(8) NOT NULL default '00000000',
  "AAABHMRN" varchar(10) NOT NULL default '0000000000',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABFJZE" varchar(1) NOT NULL default ' ',
  "AAAAYTQA" varchar(8) NOT NULL default '00000000',
  "AAAAZLBE" varchar(8) NOT NULL default '00000000',
  "AAAAYTUM" varchar(8) NOT NULL default '00000000',
  "AAAAYTUK" varchar(9) NOT NULL default ' ',
  "AAAAYTUL" varchar(9) NOT NULL default ' ',
  "AAAAYTQF" varchar(5) NOT NULL default '00000',
  "AAAAYTQG" varchar(5) NOT NULL default '00000',
  "AAABMRFO" varchar(2) NOT NULL default ' ',
  "AAAAZOEQ" varchar(12) NOT NULL default ' ',
  "AAABGJNL" varchar(8) NOT NULL default '00000000',
  "AAABHCDU" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAYTQN","AAABHMRN","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTQQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTUK" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTUK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTQV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTRH" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTRA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMLQA" varchar(1) NOT NULL default ' ',
  "AAAAYTRH" varchar(2) NOT NULL default ' ',
  "AAAAYTRE" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMLQA","AAAAYTRH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTRI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAZLNX" varchar(4) NOT NULL default ' ',
  "AAAAYTSD" varchar(20) NOT NULL default ' ',
  "AAAAZLCT" varchar(1) NOT NULL default ' ',
  "AAABBGPC" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABHVYL","AAAAZLNX","AAAAYTSD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTRV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAABMCPV" varchar(4) NOT NULL default ' ',
  "AAAAZLCT" varchar(1) NOT NULL default ' ',
  "AAABBGPC" varchar(8) NOT NULL default '00000000',
  "AAAAYTSW" varchar(5) NOT NULL default '00000',
  "AAAAYTSC" varchar(8) NOT NULL default '00000000',
  "AAAAYTSD" varchar(20) NOT NULL default ' ',
  "AAAAZLNX" varchar(4) NOT NULL default ' ',
  "AAAAYTSF" varchar(8) NOT NULL default '00000000',
  "AAABAOBG" decimal(15,2) NOT NULL default '0.00',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAABBDWB" varchar(6) NOT NULL default '000000',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAAAYTSK" decimal(15,2) NOT NULL default '0.00',
  "AAAAYTSL" decimal(15,2) NOT NULL default '0.00',
  "AAAAYTSM" decimal(15,2) NOT NULL default '0.00',
  "AAABBHVE" decimal(15,2) NOT NULL default '0.00',
  "AAAAYTSO" decimal(15,2) NOT NULL default '0.00',
  "AAAAZLBG" varchar(2) NOT NULL default '00',
  "AAAAYTSQ" varchar(4) NOT NULL default '0000',
  PRIMARY KEY ("AAABMRIL","AAABMREX","AAABMCPV","AAAAZLCT","AAABBGPC","AAAAYTSW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTSX" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZLWY" varchar(2) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  "AAABLPWW" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAZLWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTTE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTVW" varchar(2) NOT NULL default '00',
  "AAAAYTTH" varchar(1) NOT NULL default ' ',
  "AAAAYTTI" varchar(1) NOT NULL default ' ',
  "AAAAYTTJ" varchar(1) NOT NULL default ' ',
  "AAABKQOE" decimal(4,0) NOT NULL default '0',
  "AAAAYTTL" decimal(4,0) NOT NULL default '0',
  "AAAAYTTM" varchar(1) NOT NULL default ' ',
  "AAAAYTTN" decimal(4,0) NOT NULL default '0',
  "AAAAYTTO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTVW" varchar(2) NOT NULL default '00',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAAAYTUC" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYTVW","AAABGCHD","AAAAYTUC","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTUF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(2) NOT NULL default '00',
  "AAABLYZH" varchar(8) NOT NULL default '00000000',
  "AAAAYTUK" varchar(9) NOT NULL default ' ',
  "AAAAYTUL" varchar(9) NOT NULL default ' ',
  "AAAAYTUM" varchar(8) NOT NULL default '00000000',
  "AAAAYTUN" varchar(1) NOT NULL default ' ',
  "AAAAYTUO" varchar(2) NOT NULL default ' ',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAAAZLBE" varchar(8) NOT NULL default '00000000',
  "AAAAZLBF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLQVJ","AAABLYZH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTUW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYTVW" varchar(2) NOT NULL default '00',
  "AAAAYTVN" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYTVC" varchar(10) NOT NULL default ' ',
  "AAAAYTVD" decimal(6,2) NOT NULL default '0.00',
  "AAAAYTVE" varchar(6) NOT NULL default ' ',
  "AAAAYTVF" decimal(6,2) NOT NULL default '0.00',
  "AAAAYTVG" varchar(6) NOT NULL default ' ',
  "AAAAYTVH" decimal(6,2) NOT NULL default '0.00',
  "AAAAYTVI" varchar(6) NOT NULL default ' ',
  "AAAAYTVJ" decimal(4,0) NOT NULL default '0',
  "AAAAYTVK" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYTVW","AAAAYTVN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTVP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYTVW" varchar(2) NOT NULL default '00',
  "AAABHXOU" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYTVW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTVX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZMCM" varchar(16) NOT NULL default ' ',
  "AAAAZLIS" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZLIM" varchar(20) NOT NULL default ' ',
  "AAABIHEH" varchar(4) NOT NULL default ' ',
  "AAAAYTWG" varchar(4) NOT NULL default ' ',
  "AAAAYTWH" varchar(4) NOT NULL default ' ',
  "AAAAZLRO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZMCM","AAAAZLIS","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTWO" (
  "AAAAZLUQ" varchar(4) NOT NULL default ' ',
  "AAABLVCU" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZLUQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTWS" (
  "AAABBRWV" varchar(2) NOT NULL default ' ',
  "AAAAZLNX" varchar(4) NOT NULL default ' ',
  "AAABLGYD" varchar(1) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYTWZ" decimal(11,3) NOT NULL default '0.000',
  "AAAAYTXA" decimal(7,3) NOT NULL default '0.000',
  "AAAAYTXB" decimal(11,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABBRWV","AAAAZLNX","AAABLGYD","AAABLUVE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTXH" (
  "AAAAZLWY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZLWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZMDU" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZMDU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTXQ" (
  "AAABAYXR" varchar(4) NOT NULL default ' ',
  "AAAAZLVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABAYXR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTXU" (
  "AAABBGBL" varchar(4) NOT NULL default ' ',
  "AAAAYTXW" varchar(2) NOT NULL default ' ',
  "AAAAYTXX" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABBGBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTXZ" (
  "AAAAZLWY" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZLWY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTYC" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBRWV" varchar(2) NOT NULL default ' ',
  "AAABILRJ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABBRWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTYI" (
  "AAABBRWV" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABBRWV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTYL" (
  "AAAAZLVU" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAZLVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTYO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUAY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYTYT" decimal(5,3) NOT NULL default '0.000',
  "AAAAYTYU" decimal(5,3) NOT NULL default '0.000',
  "AAAAYTYV" varchar(2) NOT NULL default ' ',
  "AAAAYTYW" varchar(2) NOT NULL default ' ',
  "AAAAYTYX" varchar(2) NOT NULL default ' ',
  "AAAAYTYY" varchar(2) NOT NULL default ' ',
  "AAAAYTYZ" varchar(2) NOT NULL default ' ',
  "AAAAYTZA" varchar(2) NOT NULL default ' ',
  "AAAAYTZB" varchar(2) NOT NULL default ' ',
  "AAAAYTZC" varchar(2) NOT NULL default ' ',
  "AAAAYTZD" varchar(2) NOT NULL default ' ',
  "AAAAYTZE" varchar(1) NOT NULL default ' ',
  "AAAAYTZF" varchar(1) NOT NULL default ' ',
  "AAAAZKPB" varchar(2) NOT NULL default ' ',
  "AAABAOBG" varchar(2) NOT NULL default ' ',
  "AAAAZTWD" varchar(1) NOT NULL default ' ',
  "AAAAZEAD" varchar(2) NOT NULL default '00',
  "AAAAZBEI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUAY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTZO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZKPB" varchar(2) NOT NULL default ' ',
  "AAAAYTZR" varchar(4) NOT NULL default ' ',
  "AAAAYTZS" varchar(4) NOT NULL default ' ',
  "AAAAYTZT" varchar(4) NOT NULL default ' ',
  "AAAAYTZU" varchar(4) NOT NULL default ' ',
  "AAAAYTZV" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZKPB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYTZY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAOBG" varchar(2) NOT NULL default ' ',
  "AAAAYUAG" varchar(4) NOT NULL default '0000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABAOBG","AAAAYUAG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUAH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABGCHE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABGCHE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUAR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUAY" varchar(2) NOT NULL default ' ',
  "AAAAYUAZ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYUAY","AAAAYUAZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABEJFS" varchar(2) NOT NULL default '00',
  "AAABLOZE" varchar(1) NOT NULL default ' ',
  "AAAAYUBZ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUBI" varchar(1) NOT NULL default ' ',
  "AAAAYUBJ" varchar(1) NOT NULL default ' ',
  "AAAAYUBK" varchar(1) NOT NULL default ' ',
  "AAAAYUBL" varchar(1) NOT NULL default ' ',
  "AAAAYUBM" varchar(1) NOT NULL default ' ',
  "AAAAYUBN" varchar(1) NOT NULL default ' ',
  "AAAAYUBO" varchar(1) NOT NULL default ' ',
  "AAAAYUBP" varchar(1) NOT NULL default ' ',
  "AAAAYUBQ" varchar(1) NOT NULL default ' ',
  "AAAAYUBR" varchar(1) NOT NULL default ' ',
  "AAAAYUBS" varchar(1) NOT NULL default ' ',
  "AAAAYUBT" varchar(1) NOT NULL default ' ',
  "AAAAYUBU" varchar(1) NOT NULL default ' ',
  "AAAAYUBV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABEJFS","AAABLOZE","AAAAYUBZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUCB" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAZLVU" varchar(1) NOT NULL default ' ',
  "AAABMOQQ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAZLVU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUCH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUEE" varchar(4) NOT NULL default ' ',
  "AAAAZLNM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABILMX" varchar(15) NOT NULL default ' ',
  "AAAAYUCO" varchar(15) NOT NULL default ' ',
  "AAAAYUDS" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYUEE","AAAAZLNM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUCU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLHPM" varchar(2) NOT NULL default ' ',
  "AAABLUNJ" varchar(8) NOT NULL default '00000000',
  "AAAAYUEE" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABEJKE" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABLHPM","AAABLUNJ","AAAAYUEE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUDH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUEE" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUDM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUDU" varchar(2) NOT NULL default ' ',
  "AAAAZLNM" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUDS" decimal(6,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYUDU","AAAAZLNM","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUDX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYUEE" varchar(4) NOT NULL default ' ',
  "AAABMOQQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYUEE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUEF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAAAZLBE" varchar(8) NOT NULL default '00000000',
  "AAAAZLBF" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUEP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAZLNX" varchar(4) NOT NULL default ' ',
  "AAABEXGG" varchar(9) NOT NULL default '000000000',
  "AAAAYUFK" varchar(4) NOT NULL default '0000',
  "AAAAYUFL" varchar(16) NOT NULL default ' ',
  "AAABEHHJ" varchar(4) NOT NULL default '0000',
  "AAAAYUFN" varchar(3) NOT NULL default ' ',
  "AAABMNYU" varchar(4) NOT NULL default ' ',
  "AAABBWGQ" varchar(3) NOT NULL default ' ',
  "AAABMJUY" varchar(8) NOT NULL default '00000000',
  "AAABEIAQ" varchar(6) NOT NULL default '000000',
  "AAAAYUFC" varchar(1) NOT NULL default ' ',
  "AAABHCQW" varchar(1) NOT NULL default ' ',
  "AAAAYUFE" varchar(1) NOT NULL default ' ',
  "AAABMOHM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAAAZLNX","AAABEXGG","AAAAYUFK","AAAAYUFL","AAABEHHJ","AAAAYUFN","AAABMNYU","AAABBWGQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUFQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABGAMR" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUFX" varchar(30) NOT NULL default ' ',
  "AAABEXGG" varchar(9) NOT NULL default '000000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABGAMR","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUGE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDMU" varchar(1) NOT NULL default ' ',
  "AAAAYUGV" varchar(10) NOT NULL default '0000000000',
  "AAABLQVJ" varchar(4) NOT NULL default '0000',
  "AAAAYUGJ" varchar(2) NOT NULL default ' ',
  "AAABLQCR" varchar(4) NOT NULL default '0000',
  "AAABLZCP" varchar(30) NOT NULL default ' ',
  "AAABLZCR" varchar(30) NOT NULL default ' ',
  "AAABLVHQ" varchar(30) NOT NULL default ' ',
  "AAABLVHI" varchar(25) NOT NULL default ' ',
  "AAABMQNB" varchar(3) NOT NULL default ' ',
  "AAABMQNA" varchar(10) NOT NULL default ' ',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAAAYUGS" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDMU","AAAAYUGV","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUGX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGDI" varchar(1) NOT NULL default ' ',
  "AAABBGDJ" varchar(2) NOT NULL default ' ',
  "AAAAYUTF" varchar(2) NOT NULL default ' ',
  "AAAAYUPD" varchar(2) NOT NULL default '00',
  "AAAAYULI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBGDI","AAABBGDJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUHH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYUTE" varchar(2) NOT NULL default '00',
  "AAABBHFH" varchar(2) NOT NULL default '00',
  "AAAAYUND" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUID" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAYUID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUHW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYUID" varchar(3) NOT NULL default '000',
  "AAABBAET" varchar(80) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYUID")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUIE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYUJB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABJFHB","AAAAYUJB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUIQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYUJB" varchar(2) NOT NULL default ' ',
  "AAABBTMQ" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABHVYP","AAABJFHB","AAAAYUJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUND" varchar(2) NOT NULL default '00',
  "AAAAYULI" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUJI" varchar(20) NOT NULL default ' ',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAABEBBD" varchar(11) NOT NULL default ' ',
  "AAAAZKND" varchar(3) NOT NULL default ' ',
  "AAAAYUJM" varchar(11) NOT NULL default ' ',
  "AAABBEZG" varchar(17) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUND","AAAAYULI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUJS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUND" varchar(2) NOT NULL default '00',
  "AAAAYULI" varchar(2) NOT NULL default '00',
  "AAABBGIT" decimal(5,2) NOT NULL default '0.00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBFAQ" decimal(5,2) NOT NULL default '0.00',
  "AAABBFAP" decimal(5,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYUND","AAAAYULI","AAABBGIT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUKG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYUTF" varchar(2) NOT NULL default ' ',
  "AAAAYUKK" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYUTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUKO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYUPD" varchar(2) NOT NULL default '00',
  "AAABAZQW" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYUPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUKW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYULI" varchar(2) NOT NULL default '00',
  "AAAAYULA" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYULI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYULE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYULI" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYULI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYULJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUPD" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYUPD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYULO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUTF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUTF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYULT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYUTE" varchar(2) NOT NULL default '00',
  "AAABBACF" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYUTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUMB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBHFH" varchar(2) NOT NULL default '00',
  "AAABHXCG" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBHFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUMJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYUND" varchar(2) NOT NULL default '00',
  "AAAAZWZD" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYUND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMCQP" varchar(2) NOT NULL default '00',
  "AAABAJFL" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMCQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUMZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUND" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYUND")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUNE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCQP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMCQP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUNJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHFH" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABBHFH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUNO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUTE" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYUTE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUNT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUTB" varchar(20) NOT NULL default ' ',
  "AAAAYURD" varchar(4) NOT NULL default ' ',
  "AAAAYURE" varchar(2) NOT NULL default ' ',
  "AAABBHSN" varchar(8) NOT NULL default '00000000',
  "AAAAYUSB" varchar(20) NOT NULL default ' ',
  "AAAAYUQY" varchar(20) NOT NULL default ' ',
  "AAAAYUQZ" varchar(8) NOT NULL default '00000000',
  "AAAAYURA" varchar(28) NOT NULL default ' ',
  "AAAAYUOD" decimal(9,2) NOT NULL default '0.00',
  "AAAAYUOE" decimal(9,2) NOT NULL default '0.00',
  "AAAAYUOF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYUOG" decimal(9,2) NOT NULL default '0.00',
  "AAAAYUOH" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUTB","AAAAYURD","AAAAYURE","AAABBHSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUON" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHIPZ" varchar(3) NOT NULL default ' ',
  "AAABILKP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHIPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUOT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHFH" varchar(2) NOT NULL default '00',
  "AAAAYUPD" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUOZ" varchar(20) NOT NULL default ' ',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHFH","AAAAYUPD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUPF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUPS" varchar(4) NOT NULL default ' ',
  "AAAAYUTY" varchar(4) NOT NULL default ' ',
  "AAAAYUTB" varchar(20) NOT NULL default ' ',
  "AAAAYUPV" varchar(20) NOT NULL default ' ',
  "AAABBHSN" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLXTF" varchar(6) NOT NULL default '000000',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  PRIMARY KEY ("AAABMRIL","AAAAYUPS","AAAAYUTY","AAAAYUTB","AAAAYUPV","AAABBHSN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUPX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAZWE" varchar(1) NOT NULL default ' ',
  "AAABBAHC" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUQD" decimal(9,2) NOT NULL default '0.00',
  "AAAAYUQE" varchar(5) NOT NULL default ' ',
  "AAAAYUQF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYUQG" varchar(5) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAZWE","AAABBAHC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBMWL" varchar(4) NOT NULL default ' ',
  "AAAAYUQO" varchar(1) NOT NULL default ' ',
  "AAABAEPB" varchar(1) NOT NULL default ' ',
  "AAAAYUQQ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUQT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUTB" varchar(20) NOT NULL default ' ',
  "AAAAYURD" varchar(4) NOT NULL default ' ',
  "AAAAYURE" varchar(2) NOT NULL default ' ',
  "AAAAYUQY" varchar(20) NOT NULL default ' ',
  "AAAAYUQZ" varchar(8) NOT NULL default '00000000',
  "AAAAYURA" varchar(28) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUTB","AAAAYURD","AAAAYURE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYURF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABBMWL" varchar(4) NOT NULL default ' ',
  "AAAAYURJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABBMWL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYURN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAUV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYURS" decimal(3,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAABBAUV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYURW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUTB" varchar(20) NOT NULL default ' ',
  "AAABMREX" varchar(4) NOT NULL default ' ',
  "AAAAYUSA" varchar(10) NOT NULL default ' ',
  "AAAAYUSB" varchar(20) NOT NULL default ' ',
  "AAABILEY" varchar(1) NOT NULL default ' ',
  "AAABHFFQ" varchar(8) NOT NULL default ' ',
  "AAABHFFR" varchar(1) NOT NULL default ' ',
  "AAAAYUSF" varchar(1) NOT NULL default ' ',
  "AAAAYUSG" varchar(40) NOT NULL default ' ',
  "AAAAYUSH" varchar(10) NOT NULL default ' ',
  "AAAAYUSI" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUTB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUSL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUTY" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABMCQP" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYUTY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUTE" varchar(2) NOT NULL default '00',
  "AAAAYUTF" varchar(2) NOT NULL default ' ',
  "AAABMCQP" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUTB" varchar(20) NOT NULL default ' ',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUTE","AAAAYUTF","AAABMCQP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUTI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBAHC" varchar(9) NOT NULL default '000000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAHD" decimal(7,4) NOT NULL default '0.0000',
  PRIMARY KEY ("AAABMRIL","AAABBAHC","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUTR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYUTY" varchar(4) NOT NULL default ' ',
  "AAAAYUTV" varchar(30) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYUTY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUTZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHLV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYUUE" varchar(6) NOT NULL default ' ',
  "AAAAYUUF" varchar(6) NOT NULL default ' ',
  "AAAAYUUG" varchar(6) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHLV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUUK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILKP" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABILKP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUUS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABILKP" varchar(6) NOT NULL default ' ',
  "AAAAZNRP" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABILKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUVA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUVP" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYUVP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYUVP" varchar(6) NOT NULL default ' ',
  "AAAAZNRP" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYUVP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUVV" varchar(5) NOT NULL default ' ',
  "AAAAYWYN" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUVV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUVW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUWB" varchar(5) NOT NULL default ' ',
  "AAAAYWYN" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUWB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUWC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUWH" varchar(5) NOT NULL default ' ',
  "AAAAYWYN" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUWH")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUWI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAZGUD" varchar(4) NOT NULL default '0000',
  "AAAAYUWL" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAZGUD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUWO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUWT" varchar(2) NOT NULL default '00',
  "AAABJBVB" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUWT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUWU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAZHUY" varchar(2) NOT NULL default '00',
  "AAABAXBU" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABAPFQ" varchar(8) NOT NULL default '00000000',
  "AAAAYUXP" varchar(2) NOT NULL default '00',
  "AAABETOU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAZHUY","AAABAXBU","AAABBGCX","AAABAWGV","AAABBCSI","AAABAWCO","AAABAPFQ","AAAAYUXP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUXQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUXV" varchar(2) NOT NULL default ' ',
  "AAABINFJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUXV","AAABINFJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYUYC" varchar(3) NOT NULL default ' ',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYUYC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUYD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZHNO" decimal(15,2) NOT NULL default '0.00',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAAAYWSK" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAXJM","AAABBCSI","AAABAWCO","AAABKFCV","AAABKFCZ","AAABHVYP","AAABHVYL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUZC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZHNO" decimal(15,2) NOT NULL default '0.00',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAAAYWSK" varchar(13) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABKFCV","AAABKFCZ","AAABHVYP","AAABHVYL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYUZV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWTI" varchar(4) NOT NULL default ' ',
  "AAAAYWTJ" varchar(4) NOT NULL default ' ',
  "AAAAYVJS" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBKQM" varchar(1) NOT NULL default ' ',
  "AAAAYVAD" varchar(1) NOT NULL default ' ',
  "AAAAYVAE" varchar(1) NOT NULL default ' ',
  "AAAAYVAF" varchar(1) NOT NULL default ' ',
  "AAAAYXBL" varchar(2) NOT NULL default ' ',
  "AAABEQNO" varchar(1) NOT NULL default ' ',
  "AAAAYVAI" varchar(4) NOT NULL default ' ',
  "AAAAYVAJ" varchar(4) NOT NULL default ' ',
  "AAAAYVAK" varchar(1) NOT NULL default ' ',
  "AAABDPTH" varchar(1) NOT NULL default ' ',
  "AAAAYVAM" varchar(3) NOT NULL default ' ',
  "AAAAYVAN" varchar(1) NOT NULL default ' ',
  "AAAAYVAO" varchar(1) NOT NULL default ' ',
  "AAAAYVAP" varchar(1) NOT NULL default ' ',
  "AAAAYVAQ" varchar(1) NOT NULL default ' ',
  "AAAAYVAR" varchar(1) NOT NULL default ' ',
  "AAAAZTSZ" varchar(1) NOT NULL default ' ',
  "AAAAYVAT" varchar(4) NOT NULL default ' ',
  "AAAAYVAU" varchar(4) NOT NULL default ' ',
  "AAAAYXGI" varchar(2) NOT NULL default ' ',
  "AAAAYVAW" varchar(4) NOT NULL default ' ',
  "AAAAYVAX" varchar(4) NOT NULL default ' ',
  "AAAAYVAY" varchar(1) NOT NULL default ' ',
  "AAAAYVAZ" varchar(1) NOT NULL default ' ',
  "AAAAYVBA" varchar(1) NOT NULL default ' ',
  "AAAAYVBB" varchar(1) NOT NULL default ' ',
  "AAAAYVBC" varchar(1) NOT NULL default ' ',
  "AAAAYVBD" varchar(1) NOT NULL default ' ',
  "AAAAYVBE" varchar(1) NOT NULL default ' ',
  "AAAAYVBF" varchar(1) NOT NULL default ' ',
  "AAAAYVBG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWTI","AAAAYWTJ","AAAAYVJS","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVBM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYWTI" varchar(4) NOT NULL default ' ',
  "AAAAYWTJ" varchar(4) NOT NULL default ' ',
  "AAAAYVJS" varchar(4) NOT NULL default ' ',
  "AAAAYVBS" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYWTI","AAAAYWTJ","AAAAYVJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVBY" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYXBL" varchar(2) NOT NULL default ' ',
  "AAAAZEDC" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAYXBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVCE" (
  "AAAAYXBL" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYVCI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYXBL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVCL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXBL" varchar(2) NOT NULL default ' ',
  "AAAAYXBM" varchar(6) NOT NULL default '000000',
  "AAAAYXBN" varchar(3) NOT NULL default '000',
  "AAAAYXBO" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXBG" varchar(3) NOT NULL default '000',
  "AAAAYXBH" varchar(10) NOT NULL default ' ',
  "AAAAYXBI" varchar(10) NOT NULL default ' ',
  "AAAAYXBJ" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAAAYXBL","AAAAYXBM","AAAAYXBN","AAAAYXBO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVDD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYXBL" varchar(2) NOT NULL default ' ',
  "AAAAYXBM" varchar(6) NOT NULL default '000000',
  "AAAAYXBN" varchar(3) NOT NULL default '000',
  "AAAAYXBO" varchar(10) NOT NULL default ' ',
  "AAAAYXBK" varchar(65) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYXBL","AAAAYXBM","AAAAYXBN","AAAAYXBO")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVDR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVDZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYVHW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABCAKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVEH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWPW" varchar(2) NOT NULL default ' ',
  "AAAAYWPZ" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAXBU" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAAAYWPW","AAAAYWPZ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVEU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWPW" varchar(2) NOT NULL default ' ',
  "AAAAYWPZ" varchar(1) NOT NULL default ' ',
  "AAAAYVFA" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABCAKN","AAAAYWPW","AAAAYWPZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVFG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWPW" varchar(2) NOT NULL default ' ',
  "AAAAYWPZ" varchar(1) NOT NULL default ' ',
  "AAAAYWQD" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYVLO" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVLP" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVFQ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVFR" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVLY" decimal(5,2) NOT NULL default '0.00',
  "AAABELUO" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVMA" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVLN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAAAYWPW","AAAAYWPZ","AAAAYWQD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVGC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWPW" varchar(2) NOT NULL default ' ',
  "AAAAYWPZ" varchar(1) NOT NULL default ' ',
  "AAAAYWQD" varchar(2) NOT NULL default ' ',
  "AAAAYVGJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABCAKN","AAAAYWPW","AAAAYWPZ","AAAAYWQD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVGQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWPW" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYWPX" varchar(8) NOT NULL default '00000000',
  "AAAAYVGW" varchar(1) NOT NULL default ' ',
  "AAAAYVGX" varchar(5) NOT NULL default ' ',
  "AAAAYXKO" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWPW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVHC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYWPW" varchar(2) NOT NULL default ' ',
  "AAAAYVHG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYWPW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVHK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYVIG" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYVIG","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVHS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYVIG" varchar(2) NOT NULL default ' ',
  "AAAAYVHW" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYVIG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVIA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLOAK" varchar(10) NOT NULL default ' ',
  "AAAAYVIJ" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYVIG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLOAK","AAAAYVIJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVIL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMNYU" varchar(2) NOT NULL default ' ',
  "AAAAZQJT" varchar(3) NOT NULL default '000',
  "AAAAYVIP" varchar(3) NOT NULL default '000',
  "AAABJFGL" varchar(4) NOT NULL default ' ',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABMKXW" varchar(16) NOT NULL default ' ',
  "AAAAYVJG" varchar(2) NOT NULL default ' ',
  "AAAAYVIU" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABMRIL","AAABMNYU","AAAAZQJT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVIY" (
  "AAAAYVJG" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYVJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVJB" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYVJG" varchar(2) NOT NULL default ' ',
  "AAAAYVJE" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAAAYVJG")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVJH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYWTI" varchar(4) NOT NULL default ' ',
  "AAAAYWTJ" varchar(4) NOT NULL default ' ',
  "AAAAYVJS" varchar(4) NOT NULL default ' ',
  "AAAAYVJN" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYWTI","AAAAYWTJ","AAAAYVJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVJT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABAPFQ" varchar(8) NOT NULL default '00000000',
  "AAAAYVKB" varchar(4) NOT NULL default '0000',
  "AAAAYVKC" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAWGV","AAABBGCX","AAABBCSI","AAABAWCO","AAABAPFQ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVKK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABAPPJ" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYVKP" varchar(4) NOT NULL default ' ',
  "AAAAYVKQ" varchar(4) NOT NULL default ' ',
  "AAAAYVKR" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABAPPJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBDCQ" varchar(2) NOT NULL default ' ',
  "AAABBGJO" varchar(1) NOT NULL default ' ',
  "AAABMRBY" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBDCQ","AAABBGJO","AAABMRBY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVLI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYVQY" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYVLN" varchar(2) NOT NULL default ' ',
  "AAAAYVLO" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVLP" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVLQ" varchar(2) NOT NULL default ' ',
  "AAAAYVLR" varchar(2) NOT NULL default ' ',
  "AAAAYVLS" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVLT" decimal(7,2) NOT NULL default '0.00',
  "AAAAYVLU" decimal(7,2) NOT NULL default '0.00',
  "AAAAYVLV" varchar(2) NOT NULL default ' ',
  "AAAAYVLW" varchar(1) NOT NULL default ' ',
  "AAAAYWQD" varchar(2) NOT NULL default ' ',
  "AAAAYVLY" decimal(5,2) NOT NULL default '0.00',
  "AAABELUO" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVMA" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVMB" varchar(1) NOT NULL default ' ',
  "AAAAYVMC" varchar(1) NOT NULL default ' ',
  "AAAAYVMD" varchar(1) NOT NULL default ' ',
  "AAAAYVME" varchar(1) NOT NULL default ' ',
  "AAAAYVMF" varchar(1) NOT NULL default ' ',
  "AAAAYVMG" varchar(1) NOT NULL default ' ',
  "AAAAYVMH" varchar(1) NOT NULL default ' ',
  "AAABJUAC" decimal(4,2) NOT NULL default '0.00',
  "AAAAYVMJ" varchar(1) NOT NULL default ' ',
  "AAAAYVMK" varchar(1) NOT NULL default ' ',
  "AAAAYVML" varchar(8) NOT NULL default '00000000',
  "AAAAYVMM" varchar(8) NOT NULL default '00000000',
  "AAAAYVMN" decimal(6,0) NOT NULL default '0',
  "AAAAYVMO" decimal(9,2) NOT NULL default '0.00',
  "AAAAYVMP" varchar(5) NOT NULL default ' ',
  "AAAAYVMQ" varchar(1) NOT NULL default ' ',
  "AAAAYVMR" varchar(1) NOT NULL default ' ',
  "AAAAYVMS" decimal(4,2) NOT NULL default '0.00',
  "AAAAYVMT" varchar(1) NOT NULL default ' ',
  "AAAAYVMU" varchar(2) NOT NULL default '00',
  "AAAAYVMV" varchar(4) NOT NULL default '0000',
  "AAAAYVMW" varchar(1) NOT NULL default ' ',
  "AAAAYVMX" varchar(1) NOT NULL default ' ',
  "AAAAYVMY" varchar(1) NOT NULL default ' ',
  "AAAAYVMZ" varchar(1) NOT NULL default ' ',
  "AAAAZMNK" varchar(4) NOT NULL default ' ',
  "AAAAYVNB" varchar(1) NOT NULL default ' ',
  "AAAAYVNC" varchar(4) NOT NULL default ' ',
  "AAAAYVND" varchar(1) NOT NULL default ' ',
  "AAAAYVNE" varchar(1) NOT NULL default ' ',
  "AAAAYVNF" varchar(1) NOT NULL default ' ',
  "AAAAYVNG" varchar(1) NOT NULL default ' ',
  "AAAAYVNH" varchar(1) NOT NULL default ' ',
  "AAAAYVNI" decimal(2,0) NOT NULL default '0',
  "AAAAYVNJ" decimal(7,2) NOT NULL default '0.00',
  "AAAAYVNK" varchar(1) NOT NULL default ' ',
  "AAAAYVNL" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVNM" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVNN" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVNO" varchar(1) NOT NULL default ' ',
  "AAAAYVNP" varchar(8) NOT NULL default '00000000',
  "AAAAYVNQ" decimal(4,2) NOT NULL default '0.00',
  "AAAAYVNR" varchar(1) NOT NULL default ' ',
  "AAAAYVNS" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVNT" varchar(1) NOT NULL default ' ',
  "AAAAYVNU" varchar(1) NOT NULL default ' ',
  "AAAAYVNV" varchar(4) NOT NULL default ' ',
  "AAAAYVNW" decimal(2,0) NOT NULL default '0',
  "AAAAYVNX" decimal(7,2) NOT NULL default '0.00',
  "AAAAYVNY" varchar(1) NOT NULL default ' ',
  "AAAAYVNZ" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVOA" decimal(4,2) NOT NULL default '0.00',
  "AAAAYVOB" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVOC" varchar(1) NOT NULL default ' ',
  "AAAAYVOD" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVOE" decimal(4,2) NOT NULL default '0.00',
  "AAAAYVOF" varchar(1) NOT NULL default ' ',
  "AAAAYVOG" varchar(4) NOT NULL default ' ',
  "AAAAYVOH" decimal(2,0) NOT NULL default '0',
  "AAAAYVOI" decimal(7,2) NOT NULL default '0.00',
  "AAAAYVOJ" varchar(1) NOT NULL default ' ',
  "AAAAYVOK" decimal(4,2) NOT NULL default '0.00',
  "AAAAYVOL" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVOM" varchar(1) NOT NULL default ' ',
  "AAAAYVON" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVOO" decimal(4,2) NOT NULL default '0.00',
  "AAAAYVOP" varchar(1) NOT NULL default ' ',
  "AAAAYVOQ" varchar(1) NOT NULL default ' ',
  "AAAAYVOR" varchar(1) NOT NULL default ' ',
  "AAAAYVOS" varchar(1) NOT NULL default ' ',
  "AAAAYVOT" varchar(1) NOT NULL default ' ',
  "AAAAYVOU" varchar(1) NOT NULL default ' ',
  "AAAAYVOV" varchar(1) NOT NULL default ' ',
  "AAAAYVOW" varchar(1) NOT NULL default ' ',
  "AAAAYVOX" varchar(1) NOT NULL default ' ',
  "AAABEBVB" varchar(1) NOT NULL default ' ',
  "AAAAYVOZ" varchar(1) NOT NULL default ' ',
  "AAAAYVPA" varchar(1) NOT NULL default ' ',
  "AAAAYVPB" varchar(1) NOT NULL default ' ',
  "AAAAYVPC" varchar(1) NOT NULL default ' ',
  "AAAAYVPD" varchar(1) NOT NULL default ' ',
  "AAAAYVPE" varchar(1) NOT NULL default ' ',
  "AAAAYVPF" varchar(1) NOT NULL default ' ',
  "AAAAYVPG" varchar(1) NOT NULL default ' ',
  "AAAAYVPH" varchar(1) NOT NULL default ' ',
  "AAAAYVPI" varchar(1) NOT NULL default ' ',
  "AAAAYVPJ" varchar(1) NOT NULL default ' ',
  "AAAAYVPK" varchar(1) NOT NULL default ' ',
  "AAAAYVPL" varchar(1) NOT NULL default ' ',
  "AAAAYVPM" varchar(1) NOT NULL default ' ',
  "AAAAYVPN" varchar(1) NOT NULL default ' ',
  "AAAAYVPO" varchar(1) NOT NULL default ' ',
  "AAAAYVPP" varchar(1) NOT NULL default ' ',
  "AAAAYVPQ" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVPR" varchar(1) NOT NULL default ' ',
  "AAAAYVPS" varchar(1) NOT NULL default ' ',
  "AAAAYVPT" varchar(1) NOT NULL default ' ',
  "AAAAYVPU" varchar(1) NOT NULL default ' ',
  "AAAAYVPV" varchar(1) NOT NULL default ' ',
  "AAAAYVPW" varchar(1) NOT NULL default ' ',
  "AAAAYVPX" decimal(8,2) NOT NULL default '0.00',
  "AAAAYVPY" varchar(1) NOT NULL default ' ',
  "AAAAYVPZ" varchar(1) NOT NULL default ' ',
  "AAAAYVQA" varchar(1) NOT NULL default ' ',
  "AAAAYVQB" varchar(1) NOT NULL default ' ',
  "AAAAYVQC" varchar(1) NOT NULL default ' ',
  "AAAAYVQD" varchar(1) NOT NULL default ' ',
  "AAAAYVQE" decimal(8,2) NOT NULL default '0.00',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAAAYVQH" varchar(1) NOT NULL default ' ',
  "AAAAYVQI" varchar(1) NOT NULL default ' ',
  "AAAAZHGO" varchar(4) NOT NULL default '0000',
  "AAAAYVQK" varchar(2) NOT NULL default ' ',
  "AAAAYVQL" varchar(2) NOT NULL default ' ',
  "AAAAYVQM" varchar(2) NOT NULL default ' ',
  "AAAAYVQN" decimal(10,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAAAYVQY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVQR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYVQY" varchar(2) NOT NULL default ' ',
  "AAABDTOG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYVQY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVQZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAABKAFO" varchar(30) NOT NULL default ' ',
  "AAABLUVE" varchar(10) NOT NULL default '0000000000',
  "AAABMMRH" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAABKAFO","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVRJ" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYVVD" varchar(3) NOT NULL default '000',
  PRIMARY KEY ("AAABHVYP","AAAAYVVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVRO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWKK" varchar(2) NOT NULL default ' ',
  "AAABMLSS" varchar(8) NOT NULL default '00000000',
  "AAAAYVUS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAAAYWKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVRX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYWCQ" varchar(6) NOT NULL default '000000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAAAYWGV" varchar(1) NOT NULL default ' ',
  "AAAAYWGW" varchar(2) NOT NULL default '00',
  "AAABKWVJ" varchar(1) NOT NULL default ' ',
  "AAAAYWGY" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYWCQ","AAABMCRV","AAAAYWGV"),
  KEY "AAAAYVSL" ("AAABMRIL","AAABHVYP","AAAAYWCQ","AAAAYWGV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVSQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYWCQ" varchar(6) NOT NULL default '000000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAAAYVUM" varchar(12) NOT NULL default ' ',
  "AAABMRFQ" varchar(8) NOT NULL default '00000000',
  "AAABLTIZ" varchar(6) NOT NULL default '000000',
  "AAAAYVTA" varchar(8) NOT NULL default '00000000',
  "AAAAYVTB" varchar(8) NOT NULL default '00000000',
  "AAAAYVTC" varchar(8) NOT NULL default '00000000',
  "AAAAYVTD" varchar(8) NOT NULL default '00000000',
  "AAABLTLR" varchar(2) NOT NULL default '00',
  "AAAAYVTF" varchar(1) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAABLOTC" varchar(1) NOT NULL default ' ',
  "AAABLOTD" varchar(1) NOT NULL default ' ',
  "AAABLOTE" varchar(1) NOT NULL default ' ',
  "AAABLOTF" varchar(1) NOT NULL default ' ',
  "AAAAYVTL" varchar(1) NOT NULL default ' ',
  "AAAAYVTM" varchar(1) NOT NULL default ' ',
  "AAAAYVTN" varchar(8) NOT NULL default ' ',
  "AAAAYWKK" varchar(2) NOT NULL default ' ',
  "AAAAYWKU" varchar(2) NOT NULL default ' ',
  "AAAAYVTQ" varchar(1) NOT NULL default ' ',
  "AAAAYVTR" varchar(1) NOT NULL default ' ',
  "AAABDQKL" varchar(2) NOT NULL default '00',
  "AAABDQKM" varchar(2) NOT NULL default '00',
  "AAABDQKN" varchar(2) NOT NULL default '00',
  "AAAAZAMO" varchar(1) NOT NULL default ' ',
  "AAABAPFQ" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  "AAAAYVUA" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYWCQ","AAABKOTX","AAABBGCX","AAABAWGV"),
  KEY "AAAAYVUH" ("AAABMRIL","AAAAYWCQ"),
  KEY "AAAAYVUK" ("AAABMRIL","AAAAYVUM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVUN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWKT" varchar(12) NOT NULL default ' ',
  "AAAAYWKU" varchar(2) NOT NULL default ' ',
  "AAABMLSS" varchar(8) NOT NULL default '00000000',
  "AAAAYVUS" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWKT","AAAAYWKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVUW" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYVVD" varchar(3) NOT NULL default '000',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHVYP","AAAAYVVD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVVE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWKK" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(2) NOT NULL default '00',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAAAYVXR" varchar(40) NOT NULL default ' ',
  "AAAAYVXS" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAAAYWKK","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVVR" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(2) NOT NULL default '00',
  "AAAAYVVV" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABHVYP","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVVZ" (
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABHVYP","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVWE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWIV" varchar(3) NOT NULL default ' ',
  "AAAAYWIT" varchar(160) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVWK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYWCQ" varchar(6) NOT NULL default '000000',
  "AAABLUVE" varchar(6) NOT NULL default '000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABDGCT" varchar(2) NOT NULL default ' ',
  "AAAAYVWR" varchar(8) NOT NULL default '00000000',
  "AAAAYVWS" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYWCQ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVWX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYWCQ" varchar(6) NOT NULL default '000000',
  "AAABLUVE" varchar(6) NOT NULL default '000000',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  "AAABDGCT" varchar(2) NOT NULL default ' ',
  "AAAAYVXE" varchar(2) NOT NULL default ' ',
  "AAAAYVXF" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYWCQ","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVXK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWKT" varchar(12) NOT NULL default ' ',
  "AAAAYWKU" varchar(2) NOT NULL default ' ',
  "AAABLTLR" varchar(2) NOT NULL default '00',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABLEST" varchar(14) NOT NULL default ' ',
  "AAAAYVXR" varchar(40) NOT NULL default ' ',
  "AAAAYVXS" varchar(14) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWKT","AAAAYWKU","AAABLTLR")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVXX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYVZB" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYVYC" varchar(1) NOT NULL default ' ',
  "AAAAYVZI" varchar(5) NOT NULL default ' ',
  "AAAAYVYE" varchar(4) NOT NULL default ' ',
  "AAAAYVYF" varchar(4) NOT NULL default ' ',
  "AAAAYVYG" varchar(15) NOT NULL default ' ',
  "AAAAYVYH" varchar(4) NOT NULL default ' ',
  "AAAAYVYI" varchar(1) NOT NULL default ' ',
  "AAAAYVYJ" varchar(1) NOT NULL default ' ',
  "AAAAYVYK" varchar(1) NOT NULL default ' ',
  "AAAAYVYL" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVYM" decimal(9,2) NOT NULL default '0.00',
  "AAAAYVYN" varchar(5) NOT NULL default ' ',
  "AAAAYVYO" varchar(1) NOT NULL default ' ',
  "AAAAYVYP" varchar(1) NOT NULL default ' ',
  "AAAAYVYQ" varchar(1) NOT NULL default ' ',
  "AAAAYVYR" decimal(5,2) NOT NULL default '0.00',
  "AAAAYVYS" decimal(9,2) NOT NULL default '0.00',
  "AAAAYVYT" varchar(5) NOT NULL default ' ',
  "AAAAYVYU" varchar(8) NOT NULL default ' ',
  "AAABMKCP" varchar(2) NOT NULL default ' ',
  "AAAAYVYW" varchar(1) NOT NULL default ' ',
  "AAAAYVYX" varchar(8) NOT NULL default ' ',
  "AAAAYVYY" varchar(4) NOT NULL default ' ',
  "AAAAYWTA" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYVZB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVZD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYVZI" varchar(5) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYVZI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVZJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABIIPA" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYVZO" varchar(2) NOT NULL default ' ',
  "AAAAYVZP" varchar(4) NOT NULL default ' ',
  "AAAAYVZQ" varchar(4) NOT NULL default ' ',
  "AAABBAHO" varchar(8) NOT NULL default ' ',
  "AAAAYVZS" varchar(4) NOT NULL default ' ',
  "AAAAYVZT" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABIIPA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYVZX" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABIIPA" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABIIPA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMAYY" varchar(2) NOT NULL default '00',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMAYY")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWAN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAAAYWCQ" varchar(6) NOT NULL default '000000',
  "AAAAYWGV" varchar(1) NOT NULL default ' ',
  "AAABLTLR" varchar(2) NOT NULL default '00',
  "AAABLUVE" varchar(6) NOT NULL default '000000',
  "AAABMMRH" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAAAYWCQ","AAAAYWGV","AAABLTLR","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWBB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAAAYXBL" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(101) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAWGV","AAABBGCX","AAAAYXBL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWBN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYWBU" varchar(8) NOT NULL default '00000000',
  "AAAAYWBV" varchar(1) NOT NULL default ' ',
  "AAAAYWBW" varchar(1) NOT NULL default ' ',
  "AAAAYWBX" varchar(1) NOT NULL default ' ',
  "AAAAYWBY" varchar(1) NOT NULL default ' ',
  "AAAAYWBZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABKOTX","AAABAWGV","AAABBGCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWCE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEJW" varchar(8) NOT NULL default ' ',
  "AAABLUVE" varchar(6) NOT NULL default ' ',
  "AAAAYWCI" varchar(101) NOT NULL default ' ',
  "AAABMRFO" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEJW","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWCN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEJW" varchar(8) NOT NULL default ' ',
  "AAAAYWCQ" varchar(6) NOT NULL default '000000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABBBFS" varchar(4) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABGBIT" varchar(1) NOT NULL default ' ',
  "AAABLUWI" varchar(1) NOT NULL default ' ',
  "AAABIXBZ" varchar(1) NOT NULL default ' ',
  "AAAAZTEC" varchar(1) NOT NULL default ' ',
  "AAABMCMU" varchar(8) NOT NULL default '00000000',
  "AAABFRUK" varchar(6) NOT NULL default '000000',
  "AAABJBVR" varchar(12) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEJW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWDJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYWDP" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABKOTX","AAABAWGV","AAABBGCX","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWDV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWKT" varchar(12) NOT NULL default ' ',
  "AAAAYWDY" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWKT")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWEB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAAAYWEG" varchar(1) NOT NULL default ' ',
  "AAAAYWEH" varchar(2) NOT NULL default ' ',
  "AAAAYWEI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAWGV","AAABBGCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWEN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWES" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(2) NOT NULL default '00',
  "AAABFNSU" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMKXW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWFA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKXW" varchar(2) NOT NULL default '00',
  "AAABFWHS" varchar(10) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKXW","AAABFWHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWFH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABMKXW" varchar(2) NOT NULL default '00',
  "AAABFWHS" varchar(10) NOT NULL default ' ',
  "AAAAYWFM" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABMKXW","AAABFWHS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWFR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABBHNP" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAABAWGV","AAABBGCX")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWGK" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYWTA" varchar(4) NOT NULL default ' ',
  "AAAAYWGH" varchar(1) NOT NULL default ' ',
  "AAAAYWGI" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWGK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWGM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYWGV" varchar(1) NOT NULL default ' ',
  "AAAAYWGW" varchar(2) NOT NULL default '00',
  "AAABKWVJ" varchar(1) NOT NULL default ' ',
  "AAAAYWGY" varchar(2) NOT NULL default '00',
  "AAABLEJW" varchar(8) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMCRV","AAABAXJM","AAABBCSI","AAABAWCO","AAABKFCV","AAABKFCZ"),
  KEY "AAAAYWHJ" ("AAABMRIL","AAABHVYP","AAABAXJM","AAABBCSI","AAABAWCO","AAABKFCV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWHR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(5) NOT NULL default '00000',
  "AAAAYWIV" varchar(3) NOT NULL default ' ',
  "AAABKPFM" varchar(1) NOT NULL default ' ',
  "AAAAYWIT" varchar(160) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMCRV","AAABAXJM","AAABBCSI","AAABAWCO","AAABKFCV","AAABKFCZ","AAABHVYL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWIP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWIV" varchar(3) NOT NULL default ' ',
  "AAABKPFM" varchar(1) NOT NULL default ' ',
  "AAAAYWIT" varchar(160) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWIV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWIW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLEJW" varchar(8) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABLUWI" varchar(1) NOT NULL default ' ',
  "AAAAYWJG" varchar(1) NOT NULL default ' ',
  "AAAAYWPZ" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLEJW","AAABMCRV","AAABAXJM","AAABBCSI","AAABAWCO","AAABKFCV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWJQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLRTV" varchar(8) NOT NULL default '00000000',
  "AAABLRTW" varchar(6) NOT NULL default '000000',
  "AAAAYWKA" varchar(12) NOT NULL default ' ',
  "AAABMOIA" varchar(40) NOT NULL default ' ',
  "AAABJTGS" varchar(100) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLRTV","AAABLRTW","AAAAYWKA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWKB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWKK" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABCAKN","AAAAYWKK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWKL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYWKT" varchar(12) NOT NULL default ' ',
  "AAAAYWKU" varchar(2) NOT NULL default ' ',
  "AAABMRCK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYWKT","AAAAYWKU")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWKV" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAABKQGA" varchar(32) NOT NULL default ' ',
  "AAAAYWKZ" varchar(32) NOT NULL default ' ',
  "AAAAYWLA" varchar(32) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAKN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWLD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMKCP" varchar(2) NOT NULL default ' ',
  "AAAAYWLG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMKCP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWLJ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZHNO" decimal(15,2) NOT NULL default '0.00',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAAAYWSK" varchar(13) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWSM" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABBGCX","AAABHVYP","AAABHVYL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWTA" varchar(4) NOT NULL default ' ',
  "AAAAYXHL" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAZNXC" varchar(2) NOT NULL default ' ',
  "AAAAYWMJ" varchar(1) NOT NULL default '0',
  "AAABBGWH" varchar(1) NOT NULL default ' ',
  "AAAAYWML" varchar(1) NOT NULL default ' ',
  "AAAAYWMM" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWTA","AAAAYXHL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWMR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYWTA" varchar(4) NOT NULL default ' ',
  "AAAAYXHL" varchar(2) NOT NULL default '00',
  "AAAAYWMW" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYWTA","AAAAYXHL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWNB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYWPW" varchar(2) NOT NULL default ' ',
  "AAAAYWPX" varchar(8) NOT NULL default '00000000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWPZ" varchar(1) NOT NULL default ' ',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAAAYWQD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABKFCV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWNS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMQNA" varchar(4) NOT NULL default ' ',
  "AAABLUVE" varchar(2) NOT NULL default '00',
  "AAABCAAD" varchar(25) NOT NULL default ' ',
  "AAABLVHQ" varchar(40) NOT NULL default ' ',
  "AAABAZBK" varchar(40) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMQNA","AAABLUVE")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWOC" (
  "AAABILKP" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKVPX" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABILKP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWOJ" (
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABILKP" varchar(6) NOT NULL default ' ',
  "AAABMRCK" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABLSCE","AAABILKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWOP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWOY" varchar(4) NOT NULL default '0000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWPW" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKGMO" decimal(15,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYWOY","AAABMCRV","AAABCAKN","AAAAYWPW","AAABHVYL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBAEF" varchar(5) NOT NULL default ' ',
  "AAABFIIK" decimal(9,2) NOT NULL default '0.00',
  "AAAAYWPK" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWPO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYWPW" varchar(2) NOT NULL default ' ',
  "AAAAYWPX" varchar(8) NOT NULL default '00000000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWPZ" varchar(1) NOT NULL default ' ',
  "AAABKOTX" varchar(2) NOT NULL default ' ',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAAAYWQD" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAXJM","AAABBCSI","AAABAWCO","AAABKFCV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWQL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZHNO" decimal(15,2) NOT NULL default '0.00',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAAAYWSK" varchar(13) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWSM" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABBGCX","AAABHVYP","AAABHVYL","AAABLQVJ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZOEP" varchar(3) NOT NULL default '000',
  "AAAAZHNO" decimal(15,2) NOT NULL default '0.00',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAAAYWSK" varchar(13) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWSM" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABBGCX","AAABHVYP","AAABHVYL","AAABLQVJ","AAAAZOEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWRZ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABLQVJ" varchar(3) NOT NULL default '000',
  "AAAAZOEP" varchar(3) NOT NULL default '000',
  "AAAAZHNO" decimal(15,2) NOT NULL default '0.00',
  "AAABAQKE" decimal(15,2) NOT NULL default '0.00',
  "AAABBHDK" decimal(15,2) NOT NULL default '0.00',
  "AAAAYWSK" varchar(13) NOT NULL default ' ',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYWSM" varchar(10) NOT NULL default '0000000000',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABBGCX","AAABHVYP","AAABHVYL","AAABLQVJ","AAAAZOEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWSU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWTA" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYWTA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWTC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWTM" varchar(2) NOT NULL default ' ',
  "AAABLQVJ" varchar(6) NOT NULL default '000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYWTI" varchar(4) NOT NULL default ' ',
  "AAAAYWTJ" varchar(4) NOT NULL default ' ',
  "AAAAYWTK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWTM","AAABLQVJ","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWTP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWUF" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABIADW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWUF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWTY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYWUF" varchar(2) NOT NULL default ' ',
  "AAAAYWUC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYWUF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWUG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKQHD" varchar(6) NOT NULL default '000000',
  "AAABBHNX" VARBINARY(255),
  "AAAAYWUQ" VARBINARY(255),
  "AAAAYWUR" VARBINARY(255),
  "AAAAYWUS" VARBINARY(255),
  "AAAAYWUT" VARBINARY(255),
  "AAAAYWUU" VARBINARY(255),
  "AAAAYWUV" VARBINARY(255),
  "AAAAYWUW" varchar(1) NOT NULL default ' ',
  "AAAAYWUX" VARBINARY(255),
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABLIXT" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCRV","AAABAXJM","AAABBCSI","AAABAWCO","AAABKFCV","AAABKFCZ","AAABKQHD")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWVI" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWVW" varchar(4) NOT NULL default '0000',
  "AAAAZOEP" varchar(3) NOT NULL default '000',
  "AAAAYWVU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWVW","AAAAZOEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWVQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWVW" varchar(4) NOT NULL default '0000',
  "AAAAZOEP" varchar(3) NOT NULL default '000',
  "AAAAYWVU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWVW","AAAAZOEP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWVY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILKP" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKVPX" varchar(2) NOT NULL default '00',
  "AAAAYWWZ" varchar(1) NOT NULL default ' ',
  "AAAAYWXA" varchar(6) NOT NULL default ' ',
  "AAAAYWXB" varchar(1) NOT NULL default '0',
  "AAABJDQV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILKP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWWL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABILKP" varchar(6) NOT NULL default ' ',
  "AAAAZNRP" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABILKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWWT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABILKP" varchar(6) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKVPX" varchar(2) NOT NULL default '00',
  "AAAAYWWZ" varchar(1) NOT NULL default ' ',
  "AAAAYWXA" varchar(6) NOT NULL default ' ',
  "AAAAYWXB" varchar(1) NOT NULL default '0',
  "AAABJDQV" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABILKP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWXG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAABILKP" varchar(6) NOT NULL default ' ',
  "AAAAZNRP" varchar(132) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAABILKP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWXO" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMMVH" varchar(40) NOT NULL default ' ',
  "AAABLSTE" varchar(4) NOT NULL default ' ',
  "AAABMDEN" varchar(12) NOT NULL default ' ',
  "AAAAYWXT" varchar(128) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMMVH","AAABLSTE","AAABMDEN")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWXY" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABMCRV" varchar(8) NOT NULL default '00000000',
  "AAABLQVJ" varchar(6) NOT NULL default ' ',
  "AAABKCWA" varchar(1) NOT NULL default ' ',
  "AAABMRCK" varchar(255) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABMCRV","AAABLQVJ","AAABKCWA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWYK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYWYP" varchar(5) NOT NULL default ' ',
  "AAAAYWYN" varchar(48) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYWYP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWYQ" (
  "AAAAYXBW" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYWYU" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYXBW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWYX" (
  "AAAAYXAB" varchar(1) NOT NULL default ' ',
  "AAAAYWZE" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYWZC" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYXAB","AAAAYWZE","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWZG" (
  "AAAAYWZL" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYWZK" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYWZL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWZN" (
  "AAAAYXAB" varchar(1) NOT NULL default ' ',
  "AAABKTRY" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAYXAB","AAABKTRY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYWZV" (
  "AAAAYXAB" varchar(1) NOT NULL default ' ',
  "AAABKTRY" varchar(3) NOT NULL default ' ',
  "AAAAYXFD" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAYXAB","AAABKTRY","AAAAYXFD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXAF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBEON" varchar(2) NOT NULL default ' ',
  "AAABBEOO" varchar(2) NOT NULL default ' ',
  "AAABBEOP" varchar(1) NOT NULL default ' ',
  "AAABGLBU" varchar(8) NOT NULL default ' ',
  "AAABGLBV" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXFB" varchar(2) NOT NULL default ' ',
  "AAABKTRY" varchar(3) NOT NULL default ' ',
  "AAAAYXFD" varchar(3) NOT NULL default ' ',
  "AAAAYXAR" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBEON","AAABBEOO","AAABBEOP","AAABGLBU","AAABGLBV","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXAZ" (
  "AAAAYXBL" varchar(2) NOT NULL default ' ',
  "AAAAYXBM" varchar(6) NOT NULL default '000000',
  "AAAAYXBN" varchar(3) NOT NULL default '000',
  "AAAAYXBO" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXBG" varchar(3) NOT NULL default '000',
  "AAAAYXBH" varchar(10) NOT NULL default ' ',
  "AAAAYXBI" varchar(10) NOT NULL default ' ',
  "AAAAYXBJ" varchar(3) NOT NULL default '000',
  "AAAAYXBK" varchar(65) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYXBL","AAAAYXBM","AAAAYXBN","AAAAYXBO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXBQ" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBHLV" varchar(8) NOT NULL default '00000000',
  "AAABGLBT" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXBW" varchar(3) NOT NULL default ' ',
  "AAAAYXBX" varchar(2) NOT NULL default ' ',
  "AAAAYXBY" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBHLV","AAABGLBT","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXCD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVXC" varchar(2) NOT NULL default ' ',
  "AAABBHFX" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXCJ" varchar(1) NOT NULL default ' ',
  "AAAAYXCK" varchar(1) NOT NULL default ' ',
  "AAAAYXCL" varchar(2) NOT NULL default ' ',
  "AAAAYXCM" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVXC","AAABBHFX","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXCR" (
  "AAAAYXCW" varchar(3) NOT NULL default '000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXCV" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYXCW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXCY" (
  "AAABBAEF" varchar(10) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLXZQ" decimal(9,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABBAEF","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXDF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYXIA" varchar(1) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAAAYXDO" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYXDR" varchar(1) NOT NULL default ' ',
  "AAAAYXDS" varchar(8) NOT NULL default '00000000',
  "AAAAYXDT" varchar(1) NOT NULL default ' ',
  "AAAAYXDU" varchar(1) NOT NULL default ' ',
  "AAAAYXDV" varchar(1) NOT NULL default ' ',
  "AAAAYXGV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAAAYXIA","AAABAWGV","AAABBGCX","AAABAXJM","AAABAWCO","AAABBCSI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXEF" (
  "AAAAYXFB" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXEJ" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYXFB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXEM" (
  "AAAAYXFB" varchar(2) NOT NULL default ' ',
  "AAABKTRY" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXER" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAAAYXFB","AAABKTRY","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXEV" (
  "AAAAYXFB" varchar(2) NOT NULL default ' ',
  "AAABKTRY" varchar(3) NOT NULL default ' ',
  "AAAAYXFD" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAAAYXFB","AAABKTRY","AAAAYXFD","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXFF" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYXIA" varchar(1) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAAAYXGA" varchar(8) NOT NULL default ' ',
  "AAABETOU" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAAAYXIA","AAABAWGV","AAABBGCX","AAABAXJM","AAABAWCO","AAABBCSI","AAABHVYL","AAAAYXGA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXGB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXGI" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXGG" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYXGI","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXGK" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYXIA" varchar(1) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAAAYXGV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAAAYXIA","AAABAWGV","AAABBGCX","AAABAXJM","AAABAWCO","AAABBCSI")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXHE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXHL" varchar(2) NOT NULL default '00',
  "AAAAZNXC" varchar(2) NOT NULL default ' ',
  "AAAAYXHI" varchar(4) NOT NULL default ' ',
  "AAAAYXHJ" varchar(1) NOT NULL default '0',
  PRIMARY KEY ("AAABMRIL","AAAAYXHL","AAAAZNXC")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXHN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYXIA" varchar(1) NOT NULL default ' ',
  "AAABAWGV" varchar(2) NOT NULL default '00',
  "AAABBGCX" varchar(4) NOT NULL default '0000',
  "AAABBCSI" varchar(1) NOT NULL default ' ',
  "AAABAWCO" varchar(1) NOT NULL default ' ',
  "AAABAXJM" varchar(8) NOT NULL default '00000000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYXHX" varchar(70) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAAAYXIA","AAABAWGV","AAABBGCX","AAABBCSI","AAABAWCO","AAABAXJM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXIG" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXIP" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXIL" decimal(5,2) NOT NULL default '0.00',
  "AAAAYXIM" decimal(9,2) NOT NULL default '0.00',
  "AAAAYXIN" varchar(2) NOT NULL default '00',
  PRIMARY KEY ("AAABMRIL","AAAAYXIP","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXIR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABCAKN" varchar(2) NOT NULL default ' ',
  "AAAAYXJB" varchar(4) NOT NULL default '0000',
  "AAABLMKH" varchar(8) NOT NULL default '00000000',
  "AAABLIRO" varchar(6) NOT NULL default '000000',
  "AAABHZPH" varchar(8) NOT NULL default '00000000',
  "AAABHZPI" varchar(6) NOT NULL default '000000',
  PRIMARY KEY ("AAABMRIL","AAABCAKN","AAAAYXJB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXJC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXKO" varchar(2) NOT NULL default ' ',
  "AAABAWZL" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXJI" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYXKO","AAABAWZL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXJN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAAAYXKO" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABAWZL" varchar(2) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABGCHD","AAABJFHB","AAAAYXKO","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXKA" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABDVOC" varchar(2) NOT NULL default ' ',
  "AAAAYXKN" varchar(2) NOT NULL default ' ',
  "AAAAYXKO" varchar(2) NOT NULL default ' ',
  "AAABGCHD" varchar(2) NOT NULL default '00',
  "AAABJFHB" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXMB" varchar(1) NOT NULL default ' ',
  "AAAAYXKK" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABDVOC","AAAAYXKN","AAAAYXKO","AAABGCHD","AAABJFHB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXKS" (
  "AAABLKZT" varchar(4) NOT NULL default ' ',
  "AAAAYXKZ" varchar(4) NOT NULL default ' ',
  "AAAAYXLA" varchar(2) NOT NULL default '00',
  "AAAAYXLB" varchar(4) NOT NULL default ' ',
  "AAABMNML" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABLKZT","AAAAYXKZ","AAAAYXLA","AAAAYXLB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXLC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXUV" varchar(2) NOT NULL default ' ',
  "AAAAYXLL" decimal(3,0) NOT NULL default '0',
  "AAAAYXLG" decimal(3,0) NOT NULL default '0',
  "AAABBDCQ" varchar(2) NOT NULL default ' ',
  "AAAAYXLI" decimal(10,5) NOT NULL default '0.00000',
  PRIMARY KEY ("AAABMRIL","AAAAYXUV","AAAAYXLL")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXLM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXMB" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYXMB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXLU" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYXMB" varchar(1) NOT NULL default ' ',
  "AAAAYXLY" varchar(60) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYXMB")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXMC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMAMP" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMAMP")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXMH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCVI" varchar(4) NOT NULL default ' ',
  "AAABBHJS" varchar(4) NOT NULL default ' ',
  "AAAAYXML" varchar(20) NOT NULL default ' ',
  "AAAAYXMM" varchar(1) NOT NULL default ' ',
  "AAAAYXMN" varchar(11) NOT NULL default ' ',
  "AAAAYXMO" varchar(2) NOT NULL default ' ',
  "AAAAYXMP" varchar(9) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCVI","AAABBHJS")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXMT" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABHVYP" varchar(2) NOT NULL default ' ',
  "AAABAPPK" varchar(4) NOT NULL default ' ',
  "AAABHVYL" varchar(4) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABGMML" varchar(1) NOT NULL default ' ',
  "AAAAYXNB" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABHVYP","AAABAPPK","AAABHVYL","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXNH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXUW" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYXUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXNM" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABMCWF" varchar(3) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABMCWF")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXNR" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXUW" varchar(1) NOT NULL default ' ',
  "AAAAYXOK" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  PRIMARY KEY ("AAABMRIL","AAAAYXUW","AAAAYXOK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXOB" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYXUW" varchar(1) NOT NULL default ' ',
  "AAAAYXOK" varchar(1) NOT NULL default ' ',
  "AAAAZACB" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYXUW","AAAAYXOK")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXOL" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXPB" varchar(5) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABLQCR" varchar(10) NOT NULL default ' ',
  "AAAAZACB" varchar(20) NOT NULL default ' ',
  "AAAAYXOS" varchar(20) NOT NULL default ' ',
  "AAAAYXOT" varchar(20) NOT NULL default ' ',
  "AAABCUWM" varchar(30) NOT NULL default ' ',
  "AAABBCDL" varchar(6) NOT NULL default '000000',
  "AAAAYXOW" varchar(10) NOT NULL default ' ',
  "AAAAYXOX" varchar(25) NOT NULL default ' ',
  "AAABLYNE" varchar(14) NOT NULL default ' ',
  "AAAAYYWA" varchar(25) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYXPB","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXPD" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXPN" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXPI" varchar(1) NOT NULL default ' ',
  "AAAAYXPJ" decimal(5,2) NOT NULL default '0.00',
  "AAAAYXPK" decimal(7,2) NOT NULL default '0.00',
  "AAAAYXPL" decimal(7,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYXPN","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXPP" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXQL" varchar(2) NOT NULL default ' ',
  "AAAAYXQM" varchar(2) NOT NULL default ' ',
  "AAAAYXQA" varchar(2) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXPW" decimal(6,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYXQL","AAAAYXQM","AAAAYXQA","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXQC" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYXQL" varchar(2) NOT NULL default ' ',
  "AAAAYXQM" varchar(2) NOT NULL default ' ',
  "AAAAYXQH" varchar(5) NOT NULL default ' ',
  "AAAAYXQI" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYXQL","AAAAYXQM")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXQN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABBGOU" varchar(2) NOT NULL default '00',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXQS" varchar(1) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABBGOU","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXQW" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYXUV" varchar(3) NOT NULL default ' ',
  "AAAAYYWN" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYXUV")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXRE" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAABLSCE" varchar(1) NOT NULL default ' ',
  "AAAAYXUW" varchar(1) NOT NULL default ' ',
  "AAAAYXRI" varchar(5) NOT NULL default ' ',
  "AAAAYXRJ" varchar(20) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAABLSCE","AAAAYXUW")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXRN" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXUV" varchar(3) NOT NULL default ' ',
  "AAAAYXUW" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYYWH" varchar(3) NOT NULL default ' ',
  "AAAAYXUK" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXUL" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXUM" decimal(7,2) NOT NULL default '0.00',
  "AAAAYXUN" decimal(7,2) NOT NULL default '0.00',
  "AAAAYXUO" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXUP" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXUQ" varchar(1) NOT NULL default ' ',
  "AAAAYXUR" decimal(7,2) NOT NULL default '0.00',
  "AAAAYXUS" decimal(5,3) NOT NULL default '0.000',
  PRIMARY KEY ("AAABMRIL","AAAAYXUV","AAAAYXUW","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXSH" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYXSP" varchar(1) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABARMA" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAABBMOY" decimal(4,2) NOT NULL default '0.00',
  PRIMARY KEY ("AAABMRIL","AAAAYXSP","AAABKFCZ","AAABARMA")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;
CREATE TABLE "AAAAYXSS" (
  "AAABMRIL" varchar(3) NOT NULL default '000',
  "AAAAYYTK" varchar(3) NOT NULL default ' ',
  "AAABKFCZ" varchar(8) NOT NULL default '00000000',
  "AAABKFCV" varchar(8) NOT NULL default '00000000',
  "AAAAYXSX" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXSY" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXSZ" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXTA" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXTB" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXTC" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXTD" decimal(5,3) NOT NULL default '0.000',
  "AAAAYXTE" decimal(9,2) NOT NULL default '0.00',
  "AAAAYXTF" decimal(9,2) NOT NULL default '0.00',
  "AAAAYXTG" decimal(7,2) NOT NULL default '0.00',
  "AAAAYXTH" decimal(9,2) NOT NULL default '0.00',
  "AAAAYXTI" decimal(9,2) NOT NULL default '0.00',
  "AAAAYYJC" decimal(3,0) NOT NULL default '0',
  "AAAAYYSO" decimal(3,0) NOT NULL default '0',
  "AAAAYYSP" decimal(5,3) NOT NULL default '0.000',
  "AAAAYYSS" varchar(1) NOT NULL default ' ',
  "AAAAYXTN" varchar(1) NOT NULL default ' ',
  "AAAAYXTO" varchar(1) NOT NULL default ' ',
  "AAAAYYWH" varchar(3) NOT NULL default ' ',
  "AAAAYXTQ" decimal(9,2) NOT NULL default '0.00',
  "AAABJKCJ" varchar(4) NOT NULL default ' ',
  PRIMARY KEY ("AAABMRIL","AAAAYYTK","AAABKFCZ")
) DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--enable_query_log
