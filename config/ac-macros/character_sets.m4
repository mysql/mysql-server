dnl In order to add new charset, you must add charset name to
dnl this CHARSETS_AVAILABLE list and sql/share/charsets/Index.xml.
dnl If the character set uses strcoll or other special handling,
dnl you must also create strings/ctype-$charset_name.c

AC_DIVERT_PUSH(0)

define(CHARSETS_AVAILABLE0,binary)
define(CHARSETS_AVAILABLE1,armscii8 ascii big5 cp1250 cp1251 cp1256 cp1257)
define(CHARSETS_AVAILABLE2,cp850 cp852 cp866 dec8 euckr gb2312 gbk geostd8)
define(CHARSETS_AVAILABLE3,greek hebrew hp8 keybcs2 koi8r koi8u)
define(CHARSETS_AVAILABLE4,latin1 latin2 latin5 latin7 macce macroman)
define(CHARSETS_AVAILABLE5,sjis swe7 tis620 ucs2 ujis utf8)

DEFAULT_CHARSET=latin1
CHARSETS_AVAILABLE="CHARSETS_AVAILABLE0 CHARSETS_AVAILABLE1 CHARSETS_AVAILABLE2 CHARSETS_AVAILABLE3 CHARSETS_AVAILABLE4 CHARSETS_AVAILABLE5"
CHARSETS_COMPLEX="big5 cp1250 euckr gb2312 gbk latin1 latin2 sjis tis620 ucs2 ujis utf8"

AC_DIVERT_POP

AC_ARG_WITH(charset,
   [  --with-charset=CHARSET
                          Default character set, use one of:
                          CHARSETS_AVAILABLE0
                          CHARSETS_AVAILABLE1
                          CHARSETS_AVAILABLE2
                          CHARSETS_AVAILABLE3
                          CHARSETS_AVAILABLE4
                          CHARSETS_AVAILABLE5],
   [default_charset="$withval"],
   [default_charset="$DEFAULT_CHARSET"])

AC_ARG_WITH(collation,
   [  --with-collation=COLLATION
                          Default collation],
   [default_collation="$withval"],
   [default_collation="default"])


AC_ARG_WITH(extra-charsets,
  [  --with-extra-charsets=CHARSET[,CHARSET,...]
                          Use charsets in addition to default (none, complex,
                          all, or a list selected from the above sets)],
  [extra_charsets="$withval"],
  [extra_charsets="none"])


AC_MSG_CHECKING("character sets")

CHARSETS="$default_charset latin1 utf8"

if test "$extra_charsets" = no; then
  CHARSETS="$CHARSETS"
elif test "$extra_charsets" = none; then
  CHARSETS="$CHARSETS"
elif test "$extra_charsets" = complex; then
  CHARSETS="$CHARSETS $CHARSETS_COMPLEX"
  AC_DEFINE([DEFINE_ALL_CHARACTER_SETS],1,[all charsets are available])
elif test "$extra_charsets" = all; then
  CHARSETS="$CHARSETS $CHARSETS_AVAILABLE"
  AC_DEFINE([DEFINE_ALL_CHARACTER_SETS],1,[all charsets are available])
else
  EXTRA_CHARSETS=`echo $extra_charsets | sed -e 's/,/ /g'`
  CHARSETS="$CHARSETS $EXTRA_CHARSETS"
fi

for cs in $CHARSETS
do
  case $cs in 
    armscii8)
      AC_DEFINE(HAVE_CHARSET_armscii8, 1,
                [Define to enable charset armscii8])
      ;;
    ascii)
      AC_DEFINE(HAVE_CHARSET_ascii, 1,
                [Define to enable ascii character set])
      ;;
    big5)
      AC_DEFINE(HAVE_CHARSET_big5, 1, [Define to enable charset big5])
      AC_DEFINE([USE_MB], [1], [Use multi-byte character routines])
      AC_DEFINE(USE_MB_IDENT, [1], [ ])
      ;;
    binary)
      ;;
    cp1250)
      AC_DEFINE(HAVE_CHARSET_cp1250, 1, [Define to enable cp1250])
      ;;
    cp1251)
      AC_DEFINE(HAVE_CHARSET_cp1251, 1, [Define to enable charset cp1251])
      ;;
    cp1256)
      AC_DEFINE(HAVE_CHARSET_cp1256, 1, [Define to enable charset cp1256])
      ;;
    cp1257)
      AC_DEFINE(HAVE_CHARSET_cp1257, 1, [Define to enable charset cp1257])
      ;;
    cp850)
      AC_DEFINE(HAVE_CHARSET_cp850, 1, [Define to enable charset cp850])
      ;;
    cp852)
      AC_DEFINE(HAVE_CHARSET_cp852, 1, [Define to enable charset cp852])
      ;;
    cp866)
      AC_DEFINE(HAVE_CHARSET_cp866, 1, [Define to enable charset cp866])
      ;;
    dec8)
      AC_DEFINE(HAVE_CHARSET_dec8, 1, [Define to enable charset dec8])
      ;;
    euckr)
      AC_DEFINE(HAVE_CHARSET_euckr, 1, [Define to enable charset euckr])
      AC_DEFINE([USE_MB], [1], [Use multi-byte character routines])
      AC_DEFINE(USE_MB_IDENT, 1)
      ;;
    gb2312)
      AC_DEFINE(HAVE_CHARSET_gb2312, 1, [Define to enable charset gb2312])
      AC_DEFINE([USE_MB], 1, [Use multi-byte character routines])
      AC_DEFINE(USE_MB_IDENT, 1)
      ;;
    gbk)
      AC_DEFINE(HAVE_CHARSET_gbk, 1, [Define to enable charset gbk])
      AC_DEFINE([USE_MB], [1], [Use multi-byte character routines])
      AC_DEFINE(USE_MB_IDENT, 1)
      ;;
    geostd8)
      AC_DEFINE(HAVE_CHARSET_geostd8, 1, [Define to enable charset geostd8])
      ;;
    greek)
      AC_DEFINE(HAVE_CHARSET_greek, 1, [Define to enable charset greek])
      ;;
    hebrew)
      AC_DEFINE(HAVE_CHARSET_hebrew, 1, [Define to enable charset hebrew])
      ;;
    hp8)
      AC_DEFINE(HAVE_CHARSET_hp8, 1, [Define to enable charset hp8])
      ;;
    keybcs2)
      AC_DEFINE(HAVE_CHARSET_keybcs2, 1, [Define to enable charset keybcs2])
      ;;
    koi8r)
      AC_DEFINE(HAVE_CHARSET_koi8r, 1, [Define to enable charset koi8r])
      ;;
    koi8u)
      AC_DEFINE(HAVE_CHARSET_koi8u, 1, [Define to enable charset koi8u])
      ;;
    latin1)
      AC_DEFINE(HAVE_CHARSET_latin1, 1, [Define to enable charset latin1])
      ;;
    latin2)
      AC_DEFINE(HAVE_CHARSET_latin2, 1, [Define to enable charset latin2])
      ;;
    latin5)
      AC_DEFINE(HAVE_CHARSET_latin5, 1, [Define to enable charset latin5])
      ;;
    latin7)
      AC_DEFINE(HAVE_CHARSET_latin7, 1, [Define to enable charset latin7])
      ;;
    macce)
      AC_DEFINE(HAVE_CHARSET_macce, 1, [Define to enable charset macce])
      ;;
    macroman)
      AC_DEFINE(HAVE_CHARSET_macroman, 1,
                [Define to enable charset macroman])
      ;;
    sjis)
      AC_DEFINE(HAVE_CHARSET_sjis, 1, [Define to enable charset sjis])
      AC_DEFINE([USE_MB], 1, [Use multi-byte character routines])
      AC_DEFINE(USE_MB_IDENT, 1)
      ;;
    swe7)
      AC_DEFINE(HAVE_CHARSET_swe7, 1, [Define to enable charset swe7])
      ;;
    tis620)
      AC_DEFINE(HAVE_CHARSET_tis620, 1, [Define to enable charset tis620])
      ;;
    ucs2)
      AC_DEFINE(HAVE_CHARSET_ucs2, 1, [Define to enable charset ucs2])
      AC_DEFINE([USE_MB], [1], [Use multi-byte character routines])
      AC_DEFINE(USE_MB_IDENT, 1)
      ;;
    ujis)
      AC_DEFINE(HAVE_CHARSET_ujis, 1, [Define to enable charset ujis])
      AC_DEFINE([USE_MB], [1], [Use multi-byte character routines])
      AC_DEFINE(USE_MB_IDENT, 1)
      ;;
    utf8)
      AC_DEFINE(HAVE_CHARSET_utf8, 1, [Define to enable ut8])
      AC_DEFINE([USE_MB], 1, [Use multi-byte character routines])
      AC_DEFINE(USE_MB_IDENT, 1)
      ;;
    *)
      AC_MSG_ERROR([Charset '$cs' not available. (Available are: $CHARSETS_AVAILABLE).
      See the Installation chapter in the Reference Manual.]);
  esac
done


      default_charset_collations=""

case $default_charset in 
    armscii8)
      default_charset_default_collation="armscii8_general_ci"
      default_charset_collations="armscii8_general_ci armscii8_bin"
      ;;
    ascii)
      default_charset_default_collation="ascii_general_ci"
      default_charset_collations="ascii_general_ci ascii_bin"
      ;;
    big5)
      default_charset_default_collation="big5_chinese_ci"
      default_charset_collations="big5_chinese_ci big5_bin"
      ;;
    binary)
      default_charset_default_collation="binary"
      default_charset_collations="binary"
      ;;
    cp1250)
      default_charset_default_collation="cp1250_general_ci"
      default_charset_collations="cp1250_general_ci cp1250_czech_cs cp1250_bin"
      ;;
    cp1251)
      default_charset_default_collation="cp1251_general_ci"
      default_charset_collations="cp1251_general_ci cp1251_general_cs cp1251_bin cp1251_bulgarian_ci cp1251_ukrainian_ci"
      ;;
    cp1256)
      default_charset_default_collation="cp1256_general_ci"
      default_charset_collations="cp1256_general_ci cp1256_bin"
      ;;
    cp1257)
      default_charset_default_collation="cp1257_general_ci"
      default_charset_collations="cp1257_general_ci cp1257_lithuanian_ci cp1257_bin"
      ;;
    cp850)
      default_charset_default_collation="cp850_general_ci"
      default_charset_collations="cp850_general_ci cp850_bin"
      ;;
    cp852)
      default_charset_default_collation="cp852_general_ci"
      default_charset_collations="cp852_general_ci cp852_bin"
      ;;
    cp866)
      default_charset_default_collation="cp866_general_ci"
      default_charset_collations="cp866_general_ci cp866_bin"
      ;;
    dec8)
      default_charset_default_collation="dec8_swedish_ci"
      default_charset_collations="dec8_swedish_ci dec8_bin"
      ;;
    euckr)
      default_charset_default_collation="euckr_korean_ci"
      default_charset_collations="euckr_korean_ci euckr_bin"
      ;;
    gb2312)
      default_charset_default_collation="gb2312_chinese_ci"
      default_charset_collations="gb2312_chinese_ci gb2312_bin"
      ;;
    gbk)
      default_charset_default_collation="gbk_chinese_ci"
      default_charset_collations="gbk_chinese_ci gbk_bin"
      ;;
    geostd8)
      default_charset_default_collation="geostd8_general_ci"
      default_charset_collations="geostd8_general_ci geostd8_bin"
      ;;
    greek)
      default_charset_default_collation="greek_general_ci"
      default_charset_collations="greek_general_ci greek_bin"
      ;;
    hebrew)
      default_charset_default_collation="hebrew_general_ci"
      default_charset_collations="hebrew_general_ci hebrew_bin"
      ;;
    hp8)
      default_charset_default_collation="hp8_english_ci"
      default_charset_collations="hp8_english_ci hp8_bin"
      ;;
    keybcs2)
      default_charset_default_collation="keybcs2_general_ci"
      default_charset_collations="keybcs2_general_ci keybcs2_bin"
      ;;
    koi8r)
      default_charset_default_collation="koi8r_general_ci"
      default_charset_collations="koi8r_general_ci koi8r_bin"
      ;;
    koi8u)
      default_charset_default_collation="koi8u_general_ci"
      default_charset_collations="koi8u_general_ci koi8u_bin"
      ;;
    latin1)
      default_charset_default_collation="latin1_swedish_ci"
      default_charset_collations="latin1_general_ci latin1_general_cs latin1_bin latin1_german1_ci latin1_german2_ci latin1_danish_ci latin1_swedish_ci"
      ;;
    latin2)
      default_charset_default_collation="latin2_general_ci"
      default_charset_collations="latin2_general_ci latin2_bin latin2_czech_cs latin2_hungarian_ci latin2_croatian_ci"
      ;;
    latin5)
      default_charset_default_collation="latin5_turkish_ci"
      default_charset_collations="latin5_turkish_ci latin5_bin"
      ;;
    latin7)
      default_charset_default_collation="latin7_general_ci"
      default_charset_collations="latin7_general_ci latin7_general_cs latin7_bin latin7_estonian_cs"
      ;;
    macce)
      default_charset_default_collation="macce_general_ci"
      default_charset_collations="macce_general_ci macce_bin"
      ;;
    macroman)
      default_charset_default_collation="macroman_general_ci"
      default_charset_collations="macroman_general_ci macroman_bin"
      ;;
    sjis)
      default_charset_default_collation="sjis_japanese_ci"
      default_charset_collations="sjis_japanese_ci sjis_bin"
      ;;
    swe7)
      default_charset_default_collation="swe7_swedish_ci"
      default_charset_collations="swe7_swedish_ci swe7_bin"
      ;;
    tis620)
      default_charset_default_collation="tis620_thai_ci"
      default_charset_collations="tis620_thai_ci tis620_bin"
      ;;
    ucs2)
      default_charset_default_collation="ucs2_general_ci"
      define(UCSC1, ucs2_general_ci ucs2_bin)
      define(UCSC2, ucs2_czech_ci ucs2_danish_ci)
      define(UCSC3, ucs2_estonian_ci ucs2_icelandic_ci)
      define(UCSC4, ucs2_latvian_ci ucs2_lithuanian_ci)
      define(UCSC5, ucs2_persian_ci ucs2_polish_ci ucs2_romanian_ci)
      define(UCSC6, ucs2_slovak_ci ucs2_slovenian_ci)
      define(UCSC7, ucs2_spanish2_ci ucs2_spanish_ci)
      define(UCSC8, ucs2_swedish_ci ucs2_turkish_ci)
      define(UCSC9, ucs2_unicode_ci)
      UCSC="UCSC1 UCSC2 UCSC3 UCSC4 UCSC5 UCSC6 UCSC7 UCSC8 UCSC9"
      default_charset_collations="$UCSC"
      ;;
    ujis)
      default_charset_default_collation="ujis_japanese_ci"
      default_charset_collations="ujis_japanese_ci ujis_bin"
      ;;
    utf8)
      default_charset_default_collation="utf8_general_ci"
      define(UTFC1, utf8_general_ci utf8_bin)
      define(UTFC2, utf8_czech_ci utf8_danish_ci)
      define(UTFC3, utf8_estonian_ci utf8_icelandic_ci)
      define(UTFC4, utf8_latvian_ci utf8_lithuanian_ci)
      define(UTFC5, utf8_persian_ci utf8_polish_ci utf8_romanian_ci)
      define(UTFC6, utf8_slovak_ci utf8_slovenian_ci)
      define(UTFC7, utf8_spanish2_ci utf8_spanish_ci)
      define(UTFC8, utf8_swedish_ci utf8_turkish_ci)
      define(UTFC9, utf8_unicode_ci)
      UTFC="UTFC1 UTFC2 UTFC3 UTFC4 UTFC5 UTFC6 UTFC7 UTFC8 UTFC9"
      default_charset_collations="$UTFC"
      ;;
    *)
      AC_MSG_ERROR([Charset $cs not available. (Available are: $CHARSETS_AVAILABLE).
      See the Installation chapter in the Reference Manual.]);
esac

if test "$default_collation" = default; then
  default_collation=$default_charset_default_collation
fi

valid_default_collation=no
for cl in $default_charset_collations
do
  if test x"$cl" = x"$default_collation"
  then
    valid_default_collation=yes
    break
  fi
done

if test x$valid_default_collation = xyes
then
  AC_MSG_RESULT([default: $default_charset, collation: $default_collation; compiled in: $CHARSETS])
else
  AC_MSG_ERROR([
      Collation $default_collation is not valid for character set $default_charset.
      Valid collations are: $default_charset_collations.
      See the Installation chapter in the Reference Manual.
  ]);
fi

AC_DEFINE_UNQUOTED([MYSQL_DEFAULT_CHARSET_NAME], ["$default_charset"],
                   [Define the default charset name])
AC_DEFINE_UNQUOTED([MYSQL_DEFAULT_COLLATION_NAME], ["$default_collation"],
                   [Define the default charset name])
