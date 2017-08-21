// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 2015, International Business Machines Corporation
 * and others. All Rights Reserved.
 ********************************************************************/
/* C API TEST for UListFormatter */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/ustring.h"
#include "unicode/ulistformatter.h"
#include "cintltst.h"
#include "cmemory.h"
#include "cstring.h"

static void TestUListFmt(void);

void addUListFmtTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/ulistfmttest/" #x)

void addUListFmtTest(TestNode** root)
{
    TESTCASE(TestUListFmt);
}

static const UChar str0[] = { 0x41,0 }; /* "A" */
static const UChar str1[] = { 0x42,0x62,0 }; /* "Bb" */
static const UChar str2[] = { 0x43,0x63,0x63,0 }; /* "Ccc" */
static const UChar str3[] = { 0x44,0x64,0x64,0x64,0 }; /* "Dddd" */
static const UChar str4[] = { 0x45,0x65,0x65,0x65,0x65,0 }; /* "Eeeee" */
static const UChar* strings[] =            { str0, str1, str2, str3, str4 };
static const int32_t stringLengths[]  =    {    1,    2,    3,    4,    5 };
static const int32_t stringLengthsNeg[] = {   -1,   -1,   -1,   -1,   -1 };

typedef struct {
  const char * locale;
  int32_t stringCount;
  const char *expectedResult; /* invariant chars + escaped Unicode */
} ListFmtTestEntry;

static ListFmtTestEntry listFmtTestEntries[] = {
    /* locale stringCount expectedResult */
    { "en" ,  5,          "A, Bb, Ccc, Dddd, and Eeeee" },
    { "en" ,  2,          "A and Bb" },
    { "de" ,  5,          "A, Bb, Ccc, Dddd und Eeeee" },
    { "de" ,  2,          "A und Bb" },
    { "ja" ,  5,          "A\\u3001Bb\\u3001Ccc\\u3001Dddd\\u3001Eeeee" },
    { "ja" ,  2,          "A\\u3001Bb" },
    { "zh" ,  5,          "A\\u3001Bb\\u3001Ccc\\u3001Dddd\\u548CEeeee" },
    { "zh" ,  2,          "A\\u548CBb" },
    { NULL ,  0,          NULL } /* terminator */
    };

enum {
  kUBufMax = 128,
  kBBufMax = 256
};

static void TestUListFmt() {
    const ListFmtTestEntry * lftep;
    for (lftep = listFmtTestEntries; lftep->locale != NULL ; lftep++ ) {
        UErrorCode status = U_ZERO_ERROR;
        UListFormatter *listfmt = ulistfmt_open(lftep->locale, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("ERROR: ulistfmt_open fails for locale %s, status %s\n", lftep->locale, u_errorName(status));
        } else {
            UChar ubufActual[kUBufMax];
            int32_t ulenActual = ulistfmt_format(listfmt, strings, stringLengths, lftep->stringCount, ubufActual, kUBufMax, &status);
            if ( U_FAILURE(status) ) {
                log_err("ERROR: ulistfmt_format fails for locale %s count %d (real lengths), status %s\n", lftep->locale, lftep->stringCount, u_errorName(status));
            } else {
                UChar ubufExpected[kUBufMax];
                int32_t ulenExpected = u_unescape(lftep->expectedResult, ubufExpected, kUBufMax);
                if (ulenActual != ulenExpected || u_strncmp(ubufActual, ubufExpected, ulenExpected) != 0) {
                    log_err("ERROR: ulistfmt_format for locale %s count %d (real lengths), actual \"%s\" != expected \"%s\"\n", lftep->locale,
                            lftep->stringCount, aescstrdup(ubufActual, ulenActual), aescstrdup(ubufExpected, ulenExpected));
                }
            }
            /* try again with all lengths -1 */
            status = U_ZERO_ERROR;
            ulenActual = ulistfmt_format(listfmt, strings, stringLengthsNeg, lftep->stringCount, ubufActual, kUBufMax, &status);
            if ( U_FAILURE(status) ) {
                log_err("ERROR: ulistfmt_format fails for locale %s count %d (-1 lengths), status %s\n", lftep->locale, lftep->stringCount, u_errorName(status));
            } else {
                UChar ubufExpected[kUBufMax];
                int32_t ulenExpected = u_unescape(lftep->expectedResult, ubufExpected, kUBufMax);
                if (ulenActual != ulenExpected || u_strncmp(ubufActual, ubufExpected, ulenExpected) != 0) {
                    log_err("ERROR: ulistfmt_format for locale %s count %d (-1   lengths), actual \"%s\" != expected \"%s\"\n", lftep->locale,
                            lftep->stringCount, aescstrdup(ubufActual, ulenActual), aescstrdup(ubufExpected, ulenExpected));
                }
            }
            /* try again with NULL lengths */
            status = U_ZERO_ERROR;
            ulenActual = ulistfmt_format(listfmt, strings, NULL, lftep->stringCount, ubufActual, kUBufMax, &status);
            if ( U_FAILURE(status) ) {
                log_err("ERROR: ulistfmt_format fails for locale %s count %d (NULL lengths), status %s\n", lftep->locale, lftep->stringCount, u_errorName(status));
            } else {
                UChar ubufExpected[kUBufMax];
                int32_t ulenExpected = u_unescape(lftep->expectedResult, ubufExpected, kUBufMax);
                if (ulenActual != ulenExpected || u_strncmp(ubufActual, ubufExpected, ulenExpected) != 0) {
                    log_err("ERROR: ulistfmt_format for locale %s count %d (NULL lengths), actual \"%s\" != expected \"%s\"\n", lftep->locale,
                            lftep->stringCount, aescstrdup(ubufActual, ulenActual), aescstrdup(ubufExpected, ulenExpected));
                }
            }
            
            /* try calls that should return error */
            status = U_ZERO_ERROR;
            ulenActual = ulistfmt_format(listfmt, NULL, NULL, lftep->stringCount, ubufActual, kUBufMax, &status);
            if (status != U_ILLEGAL_ARGUMENT_ERROR || ulenActual > 0) {
                log_err("ERROR: ulistfmt_format for locale %s count %d with NULL strings, expected U_ILLEGAL_ARGUMENT_ERROR, got %s, result %d\n", lftep->locale,
                        lftep->stringCount, u_errorName(status), ulenActual);
            }
            status = U_ZERO_ERROR;
            ulenActual = ulistfmt_format(listfmt, strings, NULL, lftep->stringCount, NULL, kUBufMax, &status);
            if (status != U_ILLEGAL_ARGUMENT_ERROR || ulenActual > 0) {
                log_err("ERROR: ulistfmt_format for locale %s count %d with NULL result, expected U_ILLEGAL_ARGUMENT_ERROR, got %s, result %d\n", lftep->locale,
                        lftep->stringCount, u_errorName(status), ulenActual);
            }

            ulistfmt_close(listfmt);
        }
    }
}


#endif /* #if !UCONFIG_NO_FORMATTING */
