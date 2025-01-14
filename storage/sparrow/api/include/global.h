#ifndef _spw_api_global_h
#define _spw_api_global_h

#include <cstddef>
#include <cstdint>

// Some error codes
#define SPW_API_OK					0
#define SPW_API_FAILED				-1
#define SPW_API_BUFFER_FULL			-2
#define SPW_API_OUT_OF_MEMORY		-3
#define SPW_API_COL_NOT_NULLABLE	-4
#define SPW_API_COLINDX_OOB			-5		// Column index is out of bounds
#define SPW_API_INCOMPATIBLE_TYPES	-6		// Type of C data is not compatible with type of column
#define SPW_API_SOCKET_CONN_CLOSED	-7
#define SPW_API_SOCKET_READ_ERR		-8
#define SPW_API_SOCKET_WRITE_ERR	-9
#define SPW_API_INVALID_ARG			-10



#ifndef SPW_API_PUBLIC_FUNC

#if defined(_WIN32)
#ifdef SPARROW_API_EXPORTS
#define SPW_API_PUBLIC_FUNC __declspec(dllexport)
#else
// this is for static build
#ifdef SPW_API_LIB_BUILD
#define SPW_API_PUBLIC_FUNC
#else
// this is for clients using dynamic lib
#define SPW_API_PUBLIC_FUNC __declspec(dllimport)
#endif
#endif
#else
#define SPW_API_PUBLIC_FUNC
#endif

#endif    //#ifndef CPPCONN_PUBLIC_FUNC


#endif /* _spw_api_global_h */
