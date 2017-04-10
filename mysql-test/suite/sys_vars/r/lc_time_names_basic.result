SET @global_start_value = @@global.lc_time_names;
SELECT @global_start_value;
@global_start_value
en_US
SET @session_start_value = @@lc_time_names;
SELECT @session_start_value;
@session_start_value
en_US
SET @session_start_value = @@local.lc_time_names;
SELECT @session_start_value;
@session_start_value
en_US
SET @session_start_value = @@session.lc_time_names;
SELECT @session_start_value;
@session_start_value
en_US
'#--------------------FN_DYNVARS_060_01------------------#'
SELECT lc_time_names;
ERROR 42S22: Unknown column 'lc_time_names' in 'field list'
SET lc_time_names=ar_AE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_AE
SET global.lc_time_names=ar_AE;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near 'global.lc_time_names=ar_AE' at line 1
SET session lc_time_names=ar_AE;
SELECT session lc_time_names;
ERROR 42S22: Unknown column 'session' in 'field list'
SET global lc_time_names=ar_AE;
SELECT global lc_time_names;
ERROR 42S22: Unknown column 'global' in 'field list'
'#--------------------FN_DYNVARS_060_02-------------------------#'
SET @@lc_time_names = en_ZA;
SET @@lc_time_names = DEFAULT;
SELECT @@lc_time_names AS DEFAULT_VALUE;
DEFAULT_VALUE
ar_AE
SET @@global.lc_time_names = en_ZA;
SET @@global.lc_time_names = DEFAULT;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_US
'#--------------------FN_DYNVARS_060_03-------------------------#'
SET @@session.lc_time_names = en_IN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_IN
SET @@global.lc_time_names = en_NZ;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_NZ
SELECT @@session.lc_time_names AS res_is_en_IN;
res_is_en_IN
en_IN
SET @@session.lc_time_names = en_AU;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_AU
SELECT @@global.lc_time_names AS res_is_en_NZ;
res_is_en_NZ
en_NZ
SELECT @@global.lc_time_names=@@session.lc_time_names AS res_is_false;
res_is_false
0
'#--------------------FN_DYNVARS_060_04-------------------------#'
SELECT @@lc_time_names = @@session.lc_time_names AS res;
res
1
SELECT @@lc_time_names = @@local.lc_time_names AS res;
res
1
'#--------------------FN_DYNVARS_060_05-------------------------#'
SET @@lc_time_names = en_US | en_GB ;
ERROR 42S22: Unknown column 'en_US' in 'field list'
'#--------------------FN_DYNVARS_060_06-------------------------#'
SET @@session.lc_time_names=ar_AE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_AE
SET @@session.lc_time_names=ar_BH;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_BH
SET @@session.lc_time_names=ar_DZ;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_DZ
SET @@session.lc_time_names=ar_EG;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_EG
SET @@session.lc_time_names=ar_IN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_IN
SET @@session.lc_time_names=ar_IQ;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_IQ
SET @@session.lc_time_names=ar_JO;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_JO
SET @@session.lc_time_names=ar_KW;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_KW
SET @@session.lc_time_names=ar_LB;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_LB
SET @@session.lc_time_names=ar_LY;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_LY
SET @@session.lc_time_names=ar_MA;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_MA
SET @@session.lc_time_names=ar_OM;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_OM
SET @@session.lc_time_names=ar_QA;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_QA
SET @@session.lc_time_names=ar_SA;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_SA
SET @@session.lc_time_names=ar_SD;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_SD
SET @@session.lc_time_names=ar_SY;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_SY
SET @@session.lc_time_names=ar_TN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_TN
SET @@session.lc_time_names=ar_YE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ar_YE
SET @@session.lc_time_names=be_BY;
SELECT @@session.lc_time_names;
@@session.lc_time_names
be_BY
SET @@session.lc_time_names=bg_BG;
SELECT @@session.lc_time_names;
@@session.lc_time_names
bg_BG
SET @@session.lc_time_names=ca_ES;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ca_ES
SET @@session.lc_time_names=cs_CZ;
SELECT @@session.lc_time_names;
@@session.lc_time_names
cs_CZ
SET @@session.lc_time_names=da_DK;
SELECT @@session.lc_time_names;
@@session.lc_time_names
da_DK
SET @@session.lc_time_names=de_AT;
SELECT @@session.lc_time_names;
@@session.lc_time_names
de_AT
SET @@session.lc_time_names=de_BE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
de_BE
SET @@session.lc_time_names=de_CH;
SELECT @@session.lc_time_names;
@@session.lc_time_names
de_CH
SET @@session.lc_time_names=de_DE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
de_DE
SET @@session.lc_time_names=de_LU;
SELECT @@session.lc_time_names;
@@session.lc_time_names
de_LU
SET @@session.lc_time_names=EE;
ERROR HY000: Unknown locale: 'EE'
'Bug: The locale for Estonian - Estonia is not supported. It is present';
'in manual';
'See http://dev.mysql.com/doc/refman/5.1/en/locale-support.html'
SET @@session.lc_time_names=en_AU;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_AU
SET @@session.lc_time_names=en_CA;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_CA
SET @@session.lc_time_names=en_GB;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_GB
SET @@session.lc_time_names=en_IN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_IN
SET @@session.lc_time_names=en_NZ;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_NZ
SET @@session.lc_time_names=en_PH;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_PH
SET @@session.lc_time_names=en_US;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_US
SET @@session.lc_time_names=en_ZA;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_ZA
SET @@session.lc_time_names=en_ZW;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_ZW
SET @@session.lc_time_names=es_AR;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_AR
SET @@session.lc_time_names=es_BO;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_BO
SET @@session.lc_time_names=es_CL;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_CL
SET @@session.lc_time_names=es_CO;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_CO
SET @@session.lc_time_names=es_CR;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_CR
SET @@session.lc_time_names=es_DO;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_DO
SET @@session.lc_time_names=es_EC;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_EC
SET @@session.lc_time_names=es_ES;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_ES
SET @@session.lc_time_names=es_GT;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_GT
SET @@session.lc_time_names=es_HN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_HN
SET @@session.lc_time_names=es_MX;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_MX
SET @@session.lc_time_names=es_NI;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_NI
SET @@session.lc_time_names=es_PA;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_PA
SET @@session.lc_time_names=es_PE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_PE
SET @@session.lc_time_names=es_PR;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_PR
SET @@session.lc_time_names=es_PY;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_PY
SET @@session.lc_time_names=es_SV;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_SV
SET @@session.lc_time_names=es_US;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_US
SET @@session.lc_time_names=es_UY;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_UY
SET @@session.lc_time_names=es_VE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
es_VE
SET @@session.lc_time_names=eu_ES;
SELECT @@session.lc_time_names;
@@session.lc_time_names
eu_ES
SET @@session.lc_time_names=fi_FI;
SELECT @@session.lc_time_names;
@@session.lc_time_names
fi_FI
SET @@session.lc_time_names=fo_FO;
SELECT @@session.lc_time_names;
@@session.lc_time_names
fo_FO
SET @@session.lc_time_names=fr_BE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
fr_BE
SET @@session.lc_time_names=fr_CA;
SELECT @@session.lc_time_names;
@@session.lc_time_names
fr_CA
SET @@session.lc_time_names=fr_CH;
SELECT @@session.lc_time_names;
@@session.lc_time_names
fr_CH
SET @@session.lc_time_names=fr_FR;
SELECT @@session.lc_time_names;
@@session.lc_time_names
fr_FR
SET @@session.lc_time_names=fr_LU;
SELECT @@session.lc_time_names;
@@session.lc_time_names
fr_LU
SET @@session.lc_time_names=gl_ES;
SELECT @@session.lc_time_names;
@@session.lc_time_names
gl_ES
SET @@session.lc_time_names=gu_IN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
gu_IN
SET @@session.lc_time_names=he_IL;
SELECT @@session.lc_time_names;
@@session.lc_time_names
he_IL
SET @@session.lc_time_names=hi_IN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
hi_IN
SET @@session.lc_time_names=hr_HR;
SELECT @@session.lc_time_names;
@@session.lc_time_names
hr_HR
SET @@session.lc_time_names=hu_HU;
SELECT @@session.lc_time_names;
@@session.lc_time_names
hu_HU
SET @@session.lc_time_names=id_ID;
SELECT @@session.lc_time_names;
@@session.lc_time_names
id_ID
SET @@session.lc_time_names=is_IS;
SELECT @@session.lc_time_names;
@@session.lc_time_names
is_IS
SET @@session.lc_time_names=it_CH;
SELECT @@session.lc_time_names;
@@session.lc_time_names
it_CH
SET @@session.lc_time_names=it_IT;
SELECT @@session.lc_time_names;
@@session.lc_time_names
it_IT
SET @@session.lc_time_names=ja_JP;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ja_JP
SET @@session.lc_time_names=ko_KR;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ko_KR
SET @@session.lc_time_names=lt_LT;
SELECT @@session.lc_time_names;
@@session.lc_time_names
lt_LT
SET @@session.lc_time_names=lv_LV;
SELECT @@session.lc_time_names;
@@session.lc_time_names
lv_LV
SET @@session.lc_time_names=mk_MK;
SELECT @@session.lc_time_names;
@@session.lc_time_names
mk_MK
SET @@session.lc_time_names=mn_MN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
mn_MN
SET @@session.lc_time_names=ms_MY;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ms_MY
SET @@session.lc_time_names=nb_NO;
SELECT @@session.lc_time_names;
@@session.lc_time_names
nb_NO
SET @@session.lc_time_names=nl_BE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
nl_BE
SET @@session.lc_time_names=nl_NL;
SELECT @@session.lc_time_names;
@@session.lc_time_names
nl_NL
SET @@session.lc_time_names=no_NO;
SELECT @@session.lc_time_names;
@@session.lc_time_names
no_NO
SET @@session.lc_time_names=pl_PL;
SELECT @@session.lc_time_names;
@@session.lc_time_names
pl_PL
SET @@session.lc_time_names=pt_BR;
SELECT @@session.lc_time_names;
@@session.lc_time_names
pt_BR
SET @@session.lc_time_names=pt_PT;
SELECT @@session.lc_time_names;
@@session.lc_time_names
pt_PT
SET @@session.lc_time_names=ro_RO;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ro_RO
SET @@session.lc_time_names=ru_RU;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ru_RU
SET @@session.lc_time_names=ru_UA;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ru_UA
SET @@session.lc_time_names=sk_SK;
SELECT @@session.lc_time_names;
@@session.lc_time_names
sk_SK
SET @@session.lc_time_names=sl_SI;
SELECT @@session.lc_time_names;
@@session.lc_time_names
sl_SI
SET @@session.lc_time_names=sq_AL;
SELECT @@session.lc_time_names;
@@session.lc_time_names
sq_AL
SET @@session.lc_time_names=sr_YU;
Warnings:
Warning	1287	'sr_YU' is deprecated and will be removed in a future release. Please use sr_RS instead
SELECT @@session.lc_time_names;
@@session.lc_time_names
sr_RS
SET @@session.lc_time_names=sv_FI;
SELECT @@session.lc_time_names;
@@session.lc_time_names
sv_FI
SET @@session.lc_time_names=sr_RS;
SELECT @@session.lc_time_names;
@@session.lc_time_names
sr_RS
SET @@session.lc_time_names=sv_SE;
SELECT @@session.lc_time_names;
@@session.lc_time_names
sv_SE
SET @@session.lc_time_names=ta_IN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ta_IN
SET @@session.lc_time_names=te_IN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
te_IN
SET @@session.lc_time_names=th_TH;
SELECT @@session.lc_time_names;
@@session.lc_time_names
th_TH
SET @@session.lc_time_names=tr_TR;
SELECT @@session.lc_time_names;
@@session.lc_time_names
tr_TR
SET @@session.lc_time_names=uk_UA;
SELECT @@session.lc_time_names;
@@session.lc_time_names
uk_UA
SET @@session.lc_time_names=ur_PK;
SELECT @@session.lc_time_names;
@@session.lc_time_names
ur_PK
SET @@session.lc_time_names=vi_VN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
vi_VN
SET @@session.lc_time_names=zh_CN;
SELECT @@session.lc_time_names;
@@session.lc_time_names
zh_CN
SET @@session.lc_time_names=zh_HK;
SELECT @@session.lc_time_names;
@@session.lc_time_names
zh_HK
SET @@session.lc_time_names=zh_TW;
SELECT @@session.lc_time_names;
@@session.lc_time_names
zh_TW
'#--------------------FN_DYNVARS_060_07-------------------------#'
SET @@global.lc_time_names=ar_AE;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_AE
SET @@global.lc_time_names=ar_BH;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_BH
SET @@global.lc_time_names=ar_DZ;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_DZ
SET @@global.lc_time_names=ar_EG;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_EG
SET @@global.lc_time_names=ar_IN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_IN
SET @@global.lc_time_names=ar_IQ;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_IQ
SET @@global.lc_time_names=ar_JO;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_JO
SET @@global.lc_time_names=ar_KW;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_KW
SET @@global.lc_time_names=ar_LB;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_LB
SET @@global.lc_time_names=ar_LY;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_LY
SET @@global.lc_time_names=ar_MA;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_MA
SET @@global.lc_time_names=ar_OM;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_OM
SET @@global.lc_time_names=ar_QA;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_QA
SET @@global.lc_time_names=ar_SA;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_SA
SET @@global.lc_time_names=ar_SD;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_SD
SET @@global.lc_time_names=ar_SY;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_SY
SET @@global.lc_time_names=ar_TN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_TN
SET @@global.lc_time_names=ar_YE;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ar_YE
SET @@global.lc_time_names=be_BY;
SELECT @@global.lc_time_names;
@@global.lc_time_names
be_BY
SET @@global.lc_time_names=bg_BG;
SELECT @@global.lc_time_names;
@@global.lc_time_names
bg_BG
SET @@global.lc_time_names=ca_ES;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ca_ES
SET @@global.lc_time_names=cs_CZ;
SELECT @@global.lc_time_names;
@@global.lc_time_names
cs_CZ
SET @@global.lc_time_names=da_DK;
SELECT @@global.lc_time_names;
@@global.lc_time_names
da_DK
SET @@global.lc_time_names=de_AT;
SELECT @@global.lc_time_names;
@@global.lc_time_names
de_AT
SET @@global.lc_time_names=de_BE;
SELECT @@global.lc_time_names;
@@global.lc_time_names
de_BE
SET @@global.lc_time_names=de_CH;
SELECT @@global.lc_time_names;
@@global.lc_time_names
de_CH
SET @@global.lc_time_names=de_DE;
SELECT @@global.lc_time_names;
@@global.lc_time_names
de_DE
SET @@global.lc_time_names=de_LU;
SELECT @@global.lc_time_names;
@@global.lc_time_names
de_LU
SET @@global.lc_time_names=EE;
ERROR HY000: Unknown locale: 'EE'
'Bug: The locale for Estonian - Estonia is not supported. It is';
'present in manual';
'See http://dev.mysql.com/doc/refman/5.1/en/locale-support.html';
SELECT @@global.lc_time_names;
@@global.lc_time_names
de_LU
SET @@global.lc_time_names=en_AU;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_AU
SET @@global.lc_time_names=en_CA;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_CA
SET @@global.lc_time_names=en_GB;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_GB
SET @@global.lc_time_names=en_IN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_IN
SET @@global.lc_time_names=en_NZ;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_NZ
SET @@global.lc_time_names=en_PH;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_PH
SET @@global.lc_time_names=en_US;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_US
SET @@global.lc_time_names=en_ZA;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_ZA
SET @@global.lc_time_names=en_ZW;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_ZW
SET @@global.lc_time_names=es_AR;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_AR
SET @@global.lc_time_names=es_BO;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_BO
SET @@global.lc_time_names=es_CL;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_CL
SET @@global.lc_time_names=es_CO;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_CO
SET @@global.lc_time_names=es_CR;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_CR
SET @@global.lc_time_names=es_DO;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_DO
SET @@global.lc_time_names=es_EC;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_EC
SET @@global.lc_time_names=es_ES;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_ES
SET @@global.lc_time_names=es_GT;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_GT
SET @@global.lc_time_names=es_HN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_HN
SET @@global.lc_time_names=es_MX;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_MX
SET @@global.lc_time_names=es_NI;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_NI
SET @@global.lc_time_names=es_PA;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_PA
SET @@global.lc_time_names=es_PE;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_PE
SET @@global.lc_time_names=es_PR;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_PR
SET @@global.lc_time_names=es_PY;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_PY
SET @@global.lc_time_names=es_SV;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_SV
SET @@global.lc_time_names=es_US;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_US
SET @@global.lc_time_names=es_UY;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_UY
SET @@global.lc_time_names=es_VE;
SELECT @@global.lc_time_names;
@@global.lc_time_names
es_VE
SET @@global.lc_time_names=eu_ES;
SELECT @@global.lc_time_names;
@@global.lc_time_names
eu_ES
SET @@global.lc_time_names=fi_FI;
SELECT @@global.lc_time_names;
@@global.lc_time_names
fi_FI
SET @@global.lc_time_names=fo_FO;
SELECT @@global.lc_time_names;
@@global.lc_time_names
fo_FO
SET @@global.lc_time_names=fr_BE;
SELECT @@global.lc_time_names;
@@global.lc_time_names
fr_BE
SET @@global.lc_time_names=fr_CA;
SELECT @@global.lc_time_names;
@@global.lc_time_names
fr_CA
SET @@global.lc_time_names=fr_CH;
SELECT @@global.lc_time_names;
@@global.lc_time_names
fr_CH
SET @@global.lc_time_names=fr_FR;
SELECT @@global.lc_time_names;
@@global.lc_time_names
fr_FR
SET @@global.lc_time_names=fr_LU;
SELECT @@global.lc_time_names;
@@global.lc_time_names
fr_LU
SET @@global.lc_time_names=gl_ES;
SELECT @@global.lc_time_names;
@@global.lc_time_names
gl_ES
SET @@global.lc_time_names=gu_IN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
gu_IN
SET @@global.lc_time_names=he_IL;
SELECT @@global.lc_time_names;
@@global.lc_time_names
he_IL
SET @@global.lc_time_names=hi_IN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
hi_IN
SET @@global.lc_time_names=hr_HR;
SELECT @@global.lc_time_names;
@@global.lc_time_names
hr_HR
SET @@global.lc_time_names=hu_HU;
SELECT @@global.lc_time_names;
@@global.lc_time_names
hu_HU
SET @@global.lc_time_names=id_ID;
SELECT @@global.lc_time_names;
@@global.lc_time_names
id_ID
SET @@global.lc_time_names=is_IS;
SELECT @@global.lc_time_names;
@@global.lc_time_names
is_IS
SET @@global.lc_time_names=it_CH;
SELECT @@global.lc_time_names;
@@global.lc_time_names
it_CH
SET @@global.lc_time_names=it_IT;
SELECT @@global.lc_time_names;
@@global.lc_time_names
it_IT
SET @@global.lc_time_names=ja_JP;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ja_JP
SET @@global.lc_time_names=ko_KR;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ko_KR
SET @@global.lc_time_names=lt_LT;
SELECT @@global.lc_time_names;
@@global.lc_time_names
lt_LT
SET @@global.lc_time_names=lv_LV;
SELECT @@global.lc_time_names;
@@global.lc_time_names
lv_LV
SET @@global.lc_time_names=mk_MK;
SELECT @@global.lc_time_names;
@@global.lc_time_names
mk_MK
SET @@global.lc_time_names=mn_MN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
mn_MN
SET @@global.lc_time_names=ms_MY;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ms_MY
SET @@global.lc_time_names=nb_NO;
SELECT @@global.lc_time_names;
@@global.lc_time_names
nb_NO
SET @@global.lc_time_names=nl_BE;
SELECT @@global.lc_time_names;
@@global.lc_time_names
nl_BE
SET @@global.lc_time_names=nl_NL;
SELECT @@global.lc_time_names;
@@global.lc_time_names
nl_NL
SET @@global.lc_time_names=no_NO;
SELECT @@global.lc_time_names;
@@global.lc_time_names
no_NO
SET @@global.lc_time_names=pl_PL;
SELECT @@global.lc_time_names;
@@global.lc_time_names
pl_PL
SET @@global.lc_time_names=pt_BR;
SELECT @@global.lc_time_names;
@@global.lc_time_names
pt_BR
SET @@global.lc_time_names=pt_PT;
SELECT @@global.lc_time_names;
@@global.lc_time_names
pt_PT
SET @@global.lc_time_names=ro_RO;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ro_RO
SET @@global.lc_time_names=ru_RU;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ru_RU
SET @@global.lc_time_names=ru_UA;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ru_UA
SET @@global.lc_time_names=sk_SK;
SELECT @@global.lc_time_names;
@@global.lc_time_names
sk_SK
SET @@global.lc_time_names=sl_SI;
SELECT @@global.lc_time_names;
@@global.lc_time_names
sl_SI
SET @@global.lc_time_names=sq_AL;
SELECT @@global.lc_time_names;
@@global.lc_time_names
sq_AL
SET @@global.lc_time_names=sr_YU;
Warnings:
Warning	1287	'sr_YU' is deprecated and will be removed in a future release. Please use sr_RS instead
SELECT @@global.lc_time_names;
@@global.lc_time_names
sr_RS
SET @@global.lc_time_names=sv_FI;
SELECT @@global.lc_time_names;
@@global.lc_time_names
sv_FI
SET @@global.lc_time_names=sr_RS;
SELECT @@global.lc_time_names;
@@global.lc_time_names
sr_RS
SET @@global.lc_time_names=sv_SE;
SELECT @@global.lc_time_names;
@@global.lc_time_names
sv_SE
SET @@global.lc_time_names=ta_IN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ta_IN
SET @@global.lc_time_names=te_IN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
te_IN
SET @@global.lc_time_names=th_TH;
SELECT @@global.lc_time_names;
@@global.lc_time_names
th_TH
SET @@global.lc_time_names=tr_TR;
SELECT @@global.lc_time_names;
@@global.lc_time_names
tr_TR
SET @@global.lc_time_names=uk_UA;
SELECT @@global.lc_time_names;
@@global.lc_time_names
uk_UA
SET @@global.lc_time_names=ur_PK;
SELECT @@global.lc_time_names;
@@global.lc_time_names
ur_PK
SET @@global.lc_time_names=vi_VN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
vi_VN
SET @@global.lc_time_names=zh_CN;
SELECT @@global.lc_time_names;
@@global.lc_time_names
zh_CN
SET @@global.lc_time_names=zh_HK;
SELECT @@global.lc_time_names;
@@global.lc_time_names
zh_HK
SET @@global.lc_time_names=zh_TW;
SELECT @@global.lc_time_names;
@@global.lc_time_names
zh_TW
'#--------------------FN_DYNVARS_060_08-------------------------#'
SET @@lc_time_names = ur_pk;
SELECT @@lc_time_names;
@@lc_time_names
ur_PK
SET @@lc_time_names = UR_PK;
SELECT @@lc_time_names;
@@lc_time_names
ur_PK
SET @@lc_time_names = uR_pK;
SELECT @@lc_time_names;
@@lc_time_names
ur_PK
'#--------------------FN_DYNVARS_060_09-------------------------#'
SET @@lc_time_names = 0;
SELECT @@lc_time_names;
@@lc_time_names
en_US
SET @@lc_time_names = 1;
SELECT @@lc_time_names;
@@lc_time_names
en_GB
SET @@lc_time_names = 2;
SELECT @@lc_time_names;
@@lc_time_names
ja_JP
SET @@lc_time_names = 3;
SELECT @@lc_time_names;
@@lc_time_names
sv_SE
SET @@lc_time_names = 10;
SELECT @@lc_time_names;
@@lc_time_names
ar_SY
SET @@lc_time_names = 50;
SELECT @@lc_time_names;
@@lc_time_names
te_IN
SET @@lc_time_names = 100;
SELECT @@lc_time_names;
@@lc_time_names
fr_BE
SET @@lc_time_names = 101;
SELECT @@lc_time_names;
@@lc_time_names
fr_CA
SET @@lc_time_names = 107;
SELECT @@lc_time_names;
@@lc_time_names
sv_FI
SET @@lc_time_names = 108;
SELECT @@lc_time_names;
@@lc_time_names
zh_HK
SET @@lc_time_names = 109;
SELECT @@lc_time_names;
@@lc_time_names
el_GR
SET @@lc_time_names = 110;
SELECT @@lc_time_names;
@@lc_time_names
rm_CH
SET @@lc_time_names = 111;
ERROR HY000: Unknown locale: '111'
'#--------------------FN_DYNVARS_060_10-------------------------#'
SET @@lc_time_names = en_EN;
ERROR HY000: Unknown locale: 'en_EN'
SET @@lc_time_names = US_en;
ERROR HY000: Unknown locale: 'US_en'
SET @@lc_time_names = 1.1;
ERROR 42000: Incorrect argument type to variable 'lc_time_names'
SET @@lc_time_names = -1;
ERROR HY000: Unknown locale: '-1'
SET @@lc_time_names = 'en US';
ERROR HY000: Unknown locale: 'en US'
SET @@lc_time_names = 'enUS';
ERROR HY000: Unknown locale: 'enUS'
SET @@lc_time_names = true;
SELECT @@lc_time_names AS res_with_true;
res_with_true
en_GB
SET @@lc_time_names = false;
SELECT @@lc_time_names AS res_with_false;
res_with_false
en_US
SET @@lc_time_names = ON;
ERROR HY000: Unknown locale: 'ON'
'#--------------------FN_DYNVARS_060_11-------------------------#'
SET @@global.lc_time_names = en_EN;
ERROR HY000: Unknown locale: 'en_EN'
SET @@global.lc_time_names = US_en;
ERROR HY000: Unknown locale: 'US_en'
SET @@global.lc_time_names = 1.1;
ERROR 42000: Incorrect argument type to variable 'lc_time_names'
SET @@global.lc_time_names = -1;
ERROR HY000: Unknown locale: '-1'
SET @@global.lc_time_names = NULL;
ERROR 42000: Variable 'lc_time_names' can't be set to the value of 'NULL'
SET @@global.lc_time_names = 'enUS';
ERROR HY000: Unknown locale: 'enUS'
SET @@global.lc_time_names = true;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_GB
SET @@global.lc_time_names = false;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_US
SET @@global.lc_time_names = ON;
ERROR HY000: Unknown locale: 'ON'
'#--------------------FN_DYNVARS_060_12-------------------------#'
SELECT @@global.lc_time_names = (SELECT VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='lc_time_names') AS res;
res
1
SET @@global.lc_time_names = 1;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_GB
SELECT @@global.lc_time_names = (SELECT VARIABLE_VALUE 
FROM performance_schema.global_variables 
WHERE VARIABLE_NAME='lc_time_names') AS res;
res
1
'#--------------------FN_DYNVARS_060_13-------------------------#'
SELECT @@lc_time_names = (SELECT VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='lc_time_names') AS res;
res
1
SELECT @@local.lc_time_names = (SELECT VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='lc_time_names') AS res;
res
1
SELECT @@session.lc_time_names = (SELECT VARIABLE_VALUE 
FROM performance_schema.session_variables 
WHERE VARIABLE_NAME='lc_time_names') AS res;
res
1
SET @@global.lc_time_names = @global_start_value;
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_US
SET @@session.lc_time_names = @session_start_value;
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_US
