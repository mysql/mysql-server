#ifndef C4_YML_EXPORT_HPP_
#define C4_YML_EXPORT_HPP_

#ifdef _WIN32
    #ifdef RYML_SHARED
        #ifdef RYML_EXPORTS
            #define RYML_EXPORT __declspec(dllexport)
            #define RYML_EXPORT_EXTERN
        #else
            #define RYML_EXPORT __declspec(dllimport)
            #define RYML_EXPORT_EXTERN extern
        #endif
    #else
        #define RYML_EXPORT
        #define RYML_EXPORT_EXTERN
    #endif
#else
    #define RYML_EXPORT
    #define RYML_EXPORT_EXTERN
#endif

#endif /* C4_YML_EXPORT_HPP_ */
