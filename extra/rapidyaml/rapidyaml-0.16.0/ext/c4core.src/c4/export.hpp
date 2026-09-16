#ifndef C4_EXPORT_HPP_
#define C4_EXPORT_HPP_

#ifdef _WIN32
    #ifdef C4CORE_SHARED
        #ifdef C4CORE_EXPORTS
            #define C4CORE_EXPORT __declspec(dllexport)
            #define C4CORE_EXPORT_EXTERN
        #else
            #define C4CORE_EXPORT __declspec(dllimport)
            #define C4CORE_EXPORT_EXTERN extern
        #endif
    #else
        #define C4CORE_EXPORT
        #define C4CORE_EXPORT_EXTERN
    #endif
#else
    #define C4CORE_EXPORT
    #define C4CORE_EXPORT_EXTERN
#endif

#endif /* C4CORE_EXPORT_HPP_ */
