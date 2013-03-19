extern "C" bool complain_and_return_true_if_huge_pages_are_enabled(void);
// Effect: Return true if huge pages appear to be enabled.  If so, print some diagnostics to stderr.
//  If environment variable TOKU_HUGE_PAGES_OK is set, then don't complain.
