
/**
 * GCC linking problem...
 */
#ifdef DEFINE_CXA_PURE_VIRTUAL
extern "C" { int __cxa_pure_virtual() { return 0;} }
#else
/* Some compiler/linker combinations fail on files without exported symbols. */
extern "C" { int dummy_export_symbol() { return 0;} }
#endif
