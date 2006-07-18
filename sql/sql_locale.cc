/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  The beginnings of locale(7) support.
  Sponsored for subset of LC_TIME support,  WorkLog entry 2928, -- Josh Chamas

  !! This file is built from my_locale.pl !!
*/

#include "mysql_priv.h"


MY_LOCALE *my_locale_by_name(const char *name)
{
  MY_LOCALE **locale;
  for( locale= my_locales; *locale != NULL; locale++) 
  {
    if(!strcmp((*locale)->name, name))
      return *locale;
  }
  return NULL;
}

/***** LOCALE BEGIN ar_AE: Arabic - United Arab Emirates *****/
static const char *my_locale_month_names_ar_AE[13] = 
 {"يناير","فبراير","مارس","أبريل","مايو","يونيو","يوليو","أغسطس","سبتمبر","أكتوبر","نوفمبر","ديسمبر", NullS };
static const char *my_locale_ab_month_names_ar_AE[13] = 
 {"ينا","فبر","مار","أبر","ماي","يون","يول","أغس","سبت","أكت","نوف","ديس", NullS };
static const char *my_locale_day_names_ar_AE[8] = 
 {"الاثنين","الثلاثاء","الأربعاء","الخميس","الجمعة","السبت ","الأحد", NullS };
static const char *my_locale_ab_day_names_ar_AE[8] = 
 {"ن","ث","ر","خ","ج","س","ح", NullS };
static TYPELIB my_locale_typelib_month_names_ar_AE = 
 { array_elements(my_locale_month_names_ar_AE)-1, "", my_locale_month_names_ar_AE, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ar_AE = 
 { array_elements(my_locale_ab_month_names_ar_AE)-1, "", my_locale_ab_month_names_ar_AE, NULL };
static TYPELIB my_locale_typelib_day_names_ar_AE = 
 { array_elements(my_locale_day_names_ar_AE)-1, "", my_locale_day_names_ar_AE, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ar_AE = 
 { array_elements(my_locale_ab_day_names_ar_AE)-1, "", my_locale_ab_day_names_ar_AE, NULL };
MY_LOCALE my_locale_ar_AE=
 { "ar_AE", "Arabic - United Arab Emirates", FALSE, &my_locale_typelib_month_names_ar_AE, &my_locale_typelib_ab_month_names_ar_AE, &my_locale_typelib_day_names_ar_AE, &my_locale_typelib_ab_day_names_ar_AE };
/***** LOCALE END ar_AE *****/

/***** LOCALE BEGIN ar_BH: Arabic - Bahrain *****/
static const char *my_locale_month_names_ar_BH[13] = 
 {"يناير","فبراير","مارس","أبريل","مايو","يونيو","يوليو","أغسطس","سبتمبر","أكتوبر","نوفمبر","ديسمبر", NullS };
static const char *my_locale_ab_month_names_ar_BH[13] = 
 {"ينا","فبر","مار","أبر","ماي","يون","يول","أغس","سبت","أكت","نوف","ديس", NullS };
static const char *my_locale_day_names_ar_BH[8] = 
 {"الاثنين","الثلاثاء","الأربعاء","الخميس","الجمعة","السبت","الأحد", NullS };
static const char *my_locale_ab_day_names_ar_BH[8] = 
 {"ن","ث","ر","خ","ج","س","ح", NullS };
static TYPELIB my_locale_typelib_month_names_ar_BH = 
 { array_elements(my_locale_month_names_ar_BH)-1, "", my_locale_month_names_ar_BH, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ar_BH = 
 { array_elements(my_locale_ab_month_names_ar_BH)-1, "", my_locale_ab_month_names_ar_BH, NULL };
static TYPELIB my_locale_typelib_day_names_ar_BH = 
 { array_elements(my_locale_day_names_ar_BH)-1, "", my_locale_day_names_ar_BH, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ar_BH = 
 { array_elements(my_locale_ab_day_names_ar_BH)-1, "", my_locale_ab_day_names_ar_BH, NULL };
MY_LOCALE my_locale_ar_BH=
 { "ar_BH", "Arabic - Bahrain", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_BH *****/

/***** LOCALE BEGIN ar_JO: Arabic - Jordan *****/
static const char *my_locale_month_names_ar_JO[13] = 
 {"كانون الثاني","شباط","آذار","نيسان","نوار","حزيران","تموز","آب","أيلول","تشرين الأول","تشرين الثاني","كانون الأول", NullS };
static const char *my_locale_ab_month_names_ar_JO[13] = 
 {"كانون الثاني","شباط","آذار","نيسان","نوار","حزيران","تموز","آب","أيلول","تشرين الأول","تشرين الثاني","كانون الأول", NullS };
static const char *my_locale_day_names_ar_JO[8] = 
 {"الاثنين","الثلاثاء","الأربعاء","الخميس","الجمعة","السبت","الأحد", NullS };
static const char *my_locale_ab_day_names_ar_JO[8] = 
 {"الاثنين","الثلاثاء","الأربعاء","الخميس","الجمعة","السبت","الأحد", NullS };
static TYPELIB my_locale_typelib_month_names_ar_JO = 
 { array_elements(my_locale_month_names_ar_JO)-1, "", my_locale_month_names_ar_JO, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ar_JO = 
 { array_elements(my_locale_ab_month_names_ar_JO)-1, "", my_locale_ab_month_names_ar_JO, NULL };
static TYPELIB my_locale_typelib_day_names_ar_JO = 
 { array_elements(my_locale_day_names_ar_JO)-1, "", my_locale_day_names_ar_JO, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ar_JO = 
 { array_elements(my_locale_ab_day_names_ar_JO)-1, "", my_locale_ab_day_names_ar_JO, NULL };
MY_LOCALE my_locale_ar_JO=
 { "ar_JO", "Arabic - Jordan", FALSE, &my_locale_typelib_month_names_ar_JO, &my_locale_typelib_ab_month_names_ar_JO, &my_locale_typelib_day_names_ar_JO, &my_locale_typelib_ab_day_names_ar_JO };
/***** LOCALE END ar_JO *****/

/***** LOCALE BEGIN ar_SA: Arabic - Saudi Arabia *****/
static const char *my_locale_month_names_ar_SA[13] = 
 {"كانون الثاني","شباط","آذار","نيسـان","أيار","حزيران","تـمـوز","آب","أيلول","تشرين الأول","تشرين الثاني","كانون الأول", NullS };
static const char *my_locale_ab_month_names_ar_SA[13] = 
 {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec", NullS };
static const char *my_locale_day_names_ar_SA[8] = 
 {"الإثنين","الثلاثاء","الأربعاء","الخميس","الجمعـة","السبت","الأحد", NullS };
static const char *my_locale_ab_day_names_ar_SA[8] = 
 {"Mon","Tue","Wed","Thu","Fri","Sat","Sun", NullS };
static TYPELIB my_locale_typelib_month_names_ar_SA = 
 { array_elements(my_locale_month_names_ar_SA)-1, "", my_locale_month_names_ar_SA, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ar_SA = 
 { array_elements(my_locale_ab_month_names_ar_SA)-1, "", my_locale_ab_month_names_ar_SA, NULL };
static TYPELIB my_locale_typelib_day_names_ar_SA = 
 { array_elements(my_locale_day_names_ar_SA)-1, "", my_locale_day_names_ar_SA, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ar_SA = 
 { array_elements(my_locale_ab_day_names_ar_SA)-1, "", my_locale_ab_day_names_ar_SA, NULL };
MY_LOCALE my_locale_ar_SA=
 { "ar_SA", "Arabic - Saudi Arabia", FALSE, &my_locale_typelib_month_names_ar_SA, &my_locale_typelib_ab_month_names_ar_SA, &my_locale_typelib_day_names_ar_SA, &my_locale_typelib_ab_day_names_ar_SA };
/***** LOCALE END ar_SA *****/

/***** LOCALE BEGIN ar_SY: Arabic - Syria *****/
static const char *my_locale_month_names_ar_SY[13] = 
 {"كانون الثاني","شباط","آذار","نيسان","نواران","حزير","تموز","آب","أيلول","تشرين الأول","تشرين الثاني","كانون الأول", NullS };
static const char *my_locale_ab_month_names_ar_SY[13] = 
 {"كانون الثاني","شباط","آذار","نيسان","نوار","حزيران","تموز","آب","أيلول","تشرين الأول","تشرين الثاني","كانون الأول", NullS };
static const char *my_locale_day_names_ar_SY[8] = 
 {"الاثنين","الثلاثاء","الأربعاء","الخميس","الجمعة","السبت","الأحد", NullS };
static const char *my_locale_ab_day_names_ar_SY[8] = 
 {"الاثنين","الثلاثاء","الأربعاء","الخميس","الجمعة","السبت","الأحد", NullS };
static TYPELIB my_locale_typelib_month_names_ar_SY = 
 { array_elements(my_locale_month_names_ar_SY)-1, "", my_locale_month_names_ar_SY, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ar_SY = 
 { array_elements(my_locale_ab_month_names_ar_SY)-1, "", my_locale_ab_month_names_ar_SY, NULL };
static TYPELIB my_locale_typelib_day_names_ar_SY = 
 { array_elements(my_locale_day_names_ar_SY)-1, "", my_locale_day_names_ar_SY, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ar_SY = 
 { array_elements(my_locale_ab_day_names_ar_SY)-1, "", my_locale_ab_day_names_ar_SY, NULL };
MY_LOCALE my_locale_ar_SY=
 { "ar_SY", "Arabic - Syria", FALSE, &my_locale_typelib_month_names_ar_SY, &my_locale_typelib_ab_month_names_ar_SY, &my_locale_typelib_day_names_ar_SY, &my_locale_typelib_ab_day_names_ar_SY };
/***** LOCALE END ar_SY *****/

/***** LOCALE BEGIN be_BY: Belarusian - Belarus *****/
static const char *my_locale_month_names_be_BY[13] = 
 {"Студзень","Люты","Сакавік","Красавік","Травень","Чэрвень","Ліпень","Жнівень","Верасень","Кастрычнік","Лістапад","Снежань", NullS };
static const char *my_locale_ab_month_names_be_BY[13] = 
 {"Стд","Лют","Сак","Крс","Тра","Чэр","Ліп","Жнв","Врс","Кст","Ліс","Снж", NullS };
static const char *my_locale_day_names_be_BY[8] = 
 {"Панядзелак","Аўторак","Серада","Чацвер","Пятніца","Субота","Нядзеля", NullS };
static const char *my_locale_ab_day_names_be_BY[8] = 
 {"Пан","Аўт","Срд","Чцв","Пят","Суб","Няд", NullS };
static TYPELIB my_locale_typelib_month_names_be_BY = 
 { array_elements(my_locale_month_names_be_BY)-1, "", my_locale_month_names_be_BY, NULL };
static TYPELIB my_locale_typelib_ab_month_names_be_BY = 
 { array_elements(my_locale_ab_month_names_be_BY)-1, "", my_locale_ab_month_names_be_BY, NULL };
static TYPELIB my_locale_typelib_day_names_be_BY = 
 { array_elements(my_locale_day_names_be_BY)-1, "", my_locale_day_names_be_BY, NULL };
static TYPELIB my_locale_typelib_ab_day_names_be_BY = 
 { array_elements(my_locale_ab_day_names_be_BY)-1, "", my_locale_ab_day_names_be_BY, NULL };
MY_LOCALE my_locale_be_BY=
 { "be_BY", "Belarusian - Belarus", FALSE, &my_locale_typelib_month_names_be_BY, &my_locale_typelib_ab_month_names_be_BY, &my_locale_typelib_day_names_be_BY, &my_locale_typelib_ab_day_names_be_BY };
/***** LOCALE END be_BY *****/

/***** LOCALE BEGIN bg_BG: Bulgarian - Bulgaria *****/
static const char *my_locale_month_names_bg_BG[13] = 
 {"януари","февруари","март","април","май","юни","юли","август","септември","октомври","ноември","декември", NullS };
static const char *my_locale_ab_month_names_bg_BG[13] = 
 {"яну","фев","мар","апр","май","юни","юли","авг","сеп","окт","ное","дек", NullS };
static const char *my_locale_day_names_bg_BG[8] = 
 {"понеделник","вторник","сряда","четвъртък","петък","събота","неделя", NullS };
static const char *my_locale_ab_day_names_bg_BG[8] = 
 {"пн","вт","ср","чт","пт","сб","нд", NullS };
static TYPELIB my_locale_typelib_month_names_bg_BG = 
 { array_elements(my_locale_month_names_bg_BG)-1, "", my_locale_month_names_bg_BG, NULL };
static TYPELIB my_locale_typelib_ab_month_names_bg_BG = 
 { array_elements(my_locale_ab_month_names_bg_BG)-1, "", my_locale_ab_month_names_bg_BG, NULL };
static TYPELIB my_locale_typelib_day_names_bg_BG = 
 { array_elements(my_locale_day_names_bg_BG)-1, "", my_locale_day_names_bg_BG, NULL };
static TYPELIB my_locale_typelib_ab_day_names_bg_BG = 
 { array_elements(my_locale_ab_day_names_bg_BG)-1, "", my_locale_ab_day_names_bg_BG, NULL };
MY_LOCALE my_locale_bg_BG=
 { "bg_BG", "Bulgarian - Bulgaria", FALSE, &my_locale_typelib_month_names_bg_BG, &my_locale_typelib_ab_month_names_bg_BG, &my_locale_typelib_day_names_bg_BG, &my_locale_typelib_ab_day_names_bg_BG };
/***** LOCALE END bg_BG *****/

/***** LOCALE BEGIN ca_ES: Catalan - Catalan *****/
static const char *my_locale_month_names_ca_ES[13] = 
 {"gener","febrer","març","abril","maig","juny","juliol","agost","setembre","octubre","novembre","desembre", NullS };
static const char *my_locale_ab_month_names_ca_ES[13] = 
 {"gen","feb","mar","abr","mai","jun","jul","ago","set","oct","nov","des", NullS };
static const char *my_locale_day_names_ca_ES[8] = 
 {"dilluns","dimarts","dimecres","dijous","divendres","dissabte","diumenge", NullS };
static const char *my_locale_ab_day_names_ca_ES[8] = 
 {"dl","dt","dc","dj","dv","ds","dg", NullS };
static TYPELIB my_locale_typelib_month_names_ca_ES = 
 { array_elements(my_locale_month_names_ca_ES)-1, "", my_locale_month_names_ca_ES, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ca_ES = 
 { array_elements(my_locale_ab_month_names_ca_ES)-1, "", my_locale_ab_month_names_ca_ES, NULL };
static TYPELIB my_locale_typelib_day_names_ca_ES = 
 { array_elements(my_locale_day_names_ca_ES)-1, "", my_locale_day_names_ca_ES, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ca_ES = 
 { array_elements(my_locale_ab_day_names_ca_ES)-1, "", my_locale_ab_day_names_ca_ES, NULL };
MY_LOCALE my_locale_ca_ES=
 { "ca_ES", "Catalan - Catalan", FALSE, &my_locale_typelib_month_names_ca_ES, &my_locale_typelib_ab_month_names_ca_ES, &my_locale_typelib_day_names_ca_ES, &my_locale_typelib_ab_day_names_ca_ES };
/***** LOCALE END ca_ES *****/

/***** LOCALE BEGIN cs_CZ: Czech - Czech Republic *****/
static const char *my_locale_month_names_cs_CZ[13] = 
 {"leden","únor","březen","duben","květen","červen","červenec","srpen","září","říjen","listopad","prosinec", NullS };
static const char *my_locale_ab_month_names_cs_CZ[13] = 
 {"led","úno","bře","dub","kvě","čen","čec","srp","zář","říj","lis","pro", NullS };
static const char *my_locale_day_names_cs_CZ[8] = 
 {"Pondělí","Úterý","Středa","Čtvrtek","Pátek","Sobota","Neděle", NullS };
static const char *my_locale_ab_day_names_cs_CZ[8] = 
 {"Po","Út","St","Čt","Pá","So","Ne", NullS };
static TYPELIB my_locale_typelib_month_names_cs_CZ = 
 { array_elements(my_locale_month_names_cs_CZ)-1, "", my_locale_month_names_cs_CZ, NULL };
static TYPELIB my_locale_typelib_ab_month_names_cs_CZ = 
 { array_elements(my_locale_ab_month_names_cs_CZ)-1, "", my_locale_ab_month_names_cs_CZ, NULL };
static TYPELIB my_locale_typelib_day_names_cs_CZ = 
 { array_elements(my_locale_day_names_cs_CZ)-1, "", my_locale_day_names_cs_CZ, NULL };
static TYPELIB my_locale_typelib_ab_day_names_cs_CZ = 
 { array_elements(my_locale_ab_day_names_cs_CZ)-1, "", my_locale_ab_day_names_cs_CZ, NULL };
MY_LOCALE my_locale_cs_CZ=
 { "cs_CZ", "Czech - Czech Republic", FALSE, &my_locale_typelib_month_names_cs_CZ, &my_locale_typelib_ab_month_names_cs_CZ, &my_locale_typelib_day_names_cs_CZ, &my_locale_typelib_ab_day_names_cs_CZ };
/***** LOCALE END cs_CZ *****/

/***** LOCALE BEGIN da_DK: Danish - Denmark *****/
static const char *my_locale_month_names_da_DK[13] = 
 {"januar","februar","marts","april","maj","juni","juli","august","september","oktober","november","december", NullS };
static const char *my_locale_ab_month_names_da_DK[13] = 
 {"jan","feb","mar","apr","maj","jun","jul","aug","sep","okt","nov","dec", NullS };
static const char *my_locale_day_names_da_DK[8] = 
 {"mandag","tirsdag","onsdag","torsdag","fredag","lørdag","søndag", NullS };
static const char *my_locale_ab_day_names_da_DK[8] = 
 {"man","tir","ons","tor","fre","lør","søn", NullS };
static TYPELIB my_locale_typelib_month_names_da_DK = 
 { array_elements(my_locale_month_names_da_DK)-1, "", my_locale_month_names_da_DK, NULL };
static TYPELIB my_locale_typelib_ab_month_names_da_DK = 
 { array_elements(my_locale_ab_month_names_da_DK)-1, "", my_locale_ab_month_names_da_DK, NULL };
static TYPELIB my_locale_typelib_day_names_da_DK = 
 { array_elements(my_locale_day_names_da_DK)-1, "", my_locale_day_names_da_DK, NULL };
static TYPELIB my_locale_typelib_ab_day_names_da_DK = 
 { array_elements(my_locale_ab_day_names_da_DK)-1, "", my_locale_ab_day_names_da_DK, NULL };
MY_LOCALE my_locale_da_DK=
 { "da_DK", "Danish - Denmark", FALSE, &my_locale_typelib_month_names_da_DK, &my_locale_typelib_ab_month_names_da_DK, &my_locale_typelib_day_names_da_DK, &my_locale_typelib_ab_day_names_da_DK };
/***** LOCALE END da_DK *****/

/***** LOCALE BEGIN de_AT: German - Austria *****/
static const char *my_locale_month_names_de_AT[13] = 
 {"Jänner","Feber","März","April","Mai","Juni","Juli","August","September","Oktober","November","Dezember", NullS };
static const char *my_locale_ab_month_names_de_AT[13] = 
 {"Jän","Feb","Mär","Apr","Mai","Jun","Jul","Aug","Sep","Okt","Nov","Dez", NullS };
static const char *my_locale_day_names_de_AT[8] = 
 {"Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag","Sonntag", NullS };
static const char *my_locale_ab_day_names_de_AT[8] = 
 {"Mon","Die","Mit","Don","Fre","Sam","Son", NullS };
static TYPELIB my_locale_typelib_month_names_de_AT = 
 { array_elements(my_locale_month_names_de_AT)-1, "", my_locale_month_names_de_AT, NULL };
static TYPELIB my_locale_typelib_ab_month_names_de_AT = 
 { array_elements(my_locale_ab_month_names_de_AT)-1, "", my_locale_ab_month_names_de_AT, NULL };
static TYPELIB my_locale_typelib_day_names_de_AT = 
 { array_elements(my_locale_day_names_de_AT)-1, "", my_locale_day_names_de_AT, NULL };
static TYPELIB my_locale_typelib_ab_day_names_de_AT = 
 { array_elements(my_locale_ab_day_names_de_AT)-1, "", my_locale_ab_day_names_de_AT, NULL };
MY_LOCALE my_locale_de_AT=
 { "de_AT", "German - Austria", FALSE, &my_locale_typelib_month_names_de_AT, &my_locale_typelib_ab_month_names_de_AT, &my_locale_typelib_day_names_de_AT, &my_locale_typelib_ab_day_names_de_AT };
/***** LOCALE END de_AT *****/

/***** LOCALE BEGIN de_DE: German - Germany *****/
static const char *my_locale_month_names_de_DE[13] = 
 {"Januar","Februar","März","April","Mai","Juni","Juli","August","September","Oktober","November","Dezember", NullS };
static const char *my_locale_ab_month_names_de_DE[13] = 
 {"Jan","Feb","Mär","Apr","Mai","Jun","Jul","Aug","Sep","Okt","Nov","Dez", NullS };
static const char *my_locale_day_names_de_DE[8] = 
 {"Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag","Sonntag", NullS };
static const char *my_locale_ab_day_names_de_DE[8] = 
 {"Mo","Di","Mi","Do","Fr","Sa","So", NullS };
static TYPELIB my_locale_typelib_month_names_de_DE = 
 { array_elements(my_locale_month_names_de_DE)-1, "", my_locale_month_names_de_DE, NULL };
static TYPELIB my_locale_typelib_ab_month_names_de_DE = 
 { array_elements(my_locale_ab_month_names_de_DE)-1, "", my_locale_ab_month_names_de_DE, NULL };
static TYPELIB my_locale_typelib_day_names_de_DE = 
 { array_elements(my_locale_day_names_de_DE)-1, "", my_locale_day_names_de_DE, NULL };
static TYPELIB my_locale_typelib_ab_day_names_de_DE = 
 { array_elements(my_locale_ab_day_names_de_DE)-1, "", my_locale_ab_day_names_de_DE, NULL };
MY_LOCALE my_locale_de_DE=
 { "de_DE", "German - Germany", FALSE, &my_locale_typelib_month_names_de_DE, &my_locale_typelib_ab_month_names_de_DE, &my_locale_typelib_day_names_de_DE, &my_locale_typelib_ab_day_names_de_DE };
/***** LOCALE END de_DE *****/

/***** LOCALE BEGIN en_US: English - United States *****/
static const char *my_locale_month_names_en_US[13] = 
 {"January","February","March","April","May","June","July","August","September","October","November","December", NullS };
static const char *my_locale_ab_month_names_en_US[13] = 
 {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec", NullS };
static const char *my_locale_day_names_en_US[8] = 
 {"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday", NullS };
static const char *my_locale_ab_day_names_en_US[8] = 
 {"Mon","Tue","Wed","Thu","Fri","Sat","Sun", NullS };
static TYPELIB my_locale_typelib_month_names_en_US = 
 { array_elements(my_locale_month_names_en_US)-1, "", my_locale_month_names_en_US, NULL };
static TYPELIB my_locale_typelib_ab_month_names_en_US = 
 { array_elements(my_locale_ab_month_names_en_US)-1, "", my_locale_ab_month_names_en_US, NULL };
static TYPELIB my_locale_typelib_day_names_en_US = 
 { array_elements(my_locale_day_names_en_US)-1, "", my_locale_day_names_en_US, NULL };
static TYPELIB my_locale_typelib_ab_day_names_en_US = 
 { array_elements(my_locale_ab_day_names_en_US)-1, "", my_locale_ab_day_names_en_US, NULL };
MY_LOCALE my_locale_en_US=
 { "en_US", "English - United States", TRUE, &my_locale_typelib_month_names_en_US, &my_locale_typelib_ab_month_names_en_US, &my_locale_typelib_day_names_en_US, &my_locale_typelib_ab_day_names_en_US };
/***** LOCALE END en_US *****/

/***** LOCALE BEGIN es_ES: Spanish - Spain *****/
static const char *my_locale_month_names_es_ES[13] = 
 {"enero","febrero","marzo","abril","mayo","junio","julio","agosto","septiembre","octubre","noviembre","diciembre", NullS };
static const char *my_locale_ab_month_names_es_ES[13] = 
 {"ene","feb","mar","abr","may","jun","jul","ago","sep","oct","nov","dic", NullS };
static const char *my_locale_day_names_es_ES[8] = 
 {"lunes","martes","miércoles","jueves","viernes","sábado","domingo", NullS };
static const char *my_locale_ab_day_names_es_ES[8] = 
 {"lun","mar","mié","jue","vie","sáb","dom", NullS };
static TYPELIB my_locale_typelib_month_names_es_ES = 
 { array_elements(my_locale_month_names_es_ES)-1, "", my_locale_month_names_es_ES, NULL };
static TYPELIB my_locale_typelib_ab_month_names_es_ES = 
 { array_elements(my_locale_ab_month_names_es_ES)-1, "", my_locale_ab_month_names_es_ES, NULL };
static TYPELIB my_locale_typelib_day_names_es_ES = 
 { array_elements(my_locale_day_names_es_ES)-1, "", my_locale_day_names_es_ES, NULL };
static TYPELIB my_locale_typelib_ab_day_names_es_ES = 
 { array_elements(my_locale_ab_day_names_es_ES)-1, "", my_locale_ab_day_names_es_ES, NULL };
MY_LOCALE my_locale_es_ES=
 { "es_ES", "Spanish - Spain", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_ES *****/

/***** LOCALE BEGIN et_EE: Estonian - Estonia *****/
static const char *my_locale_month_names_et_EE[13] = 
 {"jaanuar","veebruar","märts","aprill","mai","juuni","juuli","august","september","oktoober","november","detsember", NullS };
static const char *my_locale_ab_month_names_et_EE[13] = 
 {"jaan ","veebr","märts","apr  ","mai  ","juuni","juuli","aug  ","sept ","okt  ","nov  ","dets ", NullS };
static const char *my_locale_day_names_et_EE[8] = 
 {"esmaspäev","teisipäev","kolmapäev","neljapäev","reede","laupäev","pühapäev", NullS };
static const char *my_locale_ab_day_names_et_EE[8] = 
 {"E","T","K","N","R","L","P", NullS };
static TYPELIB my_locale_typelib_month_names_et_EE = 
 { array_elements(my_locale_month_names_et_EE)-1, "", my_locale_month_names_et_EE, NULL };
static TYPELIB my_locale_typelib_ab_month_names_et_EE = 
 { array_elements(my_locale_ab_month_names_et_EE)-1, "", my_locale_ab_month_names_et_EE, NULL };
static TYPELIB my_locale_typelib_day_names_et_EE = 
 { array_elements(my_locale_day_names_et_EE)-1, "", my_locale_day_names_et_EE, NULL };
static TYPELIB my_locale_typelib_ab_day_names_et_EE = 
 { array_elements(my_locale_ab_day_names_et_EE)-1, "", my_locale_ab_day_names_et_EE, NULL };
MY_LOCALE my_locale_et_EE=
 { "et_EE", "Estonian - Estonia", FALSE, &my_locale_typelib_month_names_et_EE, &my_locale_typelib_ab_month_names_et_EE, &my_locale_typelib_day_names_et_EE, &my_locale_typelib_ab_day_names_et_EE };
/***** LOCALE END et_EE *****/

/***** LOCALE BEGIN eu_ES: Basque - Basque *****/
static const char *my_locale_month_names_eu_ES[13] = 
 {"urtarrila","otsaila","martxoa","apirila","maiatza","ekaina","uztaila","abuztua","iraila","urria","azaroa","abendua", NullS };
static const char *my_locale_ab_month_names_eu_ES[13] = 
 {"urt","ots","mar","api","mai","eka","uzt","abu","ira","urr","aza","abe", NullS };
static const char *my_locale_day_names_eu_ES[8] = 
 {"astelehena","asteartea","asteazkena","osteguna","ostirala","larunbata","igandea", NullS };
static const char *my_locale_ab_day_names_eu_ES[8] = 
 {"al.","ar.","az.","og.","or.","lr.","ig.", NullS };
static TYPELIB my_locale_typelib_month_names_eu_ES = 
 { array_elements(my_locale_month_names_eu_ES)-1, "", my_locale_month_names_eu_ES, NULL };
static TYPELIB my_locale_typelib_ab_month_names_eu_ES = 
 { array_elements(my_locale_ab_month_names_eu_ES)-1, "", my_locale_ab_month_names_eu_ES, NULL };
static TYPELIB my_locale_typelib_day_names_eu_ES = 
 { array_elements(my_locale_day_names_eu_ES)-1, "", my_locale_day_names_eu_ES, NULL };
static TYPELIB my_locale_typelib_ab_day_names_eu_ES = 
 { array_elements(my_locale_ab_day_names_eu_ES)-1, "", my_locale_ab_day_names_eu_ES, NULL };
MY_LOCALE my_locale_eu_ES=
 { "eu_ES", "Basque - Basque", TRUE, &my_locale_typelib_month_names_eu_ES, &my_locale_typelib_ab_month_names_eu_ES, &my_locale_typelib_day_names_eu_ES, &my_locale_typelib_ab_day_names_eu_ES };
/***** LOCALE END eu_ES *****/

/***** LOCALE BEGIN fi_FI: Finnish - Finland *****/
static const char *my_locale_month_names_fi_FI[13] = 
 {"tammikuu","helmikuu","maaliskuu","huhtikuu","toukokuu","kesäkuu","heinäkuu","elokuu","syyskuu","lokakuu","marraskuu","joulukuu", NullS };
static const char *my_locale_ab_month_names_fi_FI[13] = 
 {"tammi ","helmi ","maalis","huhti ","touko ","kesä  ","heinä ","elo   ","syys  ","loka  ","marras","joulu ", NullS };
static const char *my_locale_day_names_fi_FI[8] = 
 {"maanantai","tiistai","keskiviikko","torstai","perjantai","lauantai","sunnuntai", NullS };
static const char *my_locale_ab_day_names_fi_FI[8] = 
 {"ma","ti","ke","to","pe","la","su", NullS };
static TYPELIB my_locale_typelib_month_names_fi_FI = 
 { array_elements(my_locale_month_names_fi_FI)-1, "", my_locale_month_names_fi_FI, NULL };
static TYPELIB my_locale_typelib_ab_month_names_fi_FI = 
 { array_elements(my_locale_ab_month_names_fi_FI)-1, "", my_locale_ab_month_names_fi_FI, NULL };
static TYPELIB my_locale_typelib_day_names_fi_FI = 
 { array_elements(my_locale_day_names_fi_FI)-1, "", my_locale_day_names_fi_FI, NULL };
static TYPELIB my_locale_typelib_ab_day_names_fi_FI = 
 { array_elements(my_locale_ab_day_names_fi_FI)-1, "", my_locale_ab_day_names_fi_FI, NULL };
MY_LOCALE my_locale_fi_FI=
 { "fi_FI", "Finnish - Finland", FALSE, &my_locale_typelib_month_names_fi_FI, &my_locale_typelib_ab_month_names_fi_FI, &my_locale_typelib_day_names_fi_FI, &my_locale_typelib_ab_day_names_fi_FI };
/***** LOCALE END fi_FI *****/

/***** LOCALE BEGIN fo_FO: Faroese - Faroe Islands *****/
static const char *my_locale_month_names_fo_FO[13] = 
 {"januar","februar","mars","apríl","mai","juni","juli","august","september","oktober","november","desember", NullS };
static const char *my_locale_ab_month_names_fo_FO[13] = 
 {"jan","feb","mar","apr","mai","jun","jul","aug","sep","okt","nov","des", NullS };
static const char *my_locale_day_names_fo_FO[8] = 
 {"mánadagur","týsdagur","mikudagur","hósdagur","fríggjadagur","leygardagur","sunnudagur", NullS };
static const char *my_locale_ab_day_names_fo_FO[8] = 
 {"mán","týs","mik","hós","frí","ley","sun", NullS };
static TYPELIB my_locale_typelib_month_names_fo_FO = 
 { array_elements(my_locale_month_names_fo_FO)-1, "", my_locale_month_names_fo_FO, NULL };
static TYPELIB my_locale_typelib_ab_month_names_fo_FO = 
 { array_elements(my_locale_ab_month_names_fo_FO)-1, "", my_locale_ab_month_names_fo_FO, NULL };
static TYPELIB my_locale_typelib_day_names_fo_FO = 
 { array_elements(my_locale_day_names_fo_FO)-1, "", my_locale_day_names_fo_FO, NULL };
static TYPELIB my_locale_typelib_ab_day_names_fo_FO = 
 { array_elements(my_locale_ab_day_names_fo_FO)-1, "", my_locale_ab_day_names_fo_FO, NULL };
MY_LOCALE my_locale_fo_FO=
 { "fo_FO", "Faroese - Faroe Islands", FALSE, &my_locale_typelib_month_names_fo_FO, &my_locale_typelib_ab_month_names_fo_FO, &my_locale_typelib_day_names_fo_FO, &my_locale_typelib_ab_day_names_fo_FO };
/***** LOCALE END fo_FO *****/

/***** LOCALE BEGIN fr_FR: French - France *****/
static const char *my_locale_month_names_fr_FR[13] = 
 {"janvier","février","mars","avril","mai","juin","juillet","août","septembre","octobre","novembre","décembre", NullS };
static const char *my_locale_ab_month_names_fr_FR[13] = 
 {"jan","fév","mar","avr","mai","jun","jui","aoû","sep","oct","nov","déc", NullS };
static const char *my_locale_day_names_fr_FR[8] = 
 {"lundi","mardi","mercredi","jeudi","vendredi","samedi","dimanche", NullS };
static const char *my_locale_ab_day_names_fr_FR[8] = 
 {"lun","mar","mer","jeu","ven","sam","dim", NullS };
static TYPELIB my_locale_typelib_month_names_fr_FR = 
 { array_elements(my_locale_month_names_fr_FR)-1, "", my_locale_month_names_fr_FR, NULL };
static TYPELIB my_locale_typelib_ab_month_names_fr_FR = 
 { array_elements(my_locale_ab_month_names_fr_FR)-1, "", my_locale_ab_month_names_fr_FR, NULL };
static TYPELIB my_locale_typelib_day_names_fr_FR = 
 { array_elements(my_locale_day_names_fr_FR)-1, "", my_locale_day_names_fr_FR, NULL };
static TYPELIB my_locale_typelib_ab_day_names_fr_FR = 
 { array_elements(my_locale_ab_day_names_fr_FR)-1, "", my_locale_ab_day_names_fr_FR, NULL };
MY_LOCALE my_locale_fr_FR=
 { "fr_FR", "French - France", FALSE, &my_locale_typelib_month_names_fr_FR, &my_locale_typelib_ab_month_names_fr_FR, &my_locale_typelib_day_names_fr_FR, &my_locale_typelib_ab_day_names_fr_FR };
/***** LOCALE END fr_FR *****/

/***** LOCALE BEGIN gl_ES: Galician - Galician *****/
static const char *my_locale_month_names_gl_ES[13] = 
 {"Xaneiro","Febreiro","Marzo","Abril","Maio","Xuño","Xullo","Agosto","Setembro","Outubro","Novembro","Decembro", NullS };
static const char *my_locale_ab_month_names_gl_ES[13] = 
 {"Xan","Feb","Mar","Abr","Mai","Xuñ","Xul","Ago","Set","Out","Nov","Dec", NullS };
static const char *my_locale_day_names_gl_ES[8] = 
 {"Luns","Martes","Mércores","Xoves","Venres","Sábado","Domingo", NullS };
static const char *my_locale_ab_day_names_gl_ES[8] = 
 {"Lun","Mar","Mér","Xov","Ven","Sáb","Dom", NullS };
static TYPELIB my_locale_typelib_month_names_gl_ES = 
 { array_elements(my_locale_month_names_gl_ES)-1, "", my_locale_month_names_gl_ES, NULL };
static TYPELIB my_locale_typelib_ab_month_names_gl_ES = 
 { array_elements(my_locale_ab_month_names_gl_ES)-1, "", my_locale_ab_month_names_gl_ES, NULL };
static TYPELIB my_locale_typelib_day_names_gl_ES = 
 { array_elements(my_locale_day_names_gl_ES)-1, "", my_locale_day_names_gl_ES, NULL };
static TYPELIB my_locale_typelib_ab_day_names_gl_ES = 
 { array_elements(my_locale_ab_day_names_gl_ES)-1, "", my_locale_ab_day_names_gl_ES, NULL };
MY_LOCALE my_locale_gl_ES=
 { "gl_ES", "Galician - Galician", FALSE, &my_locale_typelib_month_names_gl_ES, &my_locale_typelib_ab_month_names_gl_ES, &my_locale_typelib_day_names_gl_ES, &my_locale_typelib_ab_day_names_gl_ES };
/***** LOCALE END gl_ES *****/

/***** LOCALE BEGIN gu_IN: Gujarati - India *****/
static const char *my_locale_month_names_gu_IN[13] = 
 {"જાન્યુઆરી","ફેબ્રુઆરી","માર્ચ","એપ્રિલ","મે","જુન","જુલાઇ","ઓગસ્ટ","સેપ્ટેમ્બર","ઓક્ટોબર","નવેમ્બર","ડિસેમ્બર", NullS };
static const char *my_locale_ab_month_names_gu_IN[13] = 
 {"જાન","ફેબ","માર","એપ્ર","મે","જુન","જુલ","ઓગ","સેપ્ટ","ઓક્ટ","નોવ","ડિસ", NullS };
static const char *my_locale_day_names_gu_IN[8] = 
 {"સોમવાર","મન્ગળવાર","બુધવાર","ગુરુવાર","શુક્રવાર","શનિવાર","રવિવાર", NullS };
static const char *my_locale_ab_day_names_gu_IN[8] = 
 {"સોમ","મન્ગળ","બુધ","ગુરુ","શુક્ર","શનિ","રવિ", NullS };
static TYPELIB my_locale_typelib_month_names_gu_IN = 
 { array_elements(my_locale_month_names_gu_IN)-1, "", my_locale_month_names_gu_IN, NULL };
static TYPELIB my_locale_typelib_ab_month_names_gu_IN = 
 { array_elements(my_locale_ab_month_names_gu_IN)-1, "", my_locale_ab_month_names_gu_IN, NULL };
static TYPELIB my_locale_typelib_day_names_gu_IN = 
 { array_elements(my_locale_day_names_gu_IN)-1, "", my_locale_day_names_gu_IN, NULL };
static TYPELIB my_locale_typelib_ab_day_names_gu_IN = 
 { array_elements(my_locale_ab_day_names_gu_IN)-1, "", my_locale_ab_day_names_gu_IN, NULL };
MY_LOCALE my_locale_gu_IN=
 { "gu_IN", "Gujarati - India", FALSE, &my_locale_typelib_month_names_gu_IN, &my_locale_typelib_ab_month_names_gu_IN, &my_locale_typelib_day_names_gu_IN, &my_locale_typelib_ab_day_names_gu_IN };
/***** LOCALE END gu_IN *****/

/***** LOCALE BEGIN he_IL: Hebrew - Israel *****/
static const char *my_locale_month_names_he_IL[13] = 
 {"ינואר","פברואר","מרץ","אפריל","מאי","יוני","יולי","אוגוסט","ספטמבר","אוקטובר","נובמבר","דצמבר", NullS };
static const char *my_locale_ab_month_names_he_IL[13] = 
 {"ינו","פבר","מרץ","אפר","מאי","יונ","יול","אוג","ספט","אוק","נוב","דצמ", NullS };
static const char *my_locale_day_names_he_IL[8] = 
 {"שני","שלישי","רביעי","חמישי","שישי","שבת","ראשון", NullS };
static const char *my_locale_ab_day_names_he_IL[8] = 
 {"ב'","ג'","ד'","ה'","ו'","ש'","א'", NullS };
static TYPELIB my_locale_typelib_month_names_he_IL = 
 { array_elements(my_locale_month_names_he_IL)-1, "", my_locale_month_names_he_IL, NULL };
static TYPELIB my_locale_typelib_ab_month_names_he_IL = 
 { array_elements(my_locale_ab_month_names_he_IL)-1, "", my_locale_ab_month_names_he_IL, NULL };
static TYPELIB my_locale_typelib_day_names_he_IL = 
 { array_elements(my_locale_day_names_he_IL)-1, "", my_locale_day_names_he_IL, NULL };
static TYPELIB my_locale_typelib_ab_day_names_he_IL = 
 { array_elements(my_locale_ab_day_names_he_IL)-1, "", my_locale_ab_day_names_he_IL, NULL };
MY_LOCALE my_locale_he_IL=
 { "he_IL", "Hebrew - Israel", FALSE, &my_locale_typelib_month_names_he_IL, &my_locale_typelib_ab_month_names_he_IL, &my_locale_typelib_day_names_he_IL, &my_locale_typelib_ab_day_names_he_IL };
/***** LOCALE END he_IL *****/

/***** LOCALE BEGIN hi_IN: Hindi - India *****/
static const char *my_locale_month_names_hi_IN[13] = 
 {"जनवरी","फ़रवरी","मार्च","अप्रेल","मई","जून","जुलाई","अगस्त","सितम्बर","अक्टूबर","नवम्बर","दिसम्बर", NullS };
static const char *my_locale_ab_month_names_hi_IN[13] = 
 {"जनवरी","फ़रवरी","मार्च","अप्रेल","मई","जून","जुलाई","अगस्त","सितम्बर","अक्टूबर","नवम्बर","दिसम्बर", NullS };
static const char *my_locale_day_names_hi_IN[8] = 
 {"सोमवार ","मंगलवार ","बुधवार ","गुरुवार ","शुक्रवार ","शनिवार ","रविवार ", NullS };
static const char *my_locale_ab_day_names_hi_IN[8] = 
 {"सोम ","मंगल ","बुध ","गुरु ","शुक्र ","शनि ","रवि ", NullS };
static TYPELIB my_locale_typelib_month_names_hi_IN = 
 { array_elements(my_locale_month_names_hi_IN)-1, "", my_locale_month_names_hi_IN, NULL };
static TYPELIB my_locale_typelib_ab_month_names_hi_IN = 
 { array_elements(my_locale_ab_month_names_hi_IN)-1, "", my_locale_ab_month_names_hi_IN, NULL };
static TYPELIB my_locale_typelib_day_names_hi_IN = 
 { array_elements(my_locale_day_names_hi_IN)-1, "", my_locale_day_names_hi_IN, NULL };
static TYPELIB my_locale_typelib_ab_day_names_hi_IN = 
 { array_elements(my_locale_ab_day_names_hi_IN)-1, "", my_locale_ab_day_names_hi_IN, NULL };
MY_LOCALE my_locale_hi_IN=
 { "hi_IN", "Hindi - India", FALSE, &my_locale_typelib_month_names_hi_IN, &my_locale_typelib_ab_month_names_hi_IN, &my_locale_typelib_day_names_hi_IN, &my_locale_typelib_ab_day_names_hi_IN };
/***** LOCALE END hi_IN *****/

/***** LOCALE BEGIN hr_HR: Croatian - Croatia *****/
static const char *my_locale_month_names_hr_HR[13] = 
 {"Siječanj","Veljača","Ožujak","Travanj","Svibanj","Lipanj","Srpanj","Kolovoz","Rujan","Listopad","Studeni","Prosinac", NullS };
static const char *my_locale_ab_month_names_hr_HR[13] = 
 {"Sij","Vel","Ožu","Tra","Svi","Lip","Srp","Kol","Ruj","Lis","Stu","Pro", NullS };
static const char *my_locale_day_names_hr_HR[8] = 
 {"Ponedjeljak","Utorak","Srijeda","Četvrtak","Petak","Subota","Nedjelja", NullS };
static const char *my_locale_ab_day_names_hr_HR[8] = 
 {"Pon","Uto","Sri","Čet","Pet","Sub","Ned", NullS };
static TYPELIB my_locale_typelib_month_names_hr_HR = 
 { array_elements(my_locale_month_names_hr_HR)-1, "", my_locale_month_names_hr_HR, NULL };
static TYPELIB my_locale_typelib_ab_month_names_hr_HR = 
 { array_elements(my_locale_ab_month_names_hr_HR)-1, "", my_locale_ab_month_names_hr_HR, NULL };
static TYPELIB my_locale_typelib_day_names_hr_HR = 
 { array_elements(my_locale_day_names_hr_HR)-1, "", my_locale_day_names_hr_HR, NULL };
static TYPELIB my_locale_typelib_ab_day_names_hr_HR = 
 { array_elements(my_locale_ab_day_names_hr_HR)-1, "", my_locale_ab_day_names_hr_HR, NULL };
MY_LOCALE my_locale_hr_HR=
 { "hr_HR", "Croatian - Croatia", FALSE, &my_locale_typelib_month_names_hr_HR, &my_locale_typelib_ab_month_names_hr_HR, &my_locale_typelib_day_names_hr_HR, &my_locale_typelib_ab_day_names_hr_HR };
/***** LOCALE END hr_HR *****/

/***** LOCALE BEGIN hu_HU: Hungarian - Hungary *****/
static const char *my_locale_month_names_hu_HU[13] = 
 {"január","február","március","április","május","június","július","augusztus","szeptember","október","november","december", NullS };
static const char *my_locale_ab_month_names_hu_HU[13] = 
 {"jan","feb","már","ápr","máj","jún","júl","aug","sze","okt","nov","dec", NullS };
static const char *my_locale_day_names_hu_HU[8] = 
 {"hétfő","kedd","szerda","csütörtök","péntek","szombat","vasárnap", NullS };
static const char *my_locale_ab_day_names_hu_HU[8] = 
 {"h","k","sze","cs","p","szo","v", NullS };
static TYPELIB my_locale_typelib_month_names_hu_HU = 
 { array_elements(my_locale_month_names_hu_HU)-1, "", my_locale_month_names_hu_HU, NULL };
static TYPELIB my_locale_typelib_ab_month_names_hu_HU = 
 { array_elements(my_locale_ab_month_names_hu_HU)-1, "", my_locale_ab_month_names_hu_HU, NULL };
static TYPELIB my_locale_typelib_day_names_hu_HU = 
 { array_elements(my_locale_day_names_hu_HU)-1, "", my_locale_day_names_hu_HU, NULL };
static TYPELIB my_locale_typelib_ab_day_names_hu_HU = 
 { array_elements(my_locale_ab_day_names_hu_HU)-1, "", my_locale_ab_day_names_hu_HU, NULL };
MY_LOCALE my_locale_hu_HU=
 { "hu_HU", "Hungarian - Hungary", FALSE, &my_locale_typelib_month_names_hu_HU, &my_locale_typelib_ab_month_names_hu_HU, &my_locale_typelib_day_names_hu_HU, &my_locale_typelib_ab_day_names_hu_HU };
/***** LOCALE END hu_HU *****/

/***** LOCALE BEGIN id_ID: Indonesian - Indonesia *****/
static const char *my_locale_month_names_id_ID[13] = 
 {"Januari","Pebruari","Maret","April","Mei","Juni","Juli","Agustus","September","Oktober","November","Desember", NullS };
static const char *my_locale_ab_month_names_id_ID[13] = 
 {"Jan","Peb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des", NullS };
static const char *my_locale_day_names_id_ID[8] = 
 {"Senin","Selasa","Rabu","Kamis","Jumat","Sabtu","Minggu", NullS };
static const char *my_locale_ab_day_names_id_ID[8] = 
 {"Sen","Sel","Rab","Kam","Jum","Sab","Min", NullS };
static TYPELIB my_locale_typelib_month_names_id_ID = 
 { array_elements(my_locale_month_names_id_ID)-1, "", my_locale_month_names_id_ID, NULL };
static TYPELIB my_locale_typelib_ab_month_names_id_ID = 
 { array_elements(my_locale_ab_month_names_id_ID)-1, "", my_locale_ab_month_names_id_ID, NULL };
static TYPELIB my_locale_typelib_day_names_id_ID = 
 { array_elements(my_locale_day_names_id_ID)-1, "", my_locale_day_names_id_ID, NULL };
static TYPELIB my_locale_typelib_ab_day_names_id_ID = 
 { array_elements(my_locale_ab_day_names_id_ID)-1, "", my_locale_ab_day_names_id_ID, NULL };
MY_LOCALE my_locale_id_ID=
 { "id_ID", "Indonesian - Indonesia", TRUE, &my_locale_typelib_month_names_id_ID, &my_locale_typelib_ab_month_names_id_ID, &my_locale_typelib_day_names_id_ID, &my_locale_typelib_ab_day_names_id_ID };
/***** LOCALE END id_ID *****/

/***** LOCALE BEGIN is_IS: Icelandic - Iceland *****/
static const char *my_locale_month_names_is_IS[13] = 
 {"janúar","febrúar","mars","apríl","maí","júní","júlí","ágúst","september","október","nóvember","desember", NullS };
static const char *my_locale_ab_month_names_is_IS[13] = 
 {"jan","feb","mar","apr","maí","jún","júl","ágú","sep","okt","nóv","des", NullS };
static const char *my_locale_day_names_is_IS[8] = 
 {"mánudagur","þriðjudagur","miðvikudagur","fimmtudagur","föstudagur","laugardagur","sunnudagur", NullS };
static const char *my_locale_ab_day_names_is_IS[8] = 
 {"mán","þri","mið","fim","fös","lau","sun", NullS };
static TYPELIB my_locale_typelib_month_names_is_IS = 
 { array_elements(my_locale_month_names_is_IS)-1, "", my_locale_month_names_is_IS, NULL };
static TYPELIB my_locale_typelib_ab_month_names_is_IS = 
 { array_elements(my_locale_ab_month_names_is_IS)-1, "", my_locale_ab_month_names_is_IS, NULL };
static TYPELIB my_locale_typelib_day_names_is_IS = 
 { array_elements(my_locale_day_names_is_IS)-1, "", my_locale_day_names_is_IS, NULL };
static TYPELIB my_locale_typelib_ab_day_names_is_IS = 
 { array_elements(my_locale_ab_day_names_is_IS)-1, "", my_locale_ab_day_names_is_IS, NULL };
MY_LOCALE my_locale_is_IS=
 { "is_IS", "Icelandic - Iceland", FALSE, &my_locale_typelib_month_names_is_IS, &my_locale_typelib_ab_month_names_is_IS, &my_locale_typelib_day_names_is_IS, &my_locale_typelib_ab_day_names_is_IS };
/***** LOCALE END is_IS *****/

/***** LOCALE BEGIN it_CH: Italian - Switzerland *****/
static const char *my_locale_month_names_it_CH[13] = 
 {"gennaio","febbraio","marzo","aprile","maggio","giugno","luglio","agosto","settembre","ottobre","novembre","dicembre", NullS };
static const char *my_locale_ab_month_names_it_CH[13] = 
 {"gen","feb","mar","apr","mag","giu","lug","ago","set","ott","nov","dic", NullS };
static const char *my_locale_day_names_it_CH[8] = 
 {"lunedì","martedì","mercoledì","giovedì","venerdì","sabato","domenica", NullS };
static const char *my_locale_ab_day_names_it_CH[8] = 
 {"lun","mar","mer","gio","ven","sab","dom", NullS };
static TYPELIB my_locale_typelib_month_names_it_CH = 
 { array_elements(my_locale_month_names_it_CH)-1, "", my_locale_month_names_it_CH, NULL };
static TYPELIB my_locale_typelib_ab_month_names_it_CH = 
 { array_elements(my_locale_ab_month_names_it_CH)-1, "", my_locale_ab_month_names_it_CH, NULL };
static TYPELIB my_locale_typelib_day_names_it_CH = 
 { array_elements(my_locale_day_names_it_CH)-1, "", my_locale_day_names_it_CH, NULL };
static TYPELIB my_locale_typelib_ab_day_names_it_CH = 
 { array_elements(my_locale_ab_day_names_it_CH)-1, "", my_locale_ab_day_names_it_CH, NULL };
MY_LOCALE my_locale_it_CH=
 { "it_CH", "Italian - Switzerland", FALSE, &my_locale_typelib_month_names_it_CH, &my_locale_typelib_ab_month_names_it_CH, &my_locale_typelib_day_names_it_CH, &my_locale_typelib_ab_day_names_it_CH };
/***** LOCALE END it_CH *****/

/***** LOCALE BEGIN ja_JP: Japanese - Japan *****/
static const char *my_locale_month_names_ja_JP[13] = 
 {"1月","2月","3月","4月","5月","6月","7月","8月","9月","10月","11月","12月", NullS };
static const char *my_locale_ab_month_names_ja_JP[13] = 
 {" 1月"," 2月"," 3月"," 4月"," 5月"," 6月"," 7月"," 8月"," 9月","10月","11月","12月", NullS };
static const char *my_locale_day_names_ja_JP[8] = 
 {"月曜日","火曜日","水曜日","木曜日","金曜日","土曜日","日曜日", NullS };
static const char *my_locale_ab_day_names_ja_JP[8] = 
 {"月","火","水","木","金","土","日", NullS };
static TYPELIB my_locale_typelib_month_names_ja_JP = 
 { array_elements(my_locale_month_names_ja_JP)-1, "", my_locale_month_names_ja_JP, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ja_JP = 
 { array_elements(my_locale_ab_month_names_ja_JP)-1, "", my_locale_ab_month_names_ja_JP, NULL };
static TYPELIB my_locale_typelib_day_names_ja_JP = 
 { array_elements(my_locale_day_names_ja_JP)-1, "", my_locale_day_names_ja_JP, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ja_JP = 
 { array_elements(my_locale_ab_day_names_ja_JP)-1, "", my_locale_ab_day_names_ja_JP, NULL };
MY_LOCALE my_locale_ja_JP=
 { "ja_JP", "Japanese - Japan", FALSE, &my_locale_typelib_month_names_ja_JP, &my_locale_typelib_ab_month_names_ja_JP, &my_locale_typelib_day_names_ja_JP, &my_locale_typelib_ab_day_names_ja_JP };
/***** LOCALE END ja_JP *****/

/***** LOCALE BEGIN ko_KR: Korean - Korea *****/
static const char *my_locale_month_names_ko_KR[13] = 
 {"일월","이월","삼월","사월","오월","유월","칠월","팔월","구월","시월","십일월","십이월", NullS };
static const char *my_locale_ab_month_names_ko_KR[13] = 
 {" 1월"," 2월"," 3월"," 4월"," 5월"," 6월"," 7월"," 8월"," 9월","10월","11월","12월", NullS };
static const char *my_locale_day_names_ko_KR[8] = 
 {"월요일","화요일","수요일","목요일","금요일","토요일","일요일", NullS };
static const char *my_locale_ab_day_names_ko_KR[8] = 
 {"월","화","수","목","금","토","일", NullS };
static TYPELIB my_locale_typelib_month_names_ko_KR = 
 { array_elements(my_locale_month_names_ko_KR)-1, "", my_locale_month_names_ko_KR, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ko_KR = 
 { array_elements(my_locale_ab_month_names_ko_KR)-1, "", my_locale_ab_month_names_ko_KR, NULL };
static TYPELIB my_locale_typelib_day_names_ko_KR = 
 { array_elements(my_locale_day_names_ko_KR)-1, "", my_locale_day_names_ko_KR, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ko_KR = 
 { array_elements(my_locale_ab_day_names_ko_KR)-1, "", my_locale_ab_day_names_ko_KR, NULL };
MY_LOCALE my_locale_ko_KR=
 { "ko_KR", "Korean - Korea", FALSE, &my_locale_typelib_month_names_ko_KR, &my_locale_typelib_ab_month_names_ko_KR, &my_locale_typelib_day_names_ko_KR, &my_locale_typelib_ab_day_names_ko_KR };
/***** LOCALE END ko_KR *****/

/***** LOCALE BEGIN lt_LT: Lithuanian - Lithuania *****/
static const char *my_locale_month_names_lt_LT[13] = 
 {"sausio","vasario","kovo","balandžio","gegužės","birželio","liepos","rugpjūčio","rugsėjo","spalio","lapkričio","gruodžio", NullS };
static const char *my_locale_ab_month_names_lt_LT[13] = 
 {"Sau","Vas","Kov","Bal","Geg","Bir","Lie","Rgp","Rgs","Spa","Lap","Grd", NullS };
static const char *my_locale_day_names_lt_LT[8] = 
 {"Pirmadienis","Antradienis","Trečiadienis","Ketvirtadienis","Penktadienis","Šeštadienis","Sekmadienis", NullS };
static const char *my_locale_ab_day_names_lt_LT[8] = 
 {"Pr","An","Tr","Kt","Pn","Št","Sk", NullS };
static TYPELIB my_locale_typelib_month_names_lt_LT = 
 { array_elements(my_locale_month_names_lt_LT)-1, "", my_locale_month_names_lt_LT, NULL };
static TYPELIB my_locale_typelib_ab_month_names_lt_LT = 
 { array_elements(my_locale_ab_month_names_lt_LT)-1, "", my_locale_ab_month_names_lt_LT, NULL };
static TYPELIB my_locale_typelib_day_names_lt_LT = 
 { array_elements(my_locale_day_names_lt_LT)-1, "", my_locale_day_names_lt_LT, NULL };
static TYPELIB my_locale_typelib_ab_day_names_lt_LT = 
 { array_elements(my_locale_ab_day_names_lt_LT)-1, "", my_locale_ab_day_names_lt_LT, NULL };
MY_LOCALE my_locale_lt_LT=
 { "lt_LT", "Lithuanian - Lithuania", FALSE, &my_locale_typelib_month_names_lt_LT, &my_locale_typelib_ab_month_names_lt_LT, &my_locale_typelib_day_names_lt_LT, &my_locale_typelib_ab_day_names_lt_LT };
/***** LOCALE END lt_LT *****/

/***** LOCALE BEGIN lv_LV: Latvian - Latvia *****/
static const char *my_locale_month_names_lv_LV[13] = 
 {"janvāris","februāris","marts","aprīlis","maijs","jūnijs","jūlijs","augusts","septembris","oktobris","novembris","decembris", NullS };
static const char *my_locale_ab_month_names_lv_LV[13] = 
 {"jan","feb","mar","apr","mai","jūn","jūl","aug","sep","okt","nov","dec", NullS };
static const char *my_locale_day_names_lv_LV[8] = 
 {"pirmdiena","otrdiena","trešdiena","ceturtdiena","piektdiena","sestdiena","svētdiena", NullS };
static const char *my_locale_ab_day_names_lv_LV[8] = 
 {"P ","O ","T ","C ","Pk","S ","Sv", NullS };
static TYPELIB my_locale_typelib_month_names_lv_LV = 
 { array_elements(my_locale_month_names_lv_LV)-1, "", my_locale_month_names_lv_LV, NULL };
static TYPELIB my_locale_typelib_ab_month_names_lv_LV = 
 { array_elements(my_locale_ab_month_names_lv_LV)-1, "", my_locale_ab_month_names_lv_LV, NULL };
static TYPELIB my_locale_typelib_day_names_lv_LV = 
 { array_elements(my_locale_day_names_lv_LV)-1, "", my_locale_day_names_lv_LV, NULL };
static TYPELIB my_locale_typelib_ab_day_names_lv_LV = 
 { array_elements(my_locale_ab_day_names_lv_LV)-1, "", my_locale_ab_day_names_lv_LV, NULL };
MY_LOCALE my_locale_lv_LV=
 { "lv_LV", "Latvian - Latvia", FALSE, &my_locale_typelib_month_names_lv_LV, &my_locale_typelib_ab_month_names_lv_LV, &my_locale_typelib_day_names_lv_LV, &my_locale_typelib_ab_day_names_lv_LV };
/***** LOCALE END lv_LV *****/

/***** LOCALE BEGIN mk_MK: Macedonian - FYROM *****/
static const char *my_locale_month_names_mk_MK[13] = 
 {"јануари","февруари","март","април","мај","јуни","јули","август","септември","октомври","ноември","декември", NullS };
static const char *my_locale_ab_month_names_mk_MK[13] = 
 {"јан","фев","мар","апр","мај","јун","јул","авг","сеп","окт","ное","дек", NullS };
static const char *my_locale_day_names_mk_MK[8] = 
 {"понеделник","вторник","среда","четврток","петок","сабота","недела", NullS };
static const char *my_locale_ab_day_names_mk_MK[8] = 
 {"пон","вто","сре","чет","пет","саб","нед", NullS };
static TYPELIB my_locale_typelib_month_names_mk_MK = 
 { array_elements(my_locale_month_names_mk_MK)-1, "", my_locale_month_names_mk_MK, NULL };
static TYPELIB my_locale_typelib_ab_month_names_mk_MK = 
 { array_elements(my_locale_ab_month_names_mk_MK)-1, "", my_locale_ab_month_names_mk_MK, NULL };
static TYPELIB my_locale_typelib_day_names_mk_MK = 
 { array_elements(my_locale_day_names_mk_MK)-1, "", my_locale_day_names_mk_MK, NULL };
static TYPELIB my_locale_typelib_ab_day_names_mk_MK = 
 { array_elements(my_locale_ab_day_names_mk_MK)-1, "", my_locale_ab_day_names_mk_MK, NULL };
MY_LOCALE my_locale_mk_MK=
 { "mk_MK", "Macedonian - FYROM", FALSE, &my_locale_typelib_month_names_mk_MK, &my_locale_typelib_ab_month_names_mk_MK, &my_locale_typelib_day_names_mk_MK, &my_locale_typelib_ab_day_names_mk_MK };
/***** LOCALE END mk_MK *****/

/***** LOCALE BEGIN mn_MN: Mongolia - Mongolian *****/
static const char *my_locale_month_names_mn_MN[13] = 
 {"Нэгдүгээр сар","Хоёрдугаар сар","Гуравдугаар сар","Дөрөвдүгээр сар","Тавдугаар сар","Зургаадугар сар","Долоодугаар сар","Наймдугаар сар","Есдүгээр сар","Аравдугаар сар","Арваннэгдүгээр сар","Арванхоёрдгаар сар", NullS };
static const char *my_locale_ab_month_names_mn_MN[13] = 
 {"1-р","2-р","3-р","4-р","5-р","6-р","7-р","8-р","9-р","10-р","11-р","12-р", NullS };
static const char *my_locale_day_names_mn_MN[8] = 
 {"Даваа","Мягмар","Лхагва","Пүрэв","Баасан","Бямба","Ням", NullS };
static const char *my_locale_ab_day_names_mn_MN[8] = 
 {"Да","Мя","Лх","Пү","Ба","Бя","Ня", NullS };
static TYPELIB my_locale_typelib_month_names_mn_MN = 
 { array_elements(my_locale_month_names_mn_MN)-1, "", my_locale_month_names_mn_MN, NULL };
static TYPELIB my_locale_typelib_ab_month_names_mn_MN = 
 { array_elements(my_locale_ab_month_names_mn_MN)-1, "", my_locale_ab_month_names_mn_MN, NULL };
static TYPELIB my_locale_typelib_day_names_mn_MN = 
 { array_elements(my_locale_day_names_mn_MN)-1, "", my_locale_day_names_mn_MN, NULL };
static TYPELIB my_locale_typelib_ab_day_names_mn_MN = 
 { array_elements(my_locale_ab_day_names_mn_MN)-1, "", my_locale_ab_day_names_mn_MN, NULL };
MY_LOCALE my_locale_mn_MN=
 { "mn_MN", "Mongolia - Mongolian", FALSE, &my_locale_typelib_month_names_mn_MN, &my_locale_typelib_ab_month_names_mn_MN, &my_locale_typelib_day_names_mn_MN, &my_locale_typelib_ab_day_names_mn_MN };
/***** LOCALE END mn_MN *****/

/***** LOCALE BEGIN ms_MY: Malay - Malaysia *****/
static const char *my_locale_month_names_ms_MY[13] = 
 {"Januari","Februari","Mac","April","Mei","Jun","Julai","Ogos","September","Oktober","November","Disember", NullS };
static const char *my_locale_ab_month_names_ms_MY[13] = 
 {"Jan","Feb","Mac","Apr","Mei","Jun","Jul","Ogos","Sep","Okt","Nov","Dis", NullS };
static const char *my_locale_day_names_ms_MY[8] = 
 {"Isnin","Selasa","Rabu","Khamis","Jumaat","Sabtu","Ahad", NullS };
static const char *my_locale_ab_day_names_ms_MY[8] = 
 {"Isn","Sel","Rab","Kha","Jum","Sab","Ahd", NullS };
static TYPELIB my_locale_typelib_month_names_ms_MY = 
 { array_elements(my_locale_month_names_ms_MY)-1, "", my_locale_month_names_ms_MY, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ms_MY = 
 { array_elements(my_locale_ab_month_names_ms_MY)-1, "", my_locale_ab_month_names_ms_MY, NULL };
static TYPELIB my_locale_typelib_day_names_ms_MY = 
 { array_elements(my_locale_day_names_ms_MY)-1, "", my_locale_day_names_ms_MY, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ms_MY = 
 { array_elements(my_locale_ab_day_names_ms_MY)-1, "", my_locale_ab_day_names_ms_MY, NULL };
MY_LOCALE my_locale_ms_MY=
 { "ms_MY", "Malay - Malaysia", TRUE, &my_locale_typelib_month_names_ms_MY, &my_locale_typelib_ab_month_names_ms_MY, &my_locale_typelib_day_names_ms_MY, &my_locale_typelib_ab_day_names_ms_MY };
/***** LOCALE END ms_MY *****/

/***** LOCALE BEGIN nb_NO: Norwegian(Bokml) - Norway *****/
static const char *my_locale_month_names_nb_NO[13] = 
 {"januar","februar","mars","april","mai","juni","juli","august","september","oktober","november","desember", NullS };
static const char *my_locale_ab_month_names_nb_NO[13] = 
 {"jan","feb","mar","apr","mai","jun","jul","aug","sep","okt","nov","des", NullS };
static const char *my_locale_day_names_nb_NO[8] = 
 {"mandag","tirsdag","onsdag","torsdag","fredag","lørdag","søndag", NullS };
static const char *my_locale_ab_day_names_nb_NO[8] = 
 {"man","tir","ons","tor","fre","lør","søn", NullS };
static TYPELIB my_locale_typelib_month_names_nb_NO = 
 { array_elements(my_locale_month_names_nb_NO)-1, "", my_locale_month_names_nb_NO, NULL };
static TYPELIB my_locale_typelib_ab_month_names_nb_NO = 
 { array_elements(my_locale_ab_month_names_nb_NO)-1, "", my_locale_ab_month_names_nb_NO, NULL };
static TYPELIB my_locale_typelib_day_names_nb_NO = 
 { array_elements(my_locale_day_names_nb_NO)-1, "", my_locale_day_names_nb_NO, NULL };
static TYPELIB my_locale_typelib_ab_day_names_nb_NO = 
 { array_elements(my_locale_ab_day_names_nb_NO)-1, "", my_locale_ab_day_names_nb_NO, NULL };
MY_LOCALE my_locale_nb_NO=
 { "nb_NO", "Norwegian(Bokml) - Norway", FALSE, &my_locale_typelib_month_names_nb_NO, &my_locale_typelib_ab_month_names_nb_NO, &my_locale_typelib_day_names_nb_NO, &my_locale_typelib_ab_day_names_nb_NO };
/***** LOCALE END nb_NO *****/

/***** LOCALE BEGIN nl_NL: Dutch - The Netherlands *****/
static const char *my_locale_month_names_nl_NL[13] = 
 {"januari","februari","maart","april","mei","juni","juli","augustus","september","oktober","november","december", NullS };
static const char *my_locale_ab_month_names_nl_NL[13] = 
 {"jan","feb","mrt","apr","mei","jun","jul","aug","sep","okt","nov","dec", NullS };
static const char *my_locale_day_names_nl_NL[8] = 
 {"maandag","dinsdag","woensdag","donderdag","vrijdag","zaterdag","zondag", NullS };
static const char *my_locale_ab_day_names_nl_NL[8] = 
 {"ma","di","wo","do","vr","za","zo", NullS };
static TYPELIB my_locale_typelib_month_names_nl_NL = 
 { array_elements(my_locale_month_names_nl_NL)-1, "", my_locale_month_names_nl_NL, NULL };
static TYPELIB my_locale_typelib_ab_month_names_nl_NL = 
 { array_elements(my_locale_ab_month_names_nl_NL)-1, "", my_locale_ab_month_names_nl_NL, NULL };
static TYPELIB my_locale_typelib_day_names_nl_NL = 
 { array_elements(my_locale_day_names_nl_NL)-1, "", my_locale_day_names_nl_NL, NULL };
static TYPELIB my_locale_typelib_ab_day_names_nl_NL = 
 { array_elements(my_locale_ab_day_names_nl_NL)-1, "", my_locale_ab_day_names_nl_NL, NULL };
MY_LOCALE my_locale_nl_NL=
 { "nl_NL", "Dutch - The Netherlands", TRUE, &my_locale_typelib_month_names_nl_NL, &my_locale_typelib_ab_month_names_nl_NL, &my_locale_typelib_day_names_nl_NL, &my_locale_typelib_ab_day_names_nl_NL };
/***** LOCALE END nl_NL *****/

/***** LOCALE BEGIN pl_PL: Polish - Poland *****/
static const char *my_locale_month_names_pl_PL[13] = 
 {"styczeń","luty","marzec","kwiecień","maj","czerwiec","lipiec","sierpień","wrzesień","październik","listopad","grudzień", NullS };
static const char *my_locale_ab_month_names_pl_PL[13] = 
 {"sty","lut","mar","kwi","maj","cze","lip","sie","wrz","paź","lis","gru", NullS };
static const char *my_locale_day_names_pl_PL[8] = 
 {"poniedziałek","wtorek","środa","czwartek","piątek","sobota","niedziela", NullS };
static const char *my_locale_ab_day_names_pl_PL[8] = 
 {"pon","wto","śro","czw","pią","sob","nie", NullS };
static TYPELIB my_locale_typelib_month_names_pl_PL = 
 { array_elements(my_locale_month_names_pl_PL)-1, "", my_locale_month_names_pl_PL, NULL };
static TYPELIB my_locale_typelib_ab_month_names_pl_PL = 
 { array_elements(my_locale_ab_month_names_pl_PL)-1, "", my_locale_ab_month_names_pl_PL, NULL };
static TYPELIB my_locale_typelib_day_names_pl_PL = 
 { array_elements(my_locale_day_names_pl_PL)-1, "", my_locale_day_names_pl_PL, NULL };
static TYPELIB my_locale_typelib_ab_day_names_pl_PL = 
 { array_elements(my_locale_ab_day_names_pl_PL)-1, "", my_locale_ab_day_names_pl_PL, NULL };
MY_LOCALE my_locale_pl_PL=
 { "pl_PL", "Polish - Poland", FALSE, &my_locale_typelib_month_names_pl_PL, &my_locale_typelib_ab_month_names_pl_PL, &my_locale_typelib_day_names_pl_PL, &my_locale_typelib_ab_day_names_pl_PL };
/***** LOCALE END pl_PL *****/

/***** LOCALE BEGIN pt_BR: Portugese - Brazil *****/
static const char *my_locale_month_names_pt_BR[13] = 
 {"janeiro","fevereiro","março","abril","maio","junho","julho","agosto","setembro","outubro","novembro","dezembro", NullS };
static const char *my_locale_ab_month_names_pt_BR[13] = 
 {"Jan","Fev","Mar","Abr","Mai","Jun","Jul","Ago","Set","Out","Nov","Dez", NullS };
static const char *my_locale_day_names_pt_BR[8] = 
 {"segunda","terça","quarta","quinta","sexta","sábado","domingo", NullS };
static const char *my_locale_ab_day_names_pt_BR[8] = 
 {"Seg","Ter","Qua","Qui","Sex","Sáb","Dom", NullS };
static TYPELIB my_locale_typelib_month_names_pt_BR = 
 { array_elements(my_locale_month_names_pt_BR)-1, "", my_locale_month_names_pt_BR, NULL };
static TYPELIB my_locale_typelib_ab_month_names_pt_BR = 
 { array_elements(my_locale_ab_month_names_pt_BR)-1, "", my_locale_ab_month_names_pt_BR, NULL };
static TYPELIB my_locale_typelib_day_names_pt_BR = 
 { array_elements(my_locale_day_names_pt_BR)-1, "", my_locale_day_names_pt_BR, NULL };
static TYPELIB my_locale_typelib_ab_day_names_pt_BR = 
 { array_elements(my_locale_ab_day_names_pt_BR)-1, "", my_locale_ab_day_names_pt_BR, NULL };
MY_LOCALE my_locale_pt_BR=
 { "pt_BR", "Portugese - Brazil", FALSE, &my_locale_typelib_month_names_pt_BR, &my_locale_typelib_ab_month_names_pt_BR, &my_locale_typelib_day_names_pt_BR, &my_locale_typelib_ab_day_names_pt_BR };
/***** LOCALE END pt_BR *****/

/***** LOCALE BEGIN pt_PT: Portugese - Portugal *****/
static const char *my_locale_month_names_pt_PT[13] = 
 {"Janeiro","Fevereiro","Março","Abril","Maio","Junho","Julho","Agosto","Setembro","Outubro","Novembro","Dezembro", NullS };
static const char *my_locale_ab_month_names_pt_PT[13] = 
 {"Jan","Fev","Mar","Abr","Mai","Jun","Jul","Ago","Set","Out","Nov","Dez", NullS };
static const char *my_locale_day_names_pt_PT[8] = 
 {"Segunda","Terça","Quarta","Quinta","Sexta","Sábado","Domingo", NullS };
static const char *my_locale_ab_day_names_pt_PT[8] = 
 {"Seg","Ter","Qua","Qui","Sex","Sáb","Dom", NullS };
static TYPELIB my_locale_typelib_month_names_pt_PT = 
 { array_elements(my_locale_month_names_pt_PT)-1, "", my_locale_month_names_pt_PT, NULL };
static TYPELIB my_locale_typelib_ab_month_names_pt_PT = 
 { array_elements(my_locale_ab_month_names_pt_PT)-1, "", my_locale_ab_month_names_pt_PT, NULL };
static TYPELIB my_locale_typelib_day_names_pt_PT = 
 { array_elements(my_locale_day_names_pt_PT)-1, "", my_locale_day_names_pt_PT, NULL };
static TYPELIB my_locale_typelib_ab_day_names_pt_PT = 
 { array_elements(my_locale_ab_day_names_pt_PT)-1, "", my_locale_ab_day_names_pt_PT, NULL };
MY_LOCALE my_locale_pt_PT=
 { "pt_PT", "Portugese - Portugal", FALSE, &my_locale_typelib_month_names_pt_PT, &my_locale_typelib_ab_month_names_pt_PT, &my_locale_typelib_day_names_pt_PT, &my_locale_typelib_ab_day_names_pt_PT };
/***** LOCALE END pt_PT *****/

/***** LOCALE BEGIN ro_RO: Romanian - Romania *****/
static const char *my_locale_month_names_ro_RO[13] = 
 {"Ianuarie","Februarie","Martie","Aprilie","Mai","Iunie","Iulie","August","Septembrie","Octombrie","Noiembrie","Decembrie", NullS };
static const char *my_locale_ab_month_names_ro_RO[13] = 
 {"ian","feb","mar","apr","mai","iun","iul","aug","sep","oct","nov","dec", NullS };
static const char *my_locale_day_names_ro_RO[8] = 
 {"Luni","Marţi","Miercuri","Joi","Vineri","SîmbĂtĂ","DuminicĂ", NullS };
static const char *my_locale_ab_day_names_ro_RO[8] = 
 {"Lu","Ma","Mi","Jo","Vi","Sî","Du", NullS };
static TYPELIB my_locale_typelib_month_names_ro_RO = 
 { array_elements(my_locale_month_names_ro_RO)-1, "", my_locale_month_names_ro_RO, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ro_RO = 
 { array_elements(my_locale_ab_month_names_ro_RO)-1, "", my_locale_ab_month_names_ro_RO, NULL };
static TYPELIB my_locale_typelib_day_names_ro_RO = 
 { array_elements(my_locale_day_names_ro_RO)-1, "", my_locale_day_names_ro_RO, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ro_RO = 
 { array_elements(my_locale_ab_day_names_ro_RO)-1, "", my_locale_ab_day_names_ro_RO, NULL };
MY_LOCALE my_locale_ro_RO=
 { "ro_RO", "Romanian - Romania", FALSE, &my_locale_typelib_month_names_ro_RO, &my_locale_typelib_ab_month_names_ro_RO, &my_locale_typelib_day_names_ro_RO, &my_locale_typelib_ab_day_names_ro_RO };
/***** LOCALE END ro_RO *****/

/***** LOCALE BEGIN ru_RU: Russian - Russia *****/
static const char *my_locale_month_names_ru_RU[13] = 
 {"Января","Февраля","Марта","Апреля","Мая","Июня","Июля","Августа","Сентября","Октября","Ноября","Декабря", NullS };
static const char *my_locale_ab_month_names_ru_RU[13] = 
 {"Янв","Фев","Мар","Апр","Май","Июн","Июл","Авг","Сен","Окт","Ноя","Дек", NullS };
static const char *my_locale_day_names_ru_RU[8] = 
 {"Понедельник","Вторник","Среда","Четверг","Пятница","Суббота","Воскресенье", NullS };
static const char *my_locale_ab_day_names_ru_RU[8] = 
 {"Пнд","Втр","Срд","Чтв","Птн","Сбт","Вск", NullS };
static TYPELIB my_locale_typelib_month_names_ru_RU = 
 { array_elements(my_locale_month_names_ru_RU)-1, "", my_locale_month_names_ru_RU, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ru_RU = 
 { array_elements(my_locale_ab_month_names_ru_RU)-1, "", my_locale_ab_month_names_ru_RU, NULL };
static TYPELIB my_locale_typelib_day_names_ru_RU = 
 { array_elements(my_locale_day_names_ru_RU)-1, "", my_locale_day_names_ru_RU, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ru_RU = 
 { array_elements(my_locale_ab_day_names_ru_RU)-1, "", my_locale_ab_day_names_ru_RU, NULL };
MY_LOCALE my_locale_ru_RU=
 { "ru_RU", "Russian - Russia", FALSE, &my_locale_typelib_month_names_ru_RU, &my_locale_typelib_ab_month_names_ru_RU, &my_locale_typelib_day_names_ru_RU, &my_locale_typelib_ab_day_names_ru_RU };
/***** LOCALE END ru_RU *****/

/***** LOCALE BEGIN ru_UA: Russian - Ukraine *****/
static const char *my_locale_month_names_ru_UA[13] = 
 {"Январь","Февраль","Март","Апрель","Май","Июнь","Июль","Август","Сентябрь","Октябрь","Ноябрь","Декабрь", NullS };
static const char *my_locale_ab_month_names_ru_UA[13] = 
 {"Янв","Фев","Мар","Апр","Май","Июн","Июл","Авг","Сен","Окт","Ноя","Дек", NullS };
static const char *my_locale_day_names_ru_UA[8] = 
 {"Понедельник","Вторник","Среда","Четверг","Пятница","Суббота","Воскресенье", NullS };
static const char *my_locale_ab_day_names_ru_UA[8] = 
 {"Пнд","Вто","Срд","Чтв","Птн","Суб","Вск", NullS };
static TYPELIB my_locale_typelib_month_names_ru_UA = 
 { array_elements(my_locale_month_names_ru_UA)-1, "", my_locale_month_names_ru_UA, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ru_UA = 
 { array_elements(my_locale_ab_month_names_ru_UA)-1, "", my_locale_ab_month_names_ru_UA, NULL };
static TYPELIB my_locale_typelib_day_names_ru_UA = 
 { array_elements(my_locale_day_names_ru_UA)-1, "", my_locale_day_names_ru_UA, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ru_UA = 
 { array_elements(my_locale_ab_day_names_ru_UA)-1, "", my_locale_ab_day_names_ru_UA, NULL };
MY_LOCALE my_locale_ru_UA=
 { "ru_UA", "Russian - Ukraine", FALSE, &my_locale_typelib_month_names_ru_UA, &my_locale_typelib_ab_month_names_ru_UA, &my_locale_typelib_day_names_ru_UA, &my_locale_typelib_ab_day_names_ru_UA };
/***** LOCALE END ru_UA *****/

/***** LOCALE BEGIN sk_SK: Slovak - Slovakia *****/
static const char *my_locale_month_names_sk_SK[13] = 
 {"január","február","marec","apríl","máj","jún","júl","august","september","október","november","december", NullS };
static const char *my_locale_ab_month_names_sk_SK[13] = 
 {"jan","feb","mar","apr","máj","jún","júl","aug","sep","okt","nov","dec", NullS };
static const char *my_locale_day_names_sk_SK[8] = 
 {"Pondelok","Utorok","Streda","Štvrtok","Piatok","Sobota","Nedeľa", NullS };
static const char *my_locale_ab_day_names_sk_SK[8] = 
 {"Po","Ut","St","Št","Pi","So","Ne", NullS };
static TYPELIB my_locale_typelib_month_names_sk_SK = 
 { array_elements(my_locale_month_names_sk_SK)-1, "", my_locale_month_names_sk_SK, NULL };
static TYPELIB my_locale_typelib_ab_month_names_sk_SK = 
 { array_elements(my_locale_ab_month_names_sk_SK)-1, "", my_locale_ab_month_names_sk_SK, NULL };
static TYPELIB my_locale_typelib_day_names_sk_SK = 
 { array_elements(my_locale_day_names_sk_SK)-1, "", my_locale_day_names_sk_SK, NULL };
static TYPELIB my_locale_typelib_ab_day_names_sk_SK = 
 { array_elements(my_locale_ab_day_names_sk_SK)-1, "", my_locale_ab_day_names_sk_SK, NULL };
MY_LOCALE my_locale_sk_SK=
 { "sk_SK", "Slovak - Slovakia", FALSE, &my_locale_typelib_month_names_sk_SK, &my_locale_typelib_ab_month_names_sk_SK, &my_locale_typelib_day_names_sk_SK, &my_locale_typelib_ab_day_names_sk_SK };
/***** LOCALE END sk_SK *****/

/***** LOCALE BEGIN sl_SI: Slovenian - Slovenia *****/
static const char *my_locale_month_names_sl_SI[13] = 
 {"januar","februar","marec","april","maj","junij","julij","avgust","september","oktober","november","december", NullS };
static const char *my_locale_ab_month_names_sl_SI[13] = 
 {"jan","feb","mar","apr","maj","jun","jul","avg","sep","okt","nov","dec", NullS };
static const char *my_locale_day_names_sl_SI[8] = 
 {"ponedeljek","torek","sreda","četrtek","petek","sobota","nedelja", NullS };
static const char *my_locale_ab_day_names_sl_SI[8] = 
 {"pon","tor","sre","čet","pet","sob","ned", NullS };
static TYPELIB my_locale_typelib_month_names_sl_SI = 
 { array_elements(my_locale_month_names_sl_SI)-1, "", my_locale_month_names_sl_SI, NULL };
static TYPELIB my_locale_typelib_ab_month_names_sl_SI = 
 { array_elements(my_locale_ab_month_names_sl_SI)-1, "", my_locale_ab_month_names_sl_SI, NULL };
static TYPELIB my_locale_typelib_day_names_sl_SI = 
 { array_elements(my_locale_day_names_sl_SI)-1, "", my_locale_day_names_sl_SI, NULL };
static TYPELIB my_locale_typelib_ab_day_names_sl_SI = 
 { array_elements(my_locale_ab_day_names_sl_SI)-1, "", my_locale_ab_day_names_sl_SI, NULL };
MY_LOCALE my_locale_sl_SI=
 { "sl_SI", "Slovenian - Slovenia", FALSE, &my_locale_typelib_month_names_sl_SI, &my_locale_typelib_ab_month_names_sl_SI, &my_locale_typelib_day_names_sl_SI, &my_locale_typelib_ab_day_names_sl_SI };
/***** LOCALE END sl_SI *****/

/***** LOCALE BEGIN sq_AL: Albanian - Albania *****/
static const char *my_locale_month_names_sq_AL[13] = 
 {"janar","shkurt","mars","prill","maj","qershor","korrik","gusht","shtator","tetor","nëntor","dhjetor", NullS };
static const char *my_locale_ab_month_names_sq_AL[13] = 
 {"Jan","Shk","Mar","Pri","Maj","Qer","Kor","Gsh","Sht","Tet","Nën","Dhj", NullS };
static const char *my_locale_day_names_sq_AL[8] = 
 {"e hënë ","e martë ","e mërkurë ","e enjte ","e premte ","e shtunë ","e diel ", NullS };
static const char *my_locale_ab_day_names_sq_AL[8] = 
 {"Hën ","Mar ","Mër ","Enj ","Pre ","Sht ","Die ", NullS };
static TYPELIB my_locale_typelib_month_names_sq_AL = 
 { array_elements(my_locale_month_names_sq_AL)-1, "", my_locale_month_names_sq_AL, NULL };
static TYPELIB my_locale_typelib_ab_month_names_sq_AL = 
 { array_elements(my_locale_ab_month_names_sq_AL)-1, "", my_locale_ab_month_names_sq_AL, NULL };
static TYPELIB my_locale_typelib_day_names_sq_AL = 
 { array_elements(my_locale_day_names_sq_AL)-1, "", my_locale_day_names_sq_AL, NULL };
static TYPELIB my_locale_typelib_ab_day_names_sq_AL = 
 { array_elements(my_locale_ab_day_names_sq_AL)-1, "", my_locale_ab_day_names_sq_AL, NULL };
MY_LOCALE my_locale_sq_AL=
 { "sq_AL", "Albanian - Albania", FALSE, &my_locale_typelib_month_names_sq_AL, &my_locale_typelib_ab_month_names_sq_AL, &my_locale_typelib_day_names_sq_AL, &my_locale_typelib_ab_day_names_sq_AL };
/***** LOCALE END sq_AL *****/

/***** LOCALE BEGIN sr_YU: Servian - Yugoslavia *****/
static const char *my_locale_month_names_sr_YU[13] = 
 {"januar","februar","mart","april","maj","juni","juli","avgust","septembar","oktobar","novembar","decembar", NullS };
static const char *my_locale_ab_month_names_sr_YU[13] = 
 {"jan","feb","mar","apr","maj","jun","jul","avg","sep","okt","nov","dec", NullS };
static const char *my_locale_day_names_sr_YU[8] = 
 {"ponedeljak","utorak","sreda","četvrtak","petak","subota","nedelja", NullS };
static const char *my_locale_ab_day_names_sr_YU[8] = 
 {"pon","uto","sre","čet","pet","sub","ned", NullS };
static TYPELIB my_locale_typelib_month_names_sr_YU = 
 { array_elements(my_locale_month_names_sr_YU)-1, "", my_locale_month_names_sr_YU, NULL };
static TYPELIB my_locale_typelib_ab_month_names_sr_YU = 
 { array_elements(my_locale_ab_month_names_sr_YU)-1, "", my_locale_ab_month_names_sr_YU, NULL };
static TYPELIB my_locale_typelib_day_names_sr_YU = 
 { array_elements(my_locale_day_names_sr_YU)-1, "", my_locale_day_names_sr_YU, NULL };
static TYPELIB my_locale_typelib_ab_day_names_sr_YU = 
 { array_elements(my_locale_ab_day_names_sr_YU)-1, "", my_locale_ab_day_names_sr_YU, NULL };
MY_LOCALE my_locale_sr_YU=
 { "sr_YU", "Servian - Yugoslavia", FALSE, &my_locale_typelib_month_names_sr_YU, &my_locale_typelib_ab_month_names_sr_YU, &my_locale_typelib_day_names_sr_YU, &my_locale_typelib_ab_day_names_sr_YU };
/***** LOCALE END sr_YU *****/

/***** LOCALE BEGIN sv_SE: Swedish - Sweden *****/
static const char *my_locale_month_names_sv_SE[13] = 
 {"januari","februari","mars","april","maj","juni","juli","augusti","september","oktober","november","december", NullS };
static const char *my_locale_ab_month_names_sv_SE[13] = 
 {"jan","feb","mar","apr","maj","jun","jul","aug","sep","okt","nov","dec", NullS };
static const char *my_locale_day_names_sv_SE[8] = 
 {"måndag","tisdag","onsdag","torsdag","fredag","lördag","söndag", NullS };
static const char *my_locale_ab_day_names_sv_SE[8] = 
 {"mån","tis","ons","tor","fre","lör","sön", NullS };
static TYPELIB my_locale_typelib_month_names_sv_SE = 
 { array_elements(my_locale_month_names_sv_SE)-1, "", my_locale_month_names_sv_SE, NULL };
static TYPELIB my_locale_typelib_ab_month_names_sv_SE = 
 { array_elements(my_locale_ab_month_names_sv_SE)-1, "", my_locale_ab_month_names_sv_SE, NULL };
static TYPELIB my_locale_typelib_day_names_sv_SE = 
 { array_elements(my_locale_day_names_sv_SE)-1, "", my_locale_day_names_sv_SE, NULL };
static TYPELIB my_locale_typelib_ab_day_names_sv_SE = 
 { array_elements(my_locale_ab_day_names_sv_SE)-1, "", my_locale_ab_day_names_sv_SE, NULL };
MY_LOCALE my_locale_sv_SE=
 { "sv_SE", "Swedish - Sweden", FALSE, &my_locale_typelib_month_names_sv_SE, &my_locale_typelib_ab_month_names_sv_SE, &my_locale_typelib_day_names_sv_SE, &my_locale_typelib_ab_day_names_sv_SE };
/***** LOCALE END sv_SE *****/

/***** LOCALE BEGIN ta_IN: Tamil - India *****/
static const char *my_locale_month_names_ta_IN[13] = 
 {"ஜனவரி","பெப்ரவரி","மார்ச்","ஏப்ரல்","மே","ஜூன்","ஜூலை","ஆகஸ்ட்","செப்டம்பர்","அக்டோபர்","நவம்பர்","டிசம்பர்r", NullS };
static const char *my_locale_ab_month_names_ta_IN[13] = 
 {"ஜனவரி","பெப்ரவரி","மார்ச்","ஏப்ரல்","மே","ஜூன்","ஜூலை","ஆகஸ்ட்","செப்டம்பர்","அக்டோபர்","நவம்பர்","டிசம்பர்r", NullS };
static const char *my_locale_day_names_ta_IN[8] = 
 {"திங்கள்","செவ்வாய்","புதன்","வியாழன்","வெள்ளி","சனி","ஞாயிறு", NullS };
static const char *my_locale_ab_day_names_ta_IN[8] = 
 {"த","ச","ப","வ","வ","ச","ஞ", NullS };
static TYPELIB my_locale_typelib_month_names_ta_IN = 
 { array_elements(my_locale_month_names_ta_IN)-1, "", my_locale_month_names_ta_IN, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ta_IN = 
 { array_elements(my_locale_ab_month_names_ta_IN)-1, "", my_locale_ab_month_names_ta_IN, NULL };
static TYPELIB my_locale_typelib_day_names_ta_IN = 
 { array_elements(my_locale_day_names_ta_IN)-1, "", my_locale_day_names_ta_IN, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ta_IN = 
 { array_elements(my_locale_ab_day_names_ta_IN)-1, "", my_locale_ab_day_names_ta_IN, NULL };
MY_LOCALE my_locale_ta_IN=
 { "ta_IN", "Tamil - India", FALSE, &my_locale_typelib_month_names_ta_IN, &my_locale_typelib_ab_month_names_ta_IN, &my_locale_typelib_day_names_ta_IN, &my_locale_typelib_ab_day_names_ta_IN };
/***** LOCALE END ta_IN *****/

/***** LOCALE BEGIN te_IN: Telugu - India *****/
static const char *my_locale_month_names_te_IN[13] = 
 {"జనవరి","ఫిబ్రవరి","మార్చి","ఏప్రిల్","మే","జూన్","జూలై","ఆగస్టు","సెప్టెంబర్","అక్టోబర్","నవంబర్","డిసెంబర్", NullS };
static const char *my_locale_ab_month_names_te_IN[13] = 
 {"జనవరి","ఫిబ్రవరి","మార్చి","ఏప్రిల్","మే","జూన్","జూలై","ఆగస్టు","సెప్టెంబర్","అక్టోబర్","నవంబర్","డిసెంబర్", NullS };
static const char *my_locale_day_names_te_IN[8] = 
 {"సోమవారం","మంగళవారం","బుధవారం","గురువారం","శుక్రవారం","శనివారం","ఆదివారం", NullS };
static const char *my_locale_ab_day_names_te_IN[8] = 
 {"సోమ","మంగళ","బుధ","గురు","శుక్ర","శని","ఆది", NullS };
static TYPELIB my_locale_typelib_month_names_te_IN = 
 { array_elements(my_locale_month_names_te_IN)-1, "", my_locale_month_names_te_IN, NULL };
static TYPELIB my_locale_typelib_ab_month_names_te_IN = 
 { array_elements(my_locale_ab_month_names_te_IN)-1, "", my_locale_ab_month_names_te_IN, NULL };
static TYPELIB my_locale_typelib_day_names_te_IN = 
 { array_elements(my_locale_day_names_te_IN)-1, "", my_locale_day_names_te_IN, NULL };
static TYPELIB my_locale_typelib_ab_day_names_te_IN = 
 { array_elements(my_locale_ab_day_names_te_IN)-1, "", my_locale_ab_day_names_te_IN, NULL };
MY_LOCALE my_locale_te_IN=
 { "te_IN", "Telugu - India", FALSE, &my_locale_typelib_month_names_te_IN, &my_locale_typelib_ab_month_names_te_IN, &my_locale_typelib_day_names_te_IN, &my_locale_typelib_ab_day_names_te_IN };
/***** LOCALE END te_IN *****/

/***** LOCALE BEGIN th_TH: Thai - Thailand *****/
static const char *my_locale_month_names_th_TH[13] = 
 {"มกราคม","กุมภาพันธ์","มีนาคม","เมษายน","พฤษภาคม","มิถุนายน","กรกฎาคม","สิงหาคม","กันยายน","ตุลาคม","พฤศจิกายน","ธันวาคม", NullS };
static const char *my_locale_ab_month_names_th_TH[13] = 
 {"ม.ค.","ก.พ.","มี.ค.","เม.ย.","พ.ค.","มิ.ย.","ก.ค.","ส.ค.","ก.ย.","ต.ค.","พ.ย.","ธ.ค.", NullS };
static const char *my_locale_day_names_th_TH[8] = 
 {"จันทร์","อังคาร","พุธ","พฤหัสบดี","ศุกร์","เสาร์","อาทิตย์", NullS };
static const char *my_locale_ab_day_names_th_TH[8] = 
 {"จ.","อ.","พ.","พฤ.","ศ.","ส.","อา.", NullS };
static TYPELIB my_locale_typelib_month_names_th_TH = 
 { array_elements(my_locale_month_names_th_TH)-1, "", my_locale_month_names_th_TH, NULL };
static TYPELIB my_locale_typelib_ab_month_names_th_TH = 
 { array_elements(my_locale_ab_month_names_th_TH)-1, "", my_locale_ab_month_names_th_TH, NULL };
static TYPELIB my_locale_typelib_day_names_th_TH = 
 { array_elements(my_locale_day_names_th_TH)-1, "", my_locale_day_names_th_TH, NULL };
static TYPELIB my_locale_typelib_ab_day_names_th_TH = 
 { array_elements(my_locale_ab_day_names_th_TH)-1, "", my_locale_ab_day_names_th_TH, NULL };
MY_LOCALE my_locale_th_TH=
 { "th_TH", "Thai - Thailand", FALSE, &my_locale_typelib_month_names_th_TH, &my_locale_typelib_ab_month_names_th_TH, &my_locale_typelib_day_names_th_TH, &my_locale_typelib_ab_day_names_th_TH };
/***** LOCALE END th_TH *****/

/***** LOCALE BEGIN tr_TR: Turkish - Turkey *****/
static const char *my_locale_month_names_tr_TR[13] = 
 {"Ocak","Şubat","Mart","Nisan","Mayıs","Haziran","Temmuz","Ağustos","Eylül","Ekim","Kasım","Aralık", NullS };
static const char *my_locale_ab_month_names_tr_TR[13] = 
 {"Oca","Şub","Mar","Nis","May","Haz","Tem","Ağu","Eyl","Eki","Kas","Ara", NullS };
static const char *my_locale_day_names_tr_TR[8] = 
 {"Pazartesi","Salı","Çarşamba","Perşembe","Cuma","Cumartesi","Pazar", NullS };
static const char *my_locale_ab_day_names_tr_TR[8] = 
 {"Pzt","Sal","Çrş","Prş","Cum","Cts","Paz", NullS };
static TYPELIB my_locale_typelib_month_names_tr_TR = 
 { array_elements(my_locale_month_names_tr_TR)-1, "", my_locale_month_names_tr_TR, NULL };
static TYPELIB my_locale_typelib_ab_month_names_tr_TR = 
 { array_elements(my_locale_ab_month_names_tr_TR)-1, "", my_locale_ab_month_names_tr_TR, NULL };
static TYPELIB my_locale_typelib_day_names_tr_TR = 
 { array_elements(my_locale_day_names_tr_TR)-1, "", my_locale_day_names_tr_TR, NULL };
static TYPELIB my_locale_typelib_ab_day_names_tr_TR = 
 { array_elements(my_locale_ab_day_names_tr_TR)-1, "", my_locale_ab_day_names_tr_TR, NULL };
MY_LOCALE my_locale_tr_TR=
 { "tr_TR", "Turkish - Turkey", FALSE, &my_locale_typelib_month_names_tr_TR, &my_locale_typelib_ab_month_names_tr_TR, &my_locale_typelib_day_names_tr_TR, &my_locale_typelib_ab_day_names_tr_TR };
/***** LOCALE END tr_TR *****/

/***** LOCALE BEGIN uk_UA: Ukrainian - Ukraine *****/
static const char *my_locale_month_names_uk_UA[13] = 
 {"Січень","Лютий","Березень","Квітень","Травень","Червень","Липень","Серпень","Вересень","Жовтень","Листопад","Грудень", NullS };
static const char *my_locale_ab_month_names_uk_UA[13] = 
 {"Січ","Лют","Бер","Кві","Тра","Чер","Лип","Сер","Вер","Жов","Лис","Гру", NullS };
static const char *my_locale_day_names_uk_UA[8] = 
 {"Понеділок","Вівторок","Середа","Четвер","П'ятниця","Субота","Неділя", NullS };
static const char *my_locale_ab_day_names_uk_UA[8] = 
 {"Пнд","Втр","Срд","Чтв","Птн","Сбт","Ндл", NullS };
static TYPELIB my_locale_typelib_month_names_uk_UA = 
 { array_elements(my_locale_month_names_uk_UA)-1, "", my_locale_month_names_uk_UA, NULL };
static TYPELIB my_locale_typelib_ab_month_names_uk_UA = 
 { array_elements(my_locale_ab_month_names_uk_UA)-1, "", my_locale_ab_month_names_uk_UA, NULL };
static TYPELIB my_locale_typelib_day_names_uk_UA = 
 { array_elements(my_locale_day_names_uk_UA)-1, "", my_locale_day_names_uk_UA, NULL };
static TYPELIB my_locale_typelib_ab_day_names_uk_UA = 
 { array_elements(my_locale_ab_day_names_uk_UA)-1, "", my_locale_ab_day_names_uk_UA, NULL };
MY_LOCALE my_locale_uk_UA=
 { "uk_UA", "Ukrainian - Ukraine", FALSE, &my_locale_typelib_month_names_uk_UA, &my_locale_typelib_ab_month_names_uk_UA, &my_locale_typelib_day_names_uk_UA, &my_locale_typelib_ab_day_names_uk_UA };
/***** LOCALE END uk_UA *****/

/***** LOCALE BEGIN ur_PK: Urdu - Pakistan *****/
static const char *my_locale_month_names_ur_PK[13] = 
 {"جنوري","فروري","مارچ","اپريل","مٓی","جون","جولاي","اگست","ستمبر","اكتوبر","نومبر","دسمبر", NullS };
static const char *my_locale_ab_month_names_ur_PK[13] = 
 {"جنوري","فروري","مارچ","اپريل","مٓی","جون","جولاي","اگست","ستمبر","اكتوبر","نومبر","دسمبر", NullS };
static const char *my_locale_day_names_ur_PK[8] = 
 {"پير","منگل","بدھ","جمعرات","جمعه","هفته","اتوار", NullS };
static const char *my_locale_ab_day_names_ur_PK[8] = 
 {"پير","منگل","بدھ","جمعرات","جمعه","هفته","اتوار", NullS };
static TYPELIB my_locale_typelib_month_names_ur_PK = 
 { array_elements(my_locale_month_names_ur_PK)-1, "", my_locale_month_names_ur_PK, NULL };
static TYPELIB my_locale_typelib_ab_month_names_ur_PK = 
 { array_elements(my_locale_ab_month_names_ur_PK)-1, "", my_locale_ab_month_names_ur_PK, NULL };
static TYPELIB my_locale_typelib_day_names_ur_PK = 
 { array_elements(my_locale_day_names_ur_PK)-1, "", my_locale_day_names_ur_PK, NULL };
static TYPELIB my_locale_typelib_ab_day_names_ur_PK = 
 { array_elements(my_locale_ab_day_names_ur_PK)-1, "", my_locale_ab_day_names_ur_PK, NULL };
MY_LOCALE my_locale_ur_PK=
 { "ur_PK", "Urdu - Pakistan", FALSE, &my_locale_typelib_month_names_ur_PK, &my_locale_typelib_ab_month_names_ur_PK, &my_locale_typelib_day_names_ur_PK, &my_locale_typelib_ab_day_names_ur_PK };
/***** LOCALE END ur_PK *****/

/***** LOCALE BEGIN vi_VN: Vietnamese - Vietnam *****/
static const char *my_locale_month_names_vi_VN[13] = 
 {"Tháng một","Tháng hai","Tháng ba","Tháng tư","Tháng năm","Tháng sáu","Tháng bảy","Tháng tám","Tháng chín","Tháng mười","Tháng mười một","Tháng mười hai", NullS };
static const char *my_locale_ab_month_names_vi_VN[13] = 
 {"Thg 1","Thg 2","Thg 3","Thg 4","Thg 5","Thg 6","Thg 7","Thg 8","Thg 9","Thg 10","Thg 11","Thg 12", NullS };
static const char *my_locale_day_names_vi_VN[8] = 
 {"Thứ hai ","Thứ ba ","Thứ tư ","Thứ năm ","Thứ sáu ","Thứ bảy ","Chủ nhật ", NullS };
static const char *my_locale_ab_day_names_vi_VN[8] = 
 {"Th 2 ","Th 3 ","Th 4 ","Th 5 ","Th 6 ","Th 7 ","CN ", NullS };
static TYPELIB my_locale_typelib_month_names_vi_VN = 
 { array_elements(my_locale_month_names_vi_VN)-1, "", my_locale_month_names_vi_VN, NULL };
static TYPELIB my_locale_typelib_ab_month_names_vi_VN = 
 { array_elements(my_locale_ab_month_names_vi_VN)-1, "", my_locale_ab_month_names_vi_VN, NULL };
static TYPELIB my_locale_typelib_day_names_vi_VN = 
 { array_elements(my_locale_day_names_vi_VN)-1, "", my_locale_day_names_vi_VN, NULL };
static TYPELIB my_locale_typelib_ab_day_names_vi_VN = 
 { array_elements(my_locale_ab_day_names_vi_VN)-1, "", my_locale_ab_day_names_vi_VN, NULL };
MY_LOCALE my_locale_vi_VN=
 { "vi_VN", "Vietnamese - Vietnam", FALSE, &my_locale_typelib_month_names_vi_VN, &my_locale_typelib_ab_month_names_vi_VN, &my_locale_typelib_day_names_vi_VN, &my_locale_typelib_ab_day_names_vi_VN };
/***** LOCALE END vi_VN *****/

/***** LOCALE BEGIN zh_CN: Chinese - Peoples Republic of China *****/
static const char *my_locale_month_names_zh_CN[13] = 
 {"一月","二月","三月","四月","五月","六月","七月","八月","九月","十月","十一月","十二月", NullS };
static const char *my_locale_ab_month_names_zh_CN[13] = 
 {" 1月"," 2月"," 3月"," 4月"," 5月"," 6月"," 7月"," 8月"," 9月","10月","11月","12月", NullS };
static const char *my_locale_day_names_zh_CN[8] = 
 {"星期一","星期二","星期三","星期四","星期五","星期六","星期日", NullS };
static const char *my_locale_ab_day_names_zh_CN[8] = 
 {"一","二","三","四","五","六","日", NullS };
static TYPELIB my_locale_typelib_month_names_zh_CN = 
 { array_elements(my_locale_month_names_zh_CN)-1, "", my_locale_month_names_zh_CN, NULL };
static TYPELIB my_locale_typelib_ab_month_names_zh_CN = 
 { array_elements(my_locale_ab_month_names_zh_CN)-1, "", my_locale_ab_month_names_zh_CN, NULL };
static TYPELIB my_locale_typelib_day_names_zh_CN = 
 { array_elements(my_locale_day_names_zh_CN)-1, "", my_locale_day_names_zh_CN, NULL };
static TYPELIB my_locale_typelib_ab_day_names_zh_CN = 
 { array_elements(my_locale_ab_day_names_zh_CN)-1, "", my_locale_ab_day_names_zh_CN, NULL };
MY_LOCALE my_locale_zh_CN=
 { "zh_CN", "Chinese - Peoples Republic of China", FALSE, &my_locale_typelib_month_names_zh_CN, &my_locale_typelib_ab_month_names_zh_CN, &my_locale_typelib_day_names_zh_CN, &my_locale_typelib_ab_day_names_zh_CN };
/***** LOCALE END zh_CN *****/

/***** LOCALE BEGIN zh_TW: Chinese - Taiwan *****/
static const char *my_locale_month_names_zh_TW[13] = 
 {"一月","二月","三月","四月","五月","六月","七月","八月","九月","十月","十一月","十二月", NullS };
static const char *my_locale_ab_month_names_zh_TW[13] = 
 {" 1月"," 2月"," 3月"," 4月"," 5月"," 6月"," 7月"," 8月"," 9月","10月","11月","12月", NullS };
static const char *my_locale_day_names_zh_TW[8] = 
 {"週一","週二","週三","週四","週五","週六","週日", NullS };
static const char *my_locale_ab_day_names_zh_TW[8] = 
 {"一","二","三","四","五","六","日", NullS };
static TYPELIB my_locale_typelib_month_names_zh_TW = 
 { array_elements(my_locale_month_names_zh_TW)-1, "", my_locale_month_names_zh_TW, NULL };
static TYPELIB my_locale_typelib_ab_month_names_zh_TW = 
 { array_elements(my_locale_ab_month_names_zh_TW)-1, "", my_locale_ab_month_names_zh_TW, NULL };
static TYPELIB my_locale_typelib_day_names_zh_TW = 
 { array_elements(my_locale_day_names_zh_TW)-1, "", my_locale_day_names_zh_TW, NULL };
static TYPELIB my_locale_typelib_ab_day_names_zh_TW = 
 { array_elements(my_locale_ab_day_names_zh_TW)-1, "", my_locale_ab_day_names_zh_TW, NULL };
MY_LOCALE my_locale_zh_TW=
 { "zh_TW", "Chinese - Taiwan", FALSE, &my_locale_typelib_month_names_zh_TW, &my_locale_typelib_ab_month_names_zh_TW, &my_locale_typelib_day_names_zh_TW, &my_locale_typelib_ab_day_names_zh_TW };
/***** LOCALE END zh_TW *****/

/***** LOCALE BEGIN ar_DZ: Arabic - Algeria *****/
MY_LOCALE my_locale_ar_DZ=
 { "ar_DZ", "Arabic - Algeria", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_DZ *****/

/***** LOCALE BEGIN ar_EG: Arabic - Egypt *****/
MY_LOCALE my_locale_ar_EG=
 { "ar_EG", "Arabic - Egypt", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_EG *****/

/***** LOCALE BEGIN ar_IN: Arabic - Iran *****/
MY_LOCALE my_locale_ar_IN=
 { "ar_IN", "Arabic - Iran", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_IN *****/

/***** LOCALE BEGIN ar_IQ: Arabic - Iraq *****/
MY_LOCALE my_locale_ar_IQ=
 { "ar_IQ", "Arabic - Iraq", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_IQ *****/

/***** LOCALE BEGIN ar_KW: Arabic - Kuwait *****/
MY_LOCALE my_locale_ar_KW=
 { "ar_KW", "Arabic - Kuwait", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_KW *****/

/***** LOCALE BEGIN ar_LB: Arabic - Lebanon *****/
MY_LOCALE my_locale_ar_LB=
 { "ar_LB", "Arabic - Lebanon", FALSE, &my_locale_typelib_month_names_ar_JO, &my_locale_typelib_ab_month_names_ar_JO, &my_locale_typelib_day_names_ar_JO, &my_locale_typelib_ab_day_names_ar_JO };
/***** LOCALE END ar_LB *****/

/***** LOCALE BEGIN ar_LY: Arabic - Libya *****/
MY_LOCALE my_locale_ar_LY=
 { "ar_LY", "Arabic - Libya", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_LY *****/

/***** LOCALE BEGIN ar_MA: Arabic - Morocco *****/
MY_LOCALE my_locale_ar_MA=
 { "ar_MA", "Arabic - Morocco", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_MA *****/

/***** LOCALE BEGIN ar_OM: Arabic - Oman *****/
MY_LOCALE my_locale_ar_OM=
 { "ar_OM", "Arabic - Oman", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_OM *****/

/***** LOCALE BEGIN ar_QA: Arabic - Qatar *****/
MY_LOCALE my_locale_ar_QA=
 { "ar_QA", "Arabic - Qatar", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_QA *****/

/***** LOCALE BEGIN ar_SD: Arabic - Sudan *****/
MY_LOCALE my_locale_ar_SD=
 { "ar_SD", "Arabic - Sudan", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_SD *****/

/***** LOCALE BEGIN ar_TN: Arabic - Tunisia *****/
MY_LOCALE my_locale_ar_TN=
 { "ar_TN", "Arabic - Tunisia", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_TN *****/

/***** LOCALE BEGIN ar_YE: Arabic - Yemen *****/
MY_LOCALE my_locale_ar_YE=
 { "ar_YE", "Arabic - Yemen", FALSE, &my_locale_typelib_month_names_ar_BH, &my_locale_typelib_ab_month_names_ar_BH, &my_locale_typelib_day_names_ar_BH, &my_locale_typelib_ab_day_names_ar_BH };
/***** LOCALE END ar_YE *****/

/***** LOCALE BEGIN de_BE: German - Belgium *****/
MY_LOCALE my_locale_de_BE=
 { "de_BE", "German - Belgium", FALSE, &my_locale_typelib_month_names_de_DE, &my_locale_typelib_ab_month_names_de_DE, &my_locale_typelib_day_names_de_DE, &my_locale_typelib_ab_day_names_de_DE };
/***** LOCALE END de_BE *****/

/***** LOCALE BEGIN de_CH: German - Switzerland *****/
MY_LOCALE my_locale_de_CH=
 { "de_CH", "German - Switzerland", FALSE, &my_locale_typelib_month_names_de_DE, &my_locale_typelib_ab_month_names_de_DE, &my_locale_typelib_day_names_de_DE, &my_locale_typelib_ab_day_names_de_DE };
/***** LOCALE END de_CH *****/

/***** LOCALE BEGIN de_LU: German - Luxembourg *****/
MY_LOCALE my_locale_de_LU=
 { "de_LU", "German - Luxembourg", FALSE, &my_locale_typelib_month_names_de_DE, &my_locale_typelib_ab_month_names_de_DE, &my_locale_typelib_day_names_de_DE, &my_locale_typelib_ab_day_names_de_DE };
/***** LOCALE END de_LU *****/

/***** LOCALE BEGIN en_AU: English - Australia *****/
MY_LOCALE my_locale_en_AU=
 { "en_AU", "English - Australia", TRUE, &my_locale_typelib_month_names_en_US, &my_locale_typelib_ab_month_names_en_US, &my_locale_typelib_day_names_en_US, &my_locale_typelib_ab_day_names_en_US };
/***** LOCALE END en_AU *****/

/***** LOCALE BEGIN en_CA: English - Canada *****/
MY_LOCALE my_locale_en_CA=
 { "en_CA", "English - Canada", TRUE, &my_locale_typelib_month_names_en_US, &my_locale_typelib_ab_month_names_en_US, &my_locale_typelib_day_names_en_US, &my_locale_typelib_ab_day_names_en_US };
/***** LOCALE END en_CA *****/

/***** LOCALE BEGIN en_GB: English - United Kingdom *****/
MY_LOCALE my_locale_en_GB=
 { "en_GB", "English - United Kingdom", TRUE, &my_locale_typelib_month_names_en_US, &my_locale_typelib_ab_month_names_en_US, &my_locale_typelib_day_names_en_US, &my_locale_typelib_ab_day_names_en_US };
/***** LOCALE END en_GB *****/

/***** LOCALE BEGIN en_IN: English - India *****/
MY_LOCALE my_locale_en_IN=
 { "en_IN", "English - India", TRUE, &my_locale_typelib_month_names_en_US, &my_locale_typelib_ab_month_names_en_US, &my_locale_typelib_day_names_en_US, &my_locale_typelib_ab_day_names_en_US };
/***** LOCALE END en_IN *****/

/***** LOCALE BEGIN en_NZ: English - New Zealand *****/
MY_LOCALE my_locale_en_NZ=
 { "en_NZ", "English - New Zealand", TRUE, &my_locale_typelib_month_names_en_US, &my_locale_typelib_ab_month_names_en_US, &my_locale_typelib_day_names_en_US, &my_locale_typelib_ab_day_names_en_US };
/***** LOCALE END en_NZ *****/

/***** LOCALE BEGIN en_PH: English - Philippines *****/
MY_LOCALE my_locale_en_PH=
 { "en_PH", "English - Philippines", TRUE, &my_locale_typelib_month_names_en_US, &my_locale_typelib_ab_month_names_en_US, &my_locale_typelib_day_names_en_US, &my_locale_typelib_ab_day_names_en_US };
/***** LOCALE END en_PH *****/

/***** LOCALE BEGIN en_ZA: English - South Africa *****/
MY_LOCALE my_locale_en_ZA=
 { "en_ZA", "English - South Africa", TRUE, &my_locale_typelib_month_names_en_US, &my_locale_typelib_ab_month_names_en_US, &my_locale_typelib_day_names_en_US, &my_locale_typelib_ab_day_names_en_US };
/***** LOCALE END en_ZA *****/

/***** LOCALE BEGIN en_ZW: English - Zimbabwe *****/
MY_LOCALE my_locale_en_ZW=
 { "en_ZW", "English - Zimbabwe", TRUE, &my_locale_typelib_month_names_en_US, &my_locale_typelib_ab_month_names_en_US, &my_locale_typelib_day_names_en_US, &my_locale_typelib_ab_day_names_en_US };
/***** LOCALE END en_ZW *****/

/***** LOCALE BEGIN es_AR: Spanish - Argentina *****/
MY_LOCALE my_locale_es_AR=
 { "es_AR", "Spanish - Argentina", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_AR *****/

/***** LOCALE BEGIN es_BO: Spanish - Bolivia *****/
MY_LOCALE my_locale_es_BO=
 { "es_BO", "Spanish - Bolivia", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_BO *****/

/***** LOCALE BEGIN es_CL: Spanish - Chile *****/
MY_LOCALE my_locale_es_CL=
 { "es_CL", "Spanish - Chile", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_CL *****/

/***** LOCALE BEGIN es_CO: Spanish - Columbia *****/
MY_LOCALE my_locale_es_CO=
 { "es_CO", "Spanish - Columbia", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_CO *****/

/***** LOCALE BEGIN es_CR: Spanish - Costa Rica *****/
MY_LOCALE my_locale_es_CR=
 { "es_CR", "Spanish - Costa Rica", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_CR *****/

/***** LOCALE BEGIN es_DO: Spanish - Dominican Republic *****/
MY_LOCALE my_locale_es_DO=
 { "es_DO", "Spanish - Dominican Republic", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_DO *****/

/***** LOCALE BEGIN es_EC: Spanish - Ecuador *****/
MY_LOCALE my_locale_es_EC=
 { "es_EC", "Spanish - Ecuador", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_EC *****/

/***** LOCALE BEGIN es_GT: Spanish - Guatemala *****/
MY_LOCALE my_locale_es_GT=
 { "es_GT", "Spanish - Guatemala", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_GT *****/

/***** LOCALE BEGIN es_HN: Spanish - Honduras *****/
MY_LOCALE my_locale_es_HN=
 { "es_HN", "Spanish - Honduras", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_HN *****/

/***** LOCALE BEGIN es_MX: Spanish - Mexico *****/
MY_LOCALE my_locale_es_MX=
 { "es_MX", "Spanish - Mexico", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_MX *****/

/***** LOCALE BEGIN es_NI: Spanish - Nicaragua *****/
MY_LOCALE my_locale_es_NI=
 { "es_NI", "Spanish - Nicaragua", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_NI *****/

/***** LOCALE BEGIN es_PA: Spanish - Panama *****/
MY_LOCALE my_locale_es_PA=
 { "es_PA", "Spanish - Panama", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_PA *****/

/***** LOCALE BEGIN es_PE: Spanish - Peru *****/
MY_LOCALE my_locale_es_PE=
 { "es_PE", "Spanish - Peru", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_PE *****/

/***** LOCALE BEGIN es_PR: Spanish - Puerto Rico *****/
MY_LOCALE my_locale_es_PR=
 { "es_PR", "Spanish - Puerto Rico", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_PR *****/

/***** LOCALE BEGIN es_PY: Spanish - Paraguay *****/
MY_LOCALE my_locale_es_PY=
 { "es_PY", "Spanish - Paraguay", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_PY *****/

/***** LOCALE BEGIN es_SV: Spanish - El Salvador *****/
MY_LOCALE my_locale_es_SV=
 { "es_SV", "Spanish - El Salvador", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_SV *****/

/***** LOCALE BEGIN es_US: Spanish - United States *****/
MY_LOCALE my_locale_es_US=
 { "es_US", "Spanish - United States", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_US *****/

/***** LOCALE BEGIN es_UY: Spanish - Uruguay *****/
MY_LOCALE my_locale_es_UY=
 { "es_UY", "Spanish - Uruguay", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_UY *****/

/***** LOCALE BEGIN es_VE: Spanish - Venezuela *****/
MY_LOCALE my_locale_es_VE=
 { "es_VE", "Spanish - Venezuela", FALSE, &my_locale_typelib_month_names_es_ES, &my_locale_typelib_ab_month_names_es_ES, &my_locale_typelib_day_names_es_ES, &my_locale_typelib_ab_day_names_es_ES };
/***** LOCALE END es_VE *****/

/***** LOCALE BEGIN fr_BE: French - Belgium *****/
MY_LOCALE my_locale_fr_BE=
 { "fr_BE", "French - Belgium", FALSE, &my_locale_typelib_month_names_fr_FR, &my_locale_typelib_ab_month_names_fr_FR, &my_locale_typelib_day_names_fr_FR, &my_locale_typelib_ab_day_names_fr_FR };
/***** LOCALE END fr_BE *****/

/***** LOCALE BEGIN fr_CA: French - Canada *****/
MY_LOCALE my_locale_fr_CA=
 { "fr_CA", "French - Canada", FALSE, &my_locale_typelib_month_names_fr_FR, &my_locale_typelib_ab_month_names_fr_FR, &my_locale_typelib_day_names_fr_FR, &my_locale_typelib_ab_day_names_fr_FR };
/***** LOCALE END fr_CA *****/

/***** LOCALE BEGIN fr_CH: French - Switzerland *****/
MY_LOCALE my_locale_fr_CH=
 { "fr_CH", "French - Switzerland", FALSE, &my_locale_typelib_month_names_fr_FR, &my_locale_typelib_ab_month_names_fr_FR, &my_locale_typelib_day_names_fr_FR, &my_locale_typelib_ab_day_names_fr_FR };
/***** LOCALE END fr_CH *****/

/***** LOCALE BEGIN fr_LU: French - Luxembourg *****/
MY_LOCALE my_locale_fr_LU=
 { "fr_LU", "French - Luxembourg", FALSE, &my_locale_typelib_month_names_fr_FR, &my_locale_typelib_ab_month_names_fr_FR, &my_locale_typelib_day_names_fr_FR, &my_locale_typelib_ab_day_names_fr_FR };
/***** LOCALE END fr_LU *****/

/***** LOCALE BEGIN it_IT: Italian - Italy *****/
MY_LOCALE my_locale_it_IT=
 { "it_IT", "Italian - Italy", FALSE, &my_locale_typelib_month_names_it_CH, &my_locale_typelib_ab_month_names_it_CH, &my_locale_typelib_day_names_it_CH, &my_locale_typelib_ab_day_names_it_CH };
/***** LOCALE END it_IT *****/

/***** LOCALE BEGIN nl_BE: Dutch - Belgium *****/
MY_LOCALE my_locale_nl_BE=
 { "nl_BE", "Dutch - Belgium", TRUE, &my_locale_typelib_month_names_nl_NL, &my_locale_typelib_ab_month_names_nl_NL, &my_locale_typelib_day_names_nl_NL, &my_locale_typelib_ab_day_names_nl_NL };
/***** LOCALE END nl_BE *****/

/***** LOCALE BEGIN no_NO: Norwegian - Norway *****/
MY_LOCALE my_locale_no_NO=
 { "no_NO", "Norwegian - Norway", FALSE, &my_locale_typelib_month_names_nb_NO, &my_locale_typelib_ab_month_names_nb_NO, &my_locale_typelib_day_names_nb_NO, &my_locale_typelib_ab_day_names_nb_NO };
/***** LOCALE END no_NO *****/

/***** LOCALE BEGIN sv_FI: Swedish - Finland *****/
MY_LOCALE my_locale_sv_FI=
 { "sv_FI", "Swedish - Finland", FALSE, &my_locale_typelib_month_names_sv_SE, &my_locale_typelib_ab_month_names_sv_SE, &my_locale_typelib_day_names_sv_SE, &my_locale_typelib_ab_day_names_sv_SE };
/***** LOCALE END sv_FI *****/

/***** LOCALE BEGIN zh_HK: Chinese - Hong Kong SAR *****/
MY_LOCALE my_locale_zh_HK=
 { "zh_HK", "Chinese - Hong Kong SAR", FALSE, &my_locale_typelib_month_names_zh_CN, &my_locale_typelib_ab_month_names_zh_CN, &my_locale_typelib_day_names_zh_CN, &my_locale_typelib_ab_day_names_zh_CN };
/***** LOCALE END zh_HK *****/

MY_LOCALE *my_locales[]=
  {
    &my_locale_en_US,
    &my_locale_en_GB,
    &my_locale_ja_JP,
    &my_locale_sv_SE,
    &my_locale_de_DE,
    &my_locale_fr_FR,
    &my_locale_ar_AE,
    &my_locale_ar_BH,
    &my_locale_ar_JO,
    &my_locale_ar_SA,
    &my_locale_ar_SY,
    &my_locale_be_BY,
    &my_locale_bg_BG,
    &my_locale_ca_ES,
    &my_locale_cs_CZ,
    &my_locale_da_DK,
    &my_locale_de_AT,
    &my_locale_es_ES,
    &my_locale_et_EE,
    &my_locale_eu_ES,
    &my_locale_fi_FI,
    &my_locale_fo_FO,
    &my_locale_gl_ES,
    &my_locale_gu_IN,
    &my_locale_he_IL,
    &my_locale_hi_IN,
    &my_locale_hr_HR,
    &my_locale_hu_HU,
    &my_locale_id_ID,
    &my_locale_is_IS,
    &my_locale_it_CH,
    &my_locale_ko_KR,
    &my_locale_lt_LT,
    &my_locale_lv_LV,
    &my_locale_mk_MK,
    &my_locale_mn_MN,
    &my_locale_ms_MY,
    &my_locale_nb_NO,
    &my_locale_nl_NL,
    &my_locale_pl_PL,
    &my_locale_pt_BR,
    &my_locale_pt_PT,
    &my_locale_ro_RO,
    &my_locale_ru_RU,
    &my_locale_ru_UA,
    &my_locale_sk_SK,
    &my_locale_sl_SI,
    &my_locale_sq_AL,
    &my_locale_sr_YU,
    &my_locale_ta_IN,
    &my_locale_te_IN,
    &my_locale_th_TH,
    &my_locale_tr_TR,
    &my_locale_uk_UA,
    &my_locale_ur_PK,
    &my_locale_vi_VN,
    &my_locale_zh_CN,
    &my_locale_zh_TW,
    &my_locale_ar_DZ,
    &my_locale_ar_EG,
    &my_locale_ar_IN,
    &my_locale_ar_IQ,
    &my_locale_ar_KW,
    &my_locale_ar_LB,
    &my_locale_ar_LY,
    &my_locale_ar_MA,
    &my_locale_ar_OM,
    &my_locale_ar_QA,
    &my_locale_ar_SD,
    &my_locale_ar_TN,
    &my_locale_ar_YE,
    &my_locale_de_BE,
    &my_locale_de_CH,
    &my_locale_de_LU,
    &my_locale_en_AU,
    &my_locale_en_CA,
    &my_locale_en_IN,
    &my_locale_en_NZ,
    &my_locale_en_PH,
    &my_locale_en_ZA,
    &my_locale_en_ZW,
    &my_locale_es_AR,
    &my_locale_es_BO,
    &my_locale_es_CL,
    &my_locale_es_CO,
    &my_locale_es_CR,
    &my_locale_es_DO,
    &my_locale_es_EC,
    &my_locale_es_GT,
    &my_locale_es_HN,
    &my_locale_es_MX,
    &my_locale_es_NI,
    &my_locale_es_PA,
    &my_locale_es_PE,
    &my_locale_es_PR,
    &my_locale_es_PY,
    &my_locale_es_SV,
    &my_locale_es_US,
    &my_locale_es_UY,
    &my_locale_es_VE,
    &my_locale_fr_BE,
    &my_locale_fr_CA,
    &my_locale_fr_CH,
    &my_locale_fr_LU,
    &my_locale_it_IT,
    &my_locale_nl_BE,
    &my_locale_no_NO,
    &my_locale_sv_FI,
    &my_locale_zh_HK,
    NULL 
  };
