/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: cxx_common.h,v 11.2 2002/01/11 15:52:23 bostic Exp $
 */

#ifndef _CXX_COMMON_H_
#define	_CXX_COMMON_H_

//
// Common definitions used by all of Berkeley DB's C++ include files.
//

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//
// Mechanisms for declaring classes
//

//
// Every class defined in this file has an _exported next to the class name.
// This is needed for WinTel machines so that the class methods can
// be exported or imported in a DLL as appropriate.  Users of the DLL
// use the define DB_USE_DLL.  When the DLL is built, DB_CREATE_DLL
// must be defined.
//
#if defined(_MSC_VER)

#  if defined(DB_CREATE_DLL)
#    define _exported __declspec(dllexport)      // creator of dll
#  elif defined(DB_USE_DLL)
#    define _exported __declspec(dllimport)      // user of dll
#  else
#    define _exported                            // static lib creator or user
#  endif

#else /* _MSC_VER */

#  define _exported

#endif /* _MSC_VER */
#endif /* !_CXX_COMMON_H_ */
