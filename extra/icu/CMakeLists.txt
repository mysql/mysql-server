# Copyright (c) 2017, 2022, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# This CMake file builds ICU.
# We build:
# - libicustubdata.a, a stub library for data, which we don't use.
# - libicui18n.a, the internationalization library.
# - libicuuc.a, a common utility library.
#
# To save some space and time, we have removed directories which are not
# needed by MySQL:
#   source/allinone
#   source/config
#   source/data
#   source/extra
#   source/layoutex
#   source/samples
#   source/test
#   source/tools


PROJECT (ICU)

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
IF(HAS_MISSING_PROFILE)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_MISSING_PROFILE}")
ENDIF()

IF(MY_COMPILER_IS_GNU_OR_CLANG)
  SET(ICU_LINUX_COMPILE_OPTIONS
    "-Wno-undef"
    "-Wno-deprecated-declarations"
    "-Wno-error"
    "-Wno-unused-parameter"
    "-Wno-missing-field-initializers"
    "-Wno-sign-compare"
    "-Wno-type-limits"
  )
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wreturn-local-addr" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

IF(MY_COMPILER_IS_GNU)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "-Wno-stringop-overflow")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wunused-but-set-variable" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmisleading-indentation" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmaybe-uninitialized" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wimplicit-int-float-conversion" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wrestrict" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wstringop-truncation" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wdocumentation" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

IF(WIN32)
  ADD_DEFINITIONS(
    -DU_STATIC_IMPLEMENTATION=1
    -DU_IMPORT=
    -DU_EXPORT=
  )
ENDIF()

IF(MSVC AND NOT WIN32_CLANG)
  # MSVC needs /utf-8 compiler switch if source file has no BOM
  STRING_APPEND(CMAKE_CXX_FLAGS " /utf-8")
ENDIF()

#TODO(Bug#33986227): fix the deprecation for ICU
IF(MSVC)
  STRING_APPEND(CMAKE_C_FLAGS " /wd4996")
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4996")
ENDIF()

IF(WIN32_CLANG)
  STRING_APPEND(CMAKE_CXX_FLAGS " -Wno-unused-function")
ENDIF()

IF(WITH_ICU STREQUAL "bundled")
  REMOVE_CMAKE_COMPILER_FLAGS(CMAKE_C_FLAGS "${MY_COMPILER_FLAG_FLTO}")
  REMOVE_CMAKE_COMPILER_FLAGS(CMAKE_CXX_FLAGS "${MY_COMPILER_FLAG_FLTO}")
ENDIF()

SET(ICU_COMMON_SOURCES
  ${ICU_VERSION_DIR}/source/common/appendable.cpp
  ${ICU_VERSION_DIR}/source/common/bmpset.cpp
  ${ICU_VERSION_DIR}/source/common/brkeng.cpp
  ${ICU_VERSION_DIR}/source/common/brkiter.cpp
  ${ICU_VERSION_DIR}/source/common/bytesinkutil.cpp
  ${ICU_VERSION_DIR}/source/common/bytestream.cpp
  ${ICU_VERSION_DIR}/source/common/bytestriebuilder.cpp
  ${ICU_VERSION_DIR}/source/common/bytestrie.cpp
  ${ICU_VERSION_DIR}/source/common/bytestrieiterator.cpp
  ${ICU_VERSION_DIR}/source/common/caniter.cpp
  ${ICU_VERSION_DIR}/source/common/characterproperties.cpp
  ${ICU_VERSION_DIR}/source/common/chariter.cpp
  ${ICU_VERSION_DIR}/source/common/charstr.cpp
  ${ICU_VERSION_DIR}/source/common/cmemory.cpp
  ${ICU_VERSION_DIR}/source/common/cstr.cpp
  ${ICU_VERSION_DIR}/source/common/cstring.cpp
  ${ICU_VERSION_DIR}/source/common/cwchar.cpp
  ${ICU_VERSION_DIR}/source/common/dictbe.cpp
  ${ICU_VERSION_DIR}/source/common/dictionarydata.cpp
  ${ICU_VERSION_DIR}/source/common/dtintrv.cpp
  ${ICU_VERSION_DIR}/source/common/edits.cpp
  ${ICU_VERSION_DIR}/source/common/errorcode.cpp
  ${ICU_VERSION_DIR}/source/common/filteredbrk.cpp
  ${ICU_VERSION_DIR}/source/common/filterednormalizer2.cpp
  ${ICU_VERSION_DIR}/source/common/icudataver.cpp
  ${ICU_VERSION_DIR}/source/common/icuplug.cpp
  ${ICU_VERSION_DIR}/source/common/loadednormalizer2impl.cpp
  ${ICU_VERSION_DIR}/source/common/localebuilder.cpp
  ${ICU_VERSION_DIR}/source/common/localematcher.cpp
  ${ICU_VERSION_DIR}/source/common/localeprioritylist.cpp
  ${ICU_VERSION_DIR}/source/common/locavailable.cpp
  ${ICU_VERSION_DIR}/source/common/locbased.cpp
  ${ICU_VERSION_DIR}/source/common/locdispnames.cpp
  ${ICU_VERSION_DIR}/source/common/locdistance.cpp
  ${ICU_VERSION_DIR}/source/common/locdspnm.cpp
  ${ICU_VERSION_DIR}/source/common/locid.cpp
  ${ICU_VERSION_DIR}/source/common/loclikely.cpp
  ${ICU_VERSION_DIR}/source/common/loclikelysubtags.cpp
  ${ICU_VERSION_DIR}/source/common/locmap.cpp
  ${ICU_VERSION_DIR}/source/common/locresdata.cpp
  ${ICU_VERSION_DIR}/source/common/locutil.cpp
  ${ICU_VERSION_DIR}/source/common/lsr.cpp
  ${ICU_VERSION_DIR}/source/common/messagepattern.cpp
  ${ICU_VERSION_DIR}/source/common/normalizer2.cpp
  ${ICU_VERSION_DIR}/source/common/normalizer2impl.cpp
  ${ICU_VERSION_DIR}/source/common/normlzr.cpp
  ${ICU_VERSION_DIR}/source/common/parsepos.cpp
  ${ICU_VERSION_DIR}/source/common/patternprops.cpp
  ${ICU_VERSION_DIR}/source/common/pluralmap.cpp
  ${ICU_VERSION_DIR}/source/common/propname.cpp
  ${ICU_VERSION_DIR}/source/common/propsvec.cpp
  ${ICU_VERSION_DIR}/source/common/punycode.cpp
  ${ICU_VERSION_DIR}/source/common/putil.cpp
  ${ICU_VERSION_DIR}/source/common/rbbi_cache.cpp
  ${ICU_VERSION_DIR}/source/common/rbbi.cpp
  ${ICU_VERSION_DIR}/source/common/rbbidata.cpp
  ${ICU_VERSION_DIR}/source/common/rbbinode.cpp
  ${ICU_VERSION_DIR}/source/common/rbbirb.cpp
  ${ICU_VERSION_DIR}/source/common/rbbiscan.cpp
  ${ICU_VERSION_DIR}/source/common/rbbisetb.cpp
  ${ICU_VERSION_DIR}/source/common/rbbistbl.cpp
  ${ICU_VERSION_DIR}/source/common/rbbitblb.cpp
  ${ICU_VERSION_DIR}/source/common/resbund_cnv.cpp
  ${ICU_VERSION_DIR}/source/common/resbund.cpp
  ${ICU_VERSION_DIR}/source/common/resource.cpp
  ${ICU_VERSION_DIR}/source/common/restrace.cpp
  ${ICU_VERSION_DIR}/source/common/ruleiter.cpp
  ${ICU_VERSION_DIR}/source/common/schriter.cpp
  ${ICU_VERSION_DIR}/source/common/serv.cpp
  ${ICU_VERSION_DIR}/source/common/servlk.cpp
  ${ICU_VERSION_DIR}/source/common/servlkf.cpp
  ${ICU_VERSION_DIR}/source/common/servls.cpp
  ${ICU_VERSION_DIR}/source/common/servnotf.cpp
  ${ICU_VERSION_DIR}/source/common/servrbf.cpp
  ${ICU_VERSION_DIR}/source/common/servslkf.cpp
  ${ICU_VERSION_DIR}/source/common/sharedobject.cpp
  ${ICU_VERSION_DIR}/source/common/simpleformatter.cpp
  ${ICU_VERSION_DIR}/source/common/static_unicode_sets.cpp
  ${ICU_VERSION_DIR}/source/common/stringpiece.cpp
  ${ICU_VERSION_DIR}/source/common/stringtriebuilder.cpp
  ${ICU_VERSION_DIR}/source/common/uarrsort.cpp
  ${ICU_VERSION_DIR}/source/common/ubidi.cpp
  ${ICU_VERSION_DIR}/source/common/ubidiln.cpp
  ${ICU_VERSION_DIR}/source/common/ubidi_props.cpp
  ${ICU_VERSION_DIR}/source/common/ubiditransform.cpp
  ${ICU_VERSION_DIR}/source/common/ubidiwrt.cpp
  ${ICU_VERSION_DIR}/source/common/ubrk.cpp
  ${ICU_VERSION_DIR}/source/common/ucase.cpp
  ${ICU_VERSION_DIR}/source/common/ucasemap.cpp
  ${ICU_VERSION_DIR}/source/common/ucasemap_titlecase_brkiter.cpp
  ${ICU_VERSION_DIR}/source/common/ucat.cpp
  ${ICU_VERSION_DIR}/source/common/uchar.cpp
  ${ICU_VERSION_DIR}/source/common/ucharstriebuilder.cpp
  ${ICU_VERSION_DIR}/source/common/ucharstrie.cpp
  ${ICU_VERSION_DIR}/source/common/ucharstrieiterator.cpp
  ${ICU_VERSION_DIR}/source/common/uchriter.cpp
  ${ICU_VERSION_DIR}/source/common/ucln_cmn.cpp
  ${ICU_VERSION_DIR}/source/common/ucmndata.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv2022.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_bld.cpp
  ${ICU_VERSION_DIR}/source/common/ucnvbocu.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_cb.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_cnv.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_ct.cpp
  ${ICU_VERSION_DIR}/source/common/ucnvdisp.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_err.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_ext.cpp
  ${ICU_VERSION_DIR}/source/common/ucnvhz.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_io.cpp
  ${ICU_VERSION_DIR}/source/common/ucnvisci.cpp
  ${ICU_VERSION_DIR}/source/common/ucnvlat1.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_lmb.cpp
  ${ICU_VERSION_DIR}/source/common/ucnvmbcs.cpp
  ${ICU_VERSION_DIR}/source/common/ucnvscsu.cpp
  ${ICU_VERSION_DIR}/source/common/ucnvsel.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_set.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_u16.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_u32.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_u7.cpp
  ${ICU_VERSION_DIR}/source/common/ucnv_u8.cpp
  ${ICU_VERSION_DIR}/source/common/ucol_swp.cpp
  ${ICU_VERSION_DIR}/source/common/ucptrie.cpp
  ${ICU_VERSION_DIR}/source/common/ucurr.cpp
  ${ICU_VERSION_DIR}/source/common/udata.cpp
  ${ICU_VERSION_DIR}/source/common/udatamem.cpp
  ${ICU_VERSION_DIR}/source/common/udataswp.cpp
  ${ICU_VERSION_DIR}/source/common/uenum.cpp
  ${ICU_VERSION_DIR}/source/common/uhash.cpp
  ${ICU_VERSION_DIR}/source/common/uhash_us.cpp
  ${ICU_VERSION_DIR}/source/common/uidna.cpp
  ${ICU_VERSION_DIR}/source/common/uinit.cpp
  ${ICU_VERSION_DIR}/source/common/uinvchar.cpp
  ${ICU_VERSION_DIR}/source/common/uiter.cpp
  ${ICU_VERSION_DIR}/source/common/ulist.cpp
  ${ICU_VERSION_DIR}/source/common/uloc.cpp
  ${ICU_VERSION_DIR}/source/common/uloc_keytype.cpp
  ${ICU_VERSION_DIR}/source/common/uloc_tag.cpp
  ${ICU_VERSION_DIR}/source/common/umapfile.cpp
  ${ICU_VERSION_DIR}/source/common/umath.cpp
  ${ICU_VERSION_DIR}/source/common/umutablecptrie.cpp
  ${ICU_VERSION_DIR}/source/common/umutex.cpp
  ${ICU_VERSION_DIR}/source/common/unames.cpp
  ${ICU_VERSION_DIR}/source/common/unifiedcache.cpp
  ${ICU_VERSION_DIR}/source/common/unifilt.cpp
  ${ICU_VERSION_DIR}/source/common/unifunct.cpp
  ${ICU_VERSION_DIR}/source/common/uniset_closure.cpp
  ${ICU_VERSION_DIR}/source/common/uniset.cpp
  ${ICU_VERSION_DIR}/source/common/uniset_props.cpp
  ${ICU_VERSION_DIR}/source/common/unisetspan.cpp
  ${ICU_VERSION_DIR}/source/common/unistr_case.cpp
  ${ICU_VERSION_DIR}/source/common/unistr_case_locale.cpp
  ${ICU_VERSION_DIR}/source/common/unistr_cnv.cpp
  ${ICU_VERSION_DIR}/source/common/unistr.cpp
  ${ICU_VERSION_DIR}/source/common/unistr_props.cpp
  ${ICU_VERSION_DIR}/source/common/unistr_titlecase_brkiter.cpp
  ${ICU_VERSION_DIR}/source/common/unormcmp.cpp
  ${ICU_VERSION_DIR}/source/common/unorm.cpp
  ${ICU_VERSION_DIR}/source/common/uobject.cpp
  ${ICU_VERSION_DIR}/source/common/uprops.cpp
  ${ICU_VERSION_DIR}/source/common/uresbund.cpp
  ${ICU_VERSION_DIR}/source/common/ures_cnv.cpp
  ${ICU_VERSION_DIR}/source/common/uresdata.cpp
  ${ICU_VERSION_DIR}/source/common/usc_impl.cpp
  ${ICU_VERSION_DIR}/source/common/uscript.cpp
  ${ICU_VERSION_DIR}/source/common/uscript_props.cpp
  ${ICU_VERSION_DIR}/source/common/uset.cpp
  ${ICU_VERSION_DIR}/source/common/usetiter.cpp
  ${ICU_VERSION_DIR}/source/common/uset_props.cpp
  ${ICU_VERSION_DIR}/source/common/ushape.cpp
  ${ICU_VERSION_DIR}/source/common/usprep.cpp
  ${ICU_VERSION_DIR}/source/common/ustack.cpp
  ${ICU_VERSION_DIR}/source/common/ustrcase.cpp
  ${ICU_VERSION_DIR}/source/common/ustrcase_locale.cpp
  ${ICU_VERSION_DIR}/source/common/ustr_cnv.cpp
  ${ICU_VERSION_DIR}/source/common/ustrenum.cpp
  ${ICU_VERSION_DIR}/source/common/ustrfmt.cpp
  ${ICU_VERSION_DIR}/source/common/ustring.cpp
  ${ICU_VERSION_DIR}/source/common/ustr_titlecase_brkiter.cpp
  ${ICU_VERSION_DIR}/source/common/ustrtrns.cpp
  ${ICU_VERSION_DIR}/source/common/ustr_wcs.cpp
  ${ICU_VERSION_DIR}/source/common/utext.cpp
  ${ICU_VERSION_DIR}/source/common/utf_impl.cpp
  ${ICU_VERSION_DIR}/source/common/util.cpp
  ${ICU_VERSION_DIR}/source/common/util_props.cpp
  ${ICU_VERSION_DIR}/source/common/utrace.cpp
  ${ICU_VERSION_DIR}/source/common/utrie2_builder.cpp
  ${ICU_VERSION_DIR}/source/common/utrie2.cpp
  ${ICU_VERSION_DIR}/source/common/utrie.cpp
  ${ICU_VERSION_DIR}/source/common/utrie_swap.cpp
  ${ICU_VERSION_DIR}/source/common/uts46.cpp
  ${ICU_VERSION_DIR}/source/common/utypes.cpp
  ${ICU_VERSION_DIR}/source/common/uvector.cpp
  ${ICU_VERSION_DIR}/source/common/uvectr32.cpp
  ${ICU_VERSION_DIR}/source/common/uvectr64.cpp
  ${ICU_VERSION_DIR}/source/common/wintz.cpp
)

IF(APPLE)
  LIST(REMOVE_ITEM ICU_COMMON_SOURCES
    ${ICU_VERSION_DIR}/source/common/cwchar.cpp
    ${ICU_VERSION_DIR}/source/common/icuplug.cpp
    ${ICU_VERSION_DIR}/source/common/restrace.cpp
    ${ICU_VERSION_DIR}/source/common/wintz.cpp
    )
ENDIF()

SET(ICU_I18N_SOURCES
  ${ICU_VERSION_DIR}/source/i18n/alphaindex.cpp
  ${ICU_VERSION_DIR}/source/i18n/anytrans.cpp
  ${ICU_VERSION_DIR}/source/i18n/astro.cpp
  ${ICU_VERSION_DIR}/source/i18n/basictz.cpp
  ${ICU_VERSION_DIR}/source/i18n/bocsu.cpp
  ${ICU_VERSION_DIR}/source/i18n/brktrans.cpp
  ${ICU_VERSION_DIR}/source/i18n/buddhcal.cpp
  ${ICU_VERSION_DIR}/source/i18n/calendar.cpp
  ${ICU_VERSION_DIR}/source/i18n/casetrn.cpp
  ${ICU_VERSION_DIR}/source/i18n/cecal.cpp
  ${ICU_VERSION_DIR}/source/i18n/chnsecal.cpp
  ${ICU_VERSION_DIR}/source/i18n/choicfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/coleitr.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationbuilder.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationcompare.cpp
  ${ICU_VERSION_DIR}/source/i18n/collation.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationdatabuilder.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationdata.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationdatareader.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationdatawriter.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationfastlatinbuilder.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationfastlatin.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationfcd.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationiterator.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationkeys.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationroot.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationrootelements.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationruleparser.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationsets.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationsettings.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationtailoring.cpp
  ${ICU_VERSION_DIR}/source/i18n/collationweights.cpp
  ${ICU_VERSION_DIR}/source/i18n/coll.cpp
  ${ICU_VERSION_DIR}/source/i18n/compactdecimalformat.cpp
  ${ICU_VERSION_DIR}/source/i18n/coptccal.cpp
  ${ICU_VERSION_DIR}/source/i18n/cpdtrans.cpp
  ${ICU_VERSION_DIR}/source/i18n/csdetect.cpp
  ${ICU_VERSION_DIR}/source/i18n/csmatch.cpp
  ${ICU_VERSION_DIR}/source/i18n/csr2022.cpp
  ${ICU_VERSION_DIR}/source/i18n/csrecog.cpp
  ${ICU_VERSION_DIR}/source/i18n/csrmbcs.cpp
  ${ICU_VERSION_DIR}/source/i18n/csrsbcs.cpp
  ${ICU_VERSION_DIR}/source/i18n/csrucode.cpp
  ${ICU_VERSION_DIR}/source/i18n/csrutf8.cpp
  ${ICU_VERSION_DIR}/source/i18n/curramt.cpp
  ${ICU_VERSION_DIR}/source/i18n/currfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/currpinf.cpp
  ${ICU_VERSION_DIR}/source/i18n/currunit.cpp
  ${ICU_VERSION_DIR}/source/i18n/dangical.cpp
  ${ICU_VERSION_DIR}/source/i18n/datefmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/dayperiodrules.cpp
  ${ICU_VERSION_DIR}/source/i18n/dcfmtsym.cpp
  ${ICU_VERSION_DIR}/source/i18n/decContext.cpp
  ${ICU_VERSION_DIR}/source/i18n/decimfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/decNumber.cpp
  ${ICU_VERSION_DIR}/source/i18n/double-conversion-bignum.cpp
  ${ICU_VERSION_DIR}/source/i18n/double-conversion-bignum-dtoa.cpp
  ${ICU_VERSION_DIR}/source/i18n/double-conversion-cached-powers.cpp
  ${ICU_VERSION_DIR}/source/i18n/double-conversion-double-to-string.cpp
  ${ICU_VERSION_DIR}/source/i18n/double-conversion-fast-dtoa.cpp
  ${ICU_VERSION_DIR}/source/i18n/double-conversion-string-to-double.cpp
  ${ICU_VERSION_DIR}/source/i18n/double-conversion-strtod.cpp
  ${ICU_VERSION_DIR}/source/i18n/dtfmtsym.cpp
  ${ICU_VERSION_DIR}/source/i18n/dtitvfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/dtitvinf.cpp
  ${ICU_VERSION_DIR}/source/i18n/dtptngen.cpp
  ${ICU_VERSION_DIR}/source/i18n/dtrule.cpp
  ${ICU_VERSION_DIR}/source/i18n/erarules.cpp
  ${ICU_VERSION_DIR}/source/i18n/esctrn.cpp
  ${ICU_VERSION_DIR}/source/i18n/ethpccal.cpp
  ${ICU_VERSION_DIR}/source/i18n/fmtable_cnv.cpp
  ${ICU_VERSION_DIR}/source/i18n/fmtable.cpp
  ${ICU_VERSION_DIR}/source/i18n/format.cpp
  ${ICU_VERSION_DIR}/source/i18n/formatted_string_builder.cpp
  ${ICU_VERSION_DIR}/source/i18n/formattedval_iterimpl.cpp
  ${ICU_VERSION_DIR}/source/i18n/formattedval_sbimpl.cpp
  ${ICU_VERSION_DIR}/source/i18n/formattedvalue.cpp
  ${ICU_VERSION_DIR}/source/i18n/fphdlimp.cpp
  ${ICU_VERSION_DIR}/source/i18n/fpositer.cpp
  ${ICU_VERSION_DIR}/source/i18n/funcrepl.cpp
  ${ICU_VERSION_DIR}/source/i18n/gender.cpp
  ${ICU_VERSION_DIR}/source/i18n/gregocal.cpp
  ${ICU_VERSION_DIR}/source/i18n/gregoimp.cpp
  ${ICU_VERSION_DIR}/source/i18n/hebrwcal.cpp
  ${ICU_VERSION_DIR}/source/i18n/indiancal.cpp
  ${ICU_VERSION_DIR}/source/i18n/inputext.cpp
  ${ICU_VERSION_DIR}/source/i18n/islamcal.cpp
  ${ICU_VERSION_DIR}/source/i18n/japancal.cpp
  ${ICU_VERSION_DIR}/source/i18n/listformatter.cpp
  ${ICU_VERSION_DIR}/source/i18n/measfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/measunit.cpp
  ${ICU_VERSION_DIR}/source/i18n/measunit_extra.cpp
  ${ICU_VERSION_DIR}/source/i18n/measure.cpp
  ${ICU_VERSION_DIR}/source/i18n/msgfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/name2uni.cpp
  ${ICU_VERSION_DIR}/source/i18n/nfrs.cpp
  ${ICU_VERSION_DIR}/source/i18n/nfrule.cpp
  ${ICU_VERSION_DIR}/source/i18n/nfsubs.cpp
  ${ICU_VERSION_DIR}/source/i18n/nortrans.cpp
  ${ICU_VERSION_DIR}/source/i18n/nultrans.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_affixutils.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_asformat.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_capi.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_compact.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_currencysymbols.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_decimalquantity.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_decimfmtprops.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_fluent.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_formatimpl.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_grouping.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_integerwidth.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_longnames.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_mapper.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_modifiers.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_multiplier.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_notation.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_output.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_padding.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_patternmodifier.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_patternstring.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_rounding.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_scientific.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_skeletons.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_symbolswrapper.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_usageprefs.cpp
  ${ICU_VERSION_DIR}/source/i18n/number_utils.cpp
  ${ICU_VERSION_DIR}/source/i18n/numfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/numparse_affixes.cpp
  ${ICU_VERSION_DIR}/source/i18n/numparse_compositions.cpp
  ${ICU_VERSION_DIR}/source/i18n/numparse_currency.cpp
  ${ICU_VERSION_DIR}/source/i18n/numparse_decimal.cpp
  ${ICU_VERSION_DIR}/source/i18n/numparse_impl.cpp
  ${ICU_VERSION_DIR}/source/i18n/numparse_parsednumber.cpp
  ${ICU_VERSION_DIR}/source/i18n/numparse_scientific.cpp
  ${ICU_VERSION_DIR}/source/i18n/numparse_symbols.cpp
  ${ICU_VERSION_DIR}/source/i18n/numparse_validators.cpp
  ${ICU_VERSION_DIR}/source/i18n/numrange_capi.cpp
  ${ICU_VERSION_DIR}/source/i18n/numrange_fluent.cpp
  ${ICU_VERSION_DIR}/source/i18n/numrange_impl.cpp
  ${ICU_VERSION_DIR}/source/i18n/numsys.cpp
  ${ICU_VERSION_DIR}/source/i18n/olsontz.cpp
  ${ICU_VERSION_DIR}/source/i18n/persncal.cpp
  ${ICU_VERSION_DIR}/source/i18n/pluralranges.cpp
  ${ICU_VERSION_DIR}/source/i18n/plurfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/plurrule.cpp
  ${ICU_VERSION_DIR}/source/i18n/quant.cpp
  ${ICU_VERSION_DIR}/source/i18n/quantityformatter.cpp
  ${ICU_VERSION_DIR}/source/i18n/rbnf.cpp
  ${ICU_VERSION_DIR}/source/i18n/rbt.cpp
  ${ICU_VERSION_DIR}/source/i18n/rbt_data.cpp
  ${ICU_VERSION_DIR}/source/i18n/rbt_pars.cpp
  ${ICU_VERSION_DIR}/source/i18n/rbt_rule.cpp
  ${ICU_VERSION_DIR}/source/i18n/rbt_set.cpp
  ${ICU_VERSION_DIR}/source/i18n/rbtz.cpp
  ${ICU_VERSION_DIR}/source/i18n/regexcmp.cpp
  ${ICU_VERSION_DIR}/source/i18n/regeximp.cpp
  ${ICU_VERSION_DIR}/source/i18n/regexst.cpp
  ${ICU_VERSION_DIR}/source/i18n/regextxt.cpp
  ${ICU_VERSION_DIR}/source/i18n/region.cpp
  ${ICU_VERSION_DIR}/source/i18n/reldatefmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/reldtfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/rematch.cpp
  ${ICU_VERSION_DIR}/source/i18n/remtrans.cpp
  ${ICU_VERSION_DIR}/source/i18n/repattrn.cpp
  ${ICU_VERSION_DIR}/source/i18n/rulebasedcollator.cpp
  ${ICU_VERSION_DIR}/source/i18n/scientificnumberformatter.cpp
  ${ICU_VERSION_DIR}/source/i18n/scriptset.cpp
  ${ICU_VERSION_DIR}/source/i18n/search.cpp
  ${ICU_VERSION_DIR}/source/i18n/selfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/sharedbreakiterator.cpp
  ${ICU_VERSION_DIR}/source/i18n/simpletz.cpp
  ${ICU_VERSION_DIR}/source/i18n/smpdtfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/smpdtfst.cpp
  ${ICU_VERSION_DIR}/source/i18n/sortkey.cpp
  ${ICU_VERSION_DIR}/source/i18n/standardplural.cpp
  ${ICU_VERSION_DIR}/source/i18n/string_segment.cpp
  ${ICU_VERSION_DIR}/source/i18n/strmatch.cpp
  ${ICU_VERSION_DIR}/source/i18n/strrepl.cpp
  ${ICU_VERSION_DIR}/source/i18n/stsearch.cpp
  ${ICU_VERSION_DIR}/source/i18n/taiwncal.cpp
  ${ICU_VERSION_DIR}/source/i18n/timezone.cpp
  ${ICU_VERSION_DIR}/source/i18n/titletrn.cpp
  ${ICU_VERSION_DIR}/source/i18n/tmunit.cpp
  ${ICU_VERSION_DIR}/source/i18n/tmutamt.cpp
  ${ICU_VERSION_DIR}/source/i18n/tmutfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/tolowtrn.cpp
  ${ICU_VERSION_DIR}/source/i18n/toupptrn.cpp
  ${ICU_VERSION_DIR}/source/i18n/translit.cpp
  ${ICU_VERSION_DIR}/source/i18n/transreg.cpp
  ${ICU_VERSION_DIR}/source/i18n/tridpars.cpp
  ${ICU_VERSION_DIR}/source/i18n/tzfmt.cpp
  ${ICU_VERSION_DIR}/source/i18n/tzgnames.cpp
  ${ICU_VERSION_DIR}/source/i18n/tznames.cpp
  ${ICU_VERSION_DIR}/source/i18n/tznames_impl.cpp
  ${ICU_VERSION_DIR}/source/i18n/tzrule.cpp
  ${ICU_VERSION_DIR}/source/i18n/tztrans.cpp
  ${ICU_VERSION_DIR}/source/i18n/ucal.cpp
  ${ICU_VERSION_DIR}/source/i18n/ucln_in.cpp
  ${ICU_VERSION_DIR}/source/i18n/ucol.cpp
  ${ICU_VERSION_DIR}/source/i18n/ucoleitr.cpp
  ${ICU_VERSION_DIR}/source/i18n/ucol_res.cpp
  ${ICU_VERSION_DIR}/source/i18n/ucol_sit.cpp
  ${ICU_VERSION_DIR}/source/i18n/ucsdet.cpp
  ${ICU_VERSION_DIR}/source/i18n/udat.cpp
  ${ICU_VERSION_DIR}/source/i18n/udateintervalformat.cpp
  ${ICU_VERSION_DIR}/source/i18n/udatpg.cpp
  ${ICU_VERSION_DIR}/source/i18n/ufieldpositer.cpp
  ${ICU_VERSION_DIR}/source/i18n/uitercollationiterator.cpp
  ${ICU_VERSION_DIR}/source/i18n/ulistformatter.cpp
  ${ICU_VERSION_DIR}/source/i18n/ulocdata.cpp
  ${ICU_VERSION_DIR}/source/i18n/umsg.cpp
  ${ICU_VERSION_DIR}/source/i18n/unesctrn.cpp
  ${ICU_VERSION_DIR}/source/i18n/uni2name.cpp
  ${ICU_VERSION_DIR}/source/i18n/units_complexconverter.cpp
  ${ICU_VERSION_DIR}/source/i18n/units_converter.cpp
  ${ICU_VERSION_DIR}/source/i18n/units_data.cpp
  ${ICU_VERSION_DIR}/source/i18n/units_router.cpp
  ${ICU_VERSION_DIR}/source/i18n/unum.cpp
  ${ICU_VERSION_DIR}/source/i18n/unumsys.cpp
  ${ICU_VERSION_DIR}/source/i18n/upluralrules.cpp
  ${ICU_VERSION_DIR}/source/i18n/uregexc.cpp
  ${ICU_VERSION_DIR}/source/i18n/uregex.cpp
  ${ICU_VERSION_DIR}/source/i18n/uregion.cpp
  ${ICU_VERSION_DIR}/source/i18n/usearch.cpp
  ${ICU_VERSION_DIR}/source/i18n/uspoof_build.cpp
  ${ICU_VERSION_DIR}/source/i18n/uspoof_conf.cpp
  ${ICU_VERSION_DIR}/source/i18n/uspoof.cpp
  ${ICU_VERSION_DIR}/source/i18n/uspoof_impl.cpp
  ${ICU_VERSION_DIR}/source/i18n/utf16collationiterator.cpp
  ${ICU_VERSION_DIR}/source/i18n/utf8collationiterator.cpp
  ${ICU_VERSION_DIR}/source/i18n/utmscale.cpp
  ${ICU_VERSION_DIR}/source/i18n/utrans.cpp
  ${ICU_VERSION_DIR}/source/i18n/vtzone.cpp
  ${ICU_VERSION_DIR}/source/i18n/vzone.cpp
  ${ICU_VERSION_DIR}/source/i18n/zonemeta.cpp
  ${ICU_VERSION_DIR}/source/i18n/zrule.cpp
  ${ICU_VERSION_DIR}/source/i18n/ztrans.cpp
)

IF(WIN32)
  LIST(APPEND ICU_SOURCES
    ${ICU_VERSION_DIR}/source/i18n/windtfmt.cpp
    ${ICU_VERSION_DIR}/source/i18n/winnmfmt.cpp
    ${ICU_VERSION_DIR}/source/i18n/wintzimpl.cpp
    )
ENDIF()

ADD_LIBRARY(icuuc STATIC ${ICU_COMMON_SOURCES})
TARGET_COMPILE_DEFINITIONS(icuuc PRIVATE -DU_COMMON_IMPLEMENTATION)
TARGET_INCLUDE_DIRECTORIES(icuuc PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/${ICU_VERSION_DIR}/source/common"
  )

ADD_LIBRARY(icui18n STATIC ${ICU_I18N_SOURCES})
TARGET_COMPILE_DEFINITIONS(icui18n PRIVATE -DU_I18N_IMPLEMENTATION)
TARGET_INCLUDE_DIRECTORIES(icui18n PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/${ICU_VERSION_DIR}/source/common"
  "${CMAKE_CURRENT_SOURCE_DIR}/${ICU_VERSION_DIR}/source/i18n"
  )

ADD_LIBRARY(icustubdata STATIC
  ${ICU_VERSION_DIR}/source/stubdata/stubdata.cpp)
TARGET_INCLUDE_DIRECTORIES(icustubdata PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/${ICU_VERSION_DIR}/source/common"
  )

IF(LINUX OR SOLARIS)
  TARGET_COMPILE_OPTIONS(icuuc PRIVATE ${ICU_LINUX_COMPILE_OPTIONS})
  TARGET_COMPILE_OPTIONS(icui18n PRIVATE ${ICU_LINUX_COMPILE_OPTIONS})
  TARGET_COMPILE_OPTIONS(icustubdata PRIVATE ${ICU_LINUX_COMPILE_OPTIONS})
ENDIF()

# uidna_toASCII_65 is marked as deprecated
IF(MY_COMPILER_IS_GNU_OR_CLANG)
  ADD_COMPILE_FLAGS(
    ${ICU_VERSION_DIR}/source/common/uidna.cpp COMPILE_FLAGS "-Wno-deprecated-declarations")
ENDIF()

# Utility target to build all ICU libraries
ADD_CUSTOM_TARGET(icu_all)
ADD_DEPENDENCIES(icu_all icuuc icui18n icustubdata)

# To build data files on Linux (used for all little-endian platforms)
#
# tar xvf icu4c-69_1-src.tgz
# cd icu
# mkdir bin-files
# cd bin-files
# export CXXFLAGS=-std=c++11
# ../source/configure --with-data-packaging=files
# make
# cd data/out/build
# tar cvf icudt69l.tar icudt69l/brkitr icudt69l/unames.icu
#
# Solaris sparc, same thing, but:
# tar cvf icudt69b.tar icudt69b/brkitr icudt69b/unames.icu
#
# Copy the tarballs, unpack them, and do 'git add'.

INSTALL(DIRECTORY ${ICUDT_DIR}
  DESTINATION "${INSTALL_PRIV_LIBDIR}"
  COMPONENT SharedLibraries
  FILE_PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    GROUP_READ
    WORLD_READ
  )
