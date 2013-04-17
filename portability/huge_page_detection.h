/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#pragma once

extern "C" bool complain_and_return_true_if_huge_pages_are_enabled(void);
// Effect: Return true if huge pages appear to be enabled.  If so, print some diagnostics to stderr.
//  If environment variable TOKU_HUGE_PAGES_OK is set, then don't complain.
