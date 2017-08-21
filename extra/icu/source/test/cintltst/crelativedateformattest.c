// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 2016, International Business Machines Corporation
 * and others. All Rights Reserved.
 ********************************************************************/
/* C API TEST FOR DATE INTERVAL FORMAT */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_BREAK_ITERATION

#include "unicode/ureldatefmt.h"
#include "unicode/unum.h"
#include "unicode/udisplaycontext.h"
#include "unicode/ustring.h"
#include "cintltst.h"
#include "cmemory.h"

static void TestRelDateFmt(void);
static void TestCombineDateTime(void);

void addRelativeDateFormatTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/crelativedateformattest/" #x)

void addRelativeDateFormatTest(TestNode** root)
{
    TESTCASE(TestRelDateFmt);
    TESTCASE(TestCombineDateTime);
}

static const double offsets[] = { -5.0, -2.2, -2.0, -1.0, -0.7, 0.0, 0.7, 1.0, 2.0, 5.0 };
enum { kNumOffsets = UPRV_LENGTHOF(offsets) };

static const char* en_decDef_long_midSent_week[kNumOffsets*2] = {
/*  text                    numeric */
    "5 weeks ago",          "5 weeks ago",        /* -5   */
    "2.2 weeks ago",        "2.2 weeks ago",      /* -2.2 */
    "2 weeks ago",          "2 weeks ago",        /* -2   */
    "last week",            "1 week ago",         /* -1   */
    "0.7 weeks ago",        "0.7 weeks ago",      /* -0.7 */
    "this week",            "in 0 weeks",         /*  0   */
    "in 0.7 weeks",         "in 0.7 weeks",       /*  0.7 */
    "next week",            "in 1 week",          /*  1   */
    "in 2 weeks",           "in 2 weeks",         /*  2   */
    "in 5 weeks",           "in 5 weeks"          /*  5   */
};

static const char* en_dec0_long_midSent_week[kNumOffsets*2] = {
/*  text                    numeric */
    "5 weeks ago",          "5 weeks ago",        /* -5   */
    "2 weeks ago",          "2 weeks ago",        /* -2.2 */
    "2 weeks ago",          "2 weeks ago",        /* -2  */
    "last week",            "1 week ago",         /* -1   */
    "0 weeks ago",          "0 weeks ago",        /* -0.7 */
    "this week",            "in 0 weeks",         /*  0   */
    "in 0 weeks",           "in 0 weeks",         /*  0.7 */
    "next week",            "in 1 week",          /*  1   */
    "in 2 weeks",           "in 2 weeks",         /*  2   */
    "in 5 weeks",           "in 5 weeks"          /*  5   */
};

static const char* en_decDef_short_midSent_week[kNumOffsets*2] = {
/*  text                    numeric */
    "5 wk. ago",            "5 wk. ago",          /* -5   */
    "2.2 wk. ago",          "2.2 wk. ago",        /* -2.2 */
    "2 wk. ago",            "2 wk. ago",          /* -2   */
    "last wk.",             "1 wk. ago",          /* -1   */
    "0.7 wk. ago",          "0.7 wk. ago",        /* -0.7 */
    "this wk.",             "in 0 wk.",           /*  0   */
    "in 0.7 wk.",           "in 0.7 wk.",         /*  0.7 */
    "next wk.",             "in 1 wk.",           /*  1   */
    "in 2 wk.",             "in 2 wk.",           /*  2   */
    "in 5 wk.",             "in 5 wk."            /*  5   */
};

static const char* en_decDef_long_midSent_min[kNumOffsets*2] = {
/*  text                    numeric */
    "5 minutes ago",        "5 minutes ago",      /* -5   */
    "2.2 minutes ago",      "2.2 minutes ago",    /* -2.2 */
    "2 minutes ago",        "2 minutes ago",      /* -2   */
    "1 minute ago",         "1 minute ago",       /* -1   */
    "0.7 minutes ago",      "0.7 minutes ago",    /* -0.7 */
    "in 0 minutes",         "in 0 minutes",       /*  0   */
    "in 0.7 minutes",       "in 0.7 minutes",     /*  0.7 */
    "in 1 minute",          "in 1 minute",        /*  1   */
    "in 2 minutes",         "in 2 minutes",       /*  2   */
    "in 5 minutes",         "in 5 minutes"        /*  5   */
};

static const char* en_dec0_long_midSent_tues[kNumOffsets*2] = {
/*  text                    numeric */
    ""/*no data */,         ""/*no data */,       /* -5   */
    ""/*no data */,         ""/*no data */,       /* -2.2 */
    ""/*no data */,         ""/*no data */,       /* -2   */
    "last Tuesday",         ""/*no data */,       /* -1   */
    ""/*no data */,         ""/*no data */,       /* -0.7 */
    "this Tuesday",         ""/*no data */,       /*  0   */
    ""/*no data */,         ""/*no data */,       /*  0.7 */
    "next Tuesday",         ""/*no data */,       /*  1   */
    ""/*no data */,         ""/*no data */,       /*  2   */
    ""/*no data */,         ""/*no data */,       /*  5   */
};

static const char* fr_decDef_long_midSent_day[kNumOffsets*2] = {
/*  text                    numeric */
    "il y a 5 jours",       "il y a 5 jours",     /* -5   */
    "il y a 2,2 jours",     "il y a 2,2 jours",   /* -2.2 */
    "avant-hier",           "il y a 2 jours",     /* -2   */
    "hier",                 "il y a 1 jour",      /* -1   */
    "il y a 0,7 jour",      "il y a 0,7 jour",    /* -0.7 */
    "aujourd\\u2019hui",    "dans 0 jour",        /*  0   */
    "dans 0,7 jour",        "dans 0,7 jour",      /*  0.7 */
    "demain",               "dans 1 jour",        /*  1   */
    "apr\\u00E8s-demain",   "dans 2 jours",       /*  2   */
    "dans 5 jours",         "dans 5 jours"        /*  5   */
};


typedef struct {
    const char*                         locale;
    int32_t                             decPlaces; /* fixed decimal places; -1 to use default num formatter */
    UDateRelativeDateTimeFormatterStyle width;
    UDisplayContext                     capContext;
    URelativeDateTimeUnit               unit;
    const char **                       expectedResults; /* for the various offsets */
} RelDateTimeFormatTestItem;

static const RelDateTimeFormatTestItem fmtTestItems[] = {
    { "en", -1, UDAT_STYLE_LONG,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE, UDAT_REL_UNIT_WEEK,    en_decDef_long_midSent_week  },
    { "en",  0, UDAT_STYLE_LONG,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE, UDAT_REL_UNIT_WEEK,    en_dec0_long_midSent_week    },
    { "en", -1, UDAT_STYLE_SHORT, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE, UDAT_REL_UNIT_WEEK,    en_decDef_short_midSent_week },
    { "en", -1, UDAT_STYLE_LONG,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE, UDAT_REL_UNIT_MINUTE,  en_decDef_long_midSent_min   },
    { "en", -1, UDAT_STYLE_LONG,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE, UDAT_REL_UNIT_TUESDAY, en_dec0_long_midSent_tues    },
    { "fr", -1, UDAT_STYLE_LONG,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE, UDAT_REL_UNIT_DAY,     fr_decDef_long_midSent_day   },
    { NULL,  0, (UDateRelativeDateTimeFormatterStyle)0, (UDisplayContext)0, (URelativeDateTimeUnit)0, NULL } /* terminator */
};

enum { kUBufMax = 64, kBBufMax = 256 };

static void TestRelDateFmt()
{
    const RelDateTimeFormatTestItem *itemPtr;
    log_verbose("\nTesting ureldatefmt_open(), ureldatefmt_format(), ureldatefmt_formatNumeric() with various parameters\n");
    for (itemPtr = fmtTestItems; itemPtr->locale != NULL; itemPtr++) {
        URelativeDateTimeFormatter *reldatefmt = NULL;
        UNumberFormat* nfToAdopt = NULL;
        UErrorCode status = U_ZERO_ERROR;
        int32_t iOffset;

        if (itemPtr->decPlaces >= 0) {
            nfToAdopt = unum_open(UNUM_DECIMAL, NULL, 0, itemPtr->locale, NULL, &status);
            if ( U_FAILURE(status) ) {
                log_data_err("FAIL: unum_open(UNUM_DECIMAL, ...) for locale %s: %s\n", itemPtr->locale, myErrorName(status));
                continue;
            }
		    unum_setAttribute(nfToAdopt, UNUM_MIN_FRACTION_DIGITS, itemPtr->decPlaces);
		    unum_setAttribute(nfToAdopt, UNUM_MAX_FRACTION_DIGITS, itemPtr->decPlaces);
		    unum_setAttribute(nfToAdopt, UNUM_ROUNDING_MODE, UNUM_ROUND_DOWN);
        }
        reldatefmt = ureldatefmt_open(itemPtr->locale, nfToAdopt, itemPtr->width, itemPtr->capContext, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("FAIL: ureldatefmt_open() for locale %s, decPlaces %d, width %d, capContext %d: %s\n",
                    itemPtr->locale, itemPtr->decPlaces, (int)itemPtr->width, (int)itemPtr->capContext,
                    myErrorName(status) );
            continue;
        }

        for (iOffset = 0; iOffset < kNumOffsets; iOffset++) {
            UChar ubufget[kUBufMax];
            int32_t ulenget;

            if (itemPtr->unit >= UDAT_REL_UNIT_SUNDAY && offsets[iOffset] != -1.0 && offsets[iOffset] != 0.0 && offsets[iOffset] != 1.0) {
                continue; /* we do not currently have data for this */
            }

            status = U_ZERO_ERROR;
            ulenget = ureldatefmt_format(reldatefmt, offsets[iOffset], itemPtr->unit, ubufget, kUBufMax, &status);
            if ( U_FAILURE(status) ) {
                log_err("FAIL: ureldatefmt_format() for locale %s, decPlaces %d, width %d, capContext %d, offset %.2f, unit %d: %s\n",
                    itemPtr->locale, itemPtr->decPlaces, (int)itemPtr->width, (int)itemPtr->capContext,
                    offsets[iOffset], (int)itemPtr->unit, myErrorName(status) );
            } else {
                UChar ubufexp[kUBufMax];
                int32_t ulenexp = u_unescape(itemPtr->expectedResults[iOffset*2], ubufexp, kUBufMax);
                if (ulenget != ulenexp || u_strncmp(ubufget, ubufexp, ulenexp) != 0) {
                    char  bbufget[kBBufMax];
                    u_austrncpy(bbufget, ubufget, kUBufMax);
                    log_err("ERROR: ureldatefmt_format() for locale %s, decPlaces %d, width %d, capContext %d, offset %.2f, unit %d;\n      expected %s\n      get      %s\n",
                        itemPtr->locale, itemPtr->decPlaces, (int)itemPtr->width, (int)itemPtr->capContext,
                        offsets[iOffset], (int)itemPtr->unit, itemPtr->expectedResults[iOffset*2], bbufget );
                }
            }

            if (itemPtr->unit >= UDAT_REL_UNIT_SUNDAY) {
                continue; /* we do not currently have numeric-style data for this */
            }

            status = U_ZERO_ERROR;
            ulenget = ureldatefmt_formatNumeric(reldatefmt, offsets[iOffset], itemPtr->unit, ubufget, kUBufMax, &status);
            if ( U_FAILURE(status) ) {
                log_err("FAIL: ureldatefmt_formatNumeric() for locale %s, decPlaces %d, width %d, capContext %d, offset %.2f, unit %d: %s\n",
                    itemPtr->locale, itemPtr->decPlaces, (int)itemPtr->width, (int)itemPtr->capContext,
                    offsets[iOffset], (int)itemPtr->unit, myErrorName(status) );
            } else {
                UChar ubufexp[kUBufMax];
                int32_t ulenexp = u_unescape(itemPtr->expectedResults[iOffset*2 + 1], ubufexp, kUBufMax);
                if (ulenget != ulenexp || u_strncmp(ubufget, ubufexp, ulenexp) != 0) {
                    char  bbufget[kBBufMax];
                    u_austrncpy(bbufget, ubufget, kUBufMax);
                    log_err("ERROR: ureldatefmt_formatNumeric() for locale %s, decPlaces %d, width %d, capContext %d, offset %.2f, unit %d;\n      expected %s\n      get      %s\n",
                        itemPtr->locale, itemPtr->decPlaces, (int)itemPtr->width, (int)itemPtr->capContext,
                        offsets[iOffset], (int)itemPtr->unit, itemPtr->expectedResults[iOffset*2 + 1], bbufget );
                }
            }
        }

        ureldatefmt_close(reldatefmt);
    }
}

typedef struct {
    const char*                         locale;
    UDateRelativeDateTimeFormatterStyle width;
    UDisplayContext                     capContext;
    const char *                        relativeDateString;
    const char *                        timeString;
    const char *                        expectedResult;
} CombineDateTimeTestItem;

static const CombineDateTimeTestItem combTestItems[] = {
    { "en",  UDAT_STYLE_LONG,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE, "yesterday",  "3:45 PM",  "yesterday, 3:45 PM" },
    { NULL,  (UDateRelativeDateTimeFormatterStyle)0, (UDisplayContext)0, NULL, NULL, NULL } /* terminator */
};

static void TestCombineDateTime()
{
    const CombineDateTimeTestItem *itemPtr;
    log_verbose("\nTesting ureldatefmt_combineDateAndTime() with various parameters\n");
    for (itemPtr = combTestItems; itemPtr->locale != NULL; itemPtr++) {
        URelativeDateTimeFormatter *reldatefmt = NULL;
        UErrorCode status = U_ZERO_ERROR;
        UChar ubufreldate[kUBufMax];
        UChar ubuftime[kUBufMax];
        UChar ubufget[kUBufMax];
        int32_t ulenreldate, ulentime, ulenget;

        reldatefmt = ureldatefmt_open(itemPtr->locale, NULL, itemPtr->width, itemPtr->capContext, &status);
        if ( U_FAILURE(status) ) {
            log_data_err("FAIL: ureldatefmt_open() for locale %s, width %d, capContext %d: %s\n",
                    itemPtr->locale, (int)itemPtr->width, (int)itemPtr->capContext, myErrorName(status) );
            continue;
        }

        ulenreldate = u_unescape(itemPtr->relativeDateString, ubufreldate, kUBufMax);
        ulentime    = u_unescape(itemPtr->timeString,         ubuftime,    kUBufMax);
        ulenget     = ureldatefmt_combineDateAndTime(reldatefmt, ubufreldate, ulenreldate, ubuftime, ulentime, ubufget, kUBufMax, &status);
        if ( U_FAILURE(status) ) {
            log_err("FAIL: ureldatefmt_combineDateAndTime() for locale %s, width %d, capContext %d: %s\n",
                itemPtr->locale, (int)itemPtr->width, (int)itemPtr->capContext, myErrorName(status) );
        } else {
            UChar ubufexp[kUBufMax];
            int32_t ulenexp = u_unescape(itemPtr->expectedResult, ubufexp, kUBufMax);
            if (ulenget != ulenexp || u_strncmp(ubufget, ubufexp, ulenexp) != 0) {
                char  bbufget[kBBufMax];
                u_austrncpy(bbufget, ubufget, kUBufMax);
                log_err("ERROR: ureldatefmt_combineDateAndTime() for locale %s, width %d, capContext %d;\n      expected %s\n      get      %s\n",
                    itemPtr->locale, (int)itemPtr->width, (int)itemPtr->capContext, itemPtr->expectedResult, bbufget );
            }
        }
        // preflight test
        status = U_ZERO_ERROR;
        ulenget = ureldatefmt_combineDateAndTime(reldatefmt, ubufreldate, ulenreldate, ubuftime, ulentime, NULL, 0, &status);
        if ( status != U_BUFFER_OVERFLOW_ERROR) {
            log_err("FAIL: ureldatefmt_combineDateAndTime() preflight for locale %s, width %d, capContext %d: expected U_BUFFER_OVERFLOW_ERROR, got %s\n",
                itemPtr->locale, (int)itemPtr->width, (int)itemPtr->capContext, myErrorName(status) );
        } else {
            UChar ubufexp[kUBufMax];
            int32_t ulenexp = u_unescape(itemPtr->expectedResult, ubufexp, kUBufMax);
            if (ulenget != ulenexp) {
                log_err("ERROR: ureldatefmt_combineDateAndTime() preflight for locale %s, width %d, capContext %d;\n      expected len %d, get len %d\n",
                    itemPtr->locale, (int)itemPtr->width, (int)itemPtr->capContext, ulenexp, ulenget );
            }
        }

        ureldatefmt_close(reldatefmt);
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_BREAK_ITERATION */
