
/**
 * GCC linking problem...
 */
#if ( __GNUC__ == 3 )
extern "C" { int __cxa_pure_virtual() { return 0;} }
#endif
