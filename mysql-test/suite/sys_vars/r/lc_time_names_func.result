'#--------------------FN_DYNVARS_060_01-------------------------#'
SET @@global.lc_time_names = "en_CA";
'connect (con1,localhost,root,,,,)'
'connection con1'
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_CA
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_CA
SET @@global.lc_time_names = "en_IN";
'connect (con2,localhost,root,,,,)'
'connection con2'
SELECT @@global.lc_time_names;
@@global.lc_time_names
en_IN
SELECT @@session.lc_time_names;
@@session.lc_time_names
en_IN
'#--------------------FN_DYNVARS_060_02-------------------------#'
'connection default'
SET NAMES 'utf8';
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SET @@session.lc_time_names = 'en_US';
SELECT DATE_FORMAT('2010-01-01','%W %a %M %b'), DAYNAME('2010-05-05'),MONTHNAME('2010-12-12');
DATE_FORMAT('2010-01-01','%W %a %M %b')	DAYNAME('2010-05-05')	MONTHNAME('2010-12-12')
Friday Fri January Jan	Wednesday	December
SET @@session.lc_time_names = 'ar_AE';
SELECT DATE_FORMAT('2010-01-01','%W %a %M %b'), DAYNAME('2010-05-05'),MONTHNAME('2010-12-12');
DATE_FORMAT('2010-01-01','%W %a %M %b')	DAYNAME('2010-05-05')	MONTHNAME('2010-12-12')
الجمعة ج يناير ينا	الأربعاء	ديسمبر
SET @@session.lc_time_names = 'es_ES';
SELECT DATE_FORMAT('2010-01-01','%W %a %M %b'), DAYNAME('2010-05-05'),MONTHNAME('2010-12-12');
DATE_FORMAT('2010-01-01','%W %a %M %b')	DAYNAME('2010-05-05')	MONTHNAME('2010-12-12')
viernes vie enero ene	miércoles	diciembre
SET @@session.lc_time_names = 'fi_FI';
SELECT DATE_FORMAT('2010-01-01','%W %a %M %b'), DAYNAME('2010-05-05'),MONTHNAME('2010-12-12');
DATE_FORMAT('2010-01-01','%W %a %M %b')	DAYNAME('2010-05-05')	MONTHNAME('2010-12-12')
perjantai pe tammikuu tammi 	keskiviikko	joulukuu
SET @@session.lc_time_names = 'gu_IN';
SELECT DATE_FORMAT('2010-01-01','%W %a %M %b'), DAYNAME('2010-05-05'),MONTHNAME('2010-12-12');
DATE_FORMAT('2010-01-01','%W %a %M %b')	DAYNAME('2010-05-05')	MONTHNAME('2010-12-12')
શુક્રવાર શુક્ર જાન્યુઆરી જાન	બુધવાર	ડિસેમ્બર
SET @@session.lc_time_names = 'it_IT';
SELECT DATE_FORMAT('2010-01-01','%W %a %M %b'), DAYNAME('2010-05-05'),MONTHNAME('2010-12-12');
DATE_FORMAT('2010-01-01','%W %a %M %b')	DAYNAME('2010-05-05')	MONTHNAME('2010-12-12')
venerdì ven gennaio gen	mercoledì	dicembre
SET @@session.lc_time_names = 'pt_BR';
SELECT DATE_FORMAT('2010-01-01','%W %a %M %b'), DAYNAME('2010-05-05'),MONTHNAME('2010-12-12');
DATE_FORMAT('2010-01-01','%W %a %M %b')	DAYNAME('2010-05-05')	MONTHNAME('2010-12-12')
sexta Sex janeiro Jan	quarta	dezembro
SET @@session.lc_time_names = 'ur_PK';
SELECT DATE_FORMAT('2010-01-01','%W %a %M %b'), DAYNAME('2010-05-05'),MONTHNAME('2010-12-12');
DATE_FORMAT('2010-01-01','%W %a %M %b')	DAYNAME('2010-05-05')	MONTHNAME('2010-12-12')
جمعه جمعه جنوري جنوري	بدھ	دسمبر
SET @@global.lc_time_names = "en_US";
