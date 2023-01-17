# Copyright (c) 2022, 2023, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

OPTION(MSVC_CPPCHECK "Enable the extra MSVC CppCheck checks" OFF)

# check https://learn.microsoft.com/en-us/cpp/build/reference/analyze-code-analysis

MACRO(MSVC_CPPCHECK_ADD_SUPPRESSIONS)
  IF (MSVC AND MSVC_CPPCHECK)
    IF((NOT FORCE_UNSUPPORTED_COMPILER) AND MSVC_VERSION LESS 1929)
      MESSAGE(FATAL_ERROR
        "Visual Studio 2019 Update 11 or newer is required!")
    ELSEIF((NOT FORCE_UNSUPPORTED_COMPILER) AND MSVC_VERSION LESS 1934)
      # Later versions of MSVC haven't raised the following warnings:
      STRING_APPEND(suppress_warnings " /wd26052") # Potentially unconstrained access using expression '...'
      STRING_APPEND(suppress_warnings " /wd26812") # The enum type 'xxxx' is unscoped. Prefer 'enum class' over 'enum' (Enum.3).
      STRING_APPEND(suppress_warnings " /wd28020") # The expression '...' is not true at this call.
      STRING_APPEND(suppress_warnings " /wd6246") # Local declaration of '...' hides declaration of the same name in outer scope.
      STRING_APPEND(suppress_warnings " /wd4251") # '...': class '...' needs to have dll-interface to be used by clients of class '...'

    ENDIF()

    STRING_APPEND(suppress_warnings " /wd26110") # Caller failing to hold lock '...' before calling function 'ReleaseSRWLockExclusive'.
    STRING_APPEND(suppress_warnings " /wd26115") # Failing to release lock '..' in function '..'
    STRING_APPEND(suppress_warnings " /wd26135") # Missing annotation _Acquires_lock_(...) at function '...'.
    STRING_APPEND(suppress_warnings " /wd26160") # Caller possibly failing to hold lock '...' before calling function 'ReleaseSRWLockExclusive'.
    STRING_APPEND(suppress_warnings " /wd26165") # Possibly failing to release lock '...' in function '..'
    STRING_APPEND(suppress_warnings " /wd26400") # Do not assign the result of an allocation or a function call with an owner<T> return value to a raw pointer, use owner<T> instead (i.11).
    STRING_APPEND(suppress_warnings " /wd26401") # Do not delete a raw pointer that is not an owner<T> (i.11).
    STRING_APPEND(suppress_warnings " /wd26402") # Return a scoped object instead of a heap-allocated if it has a move constructor (r.3).
    STRING_APPEND(suppress_warnings " /wd26403") # Reset or explicitly delete an owner<T> pointer '...' (r.3).
    STRING_APPEND(suppress_warnings " /wd26405") # Do not assign to an owner<t> which may be in a valid state (r.3).
    STRING_APPEND(suppress_warnings " /wd26408") # Avoid malloc() and free(), prefer the nothrow version of new with delete (r.10).
    STRING_APPEND(suppress_warnings " /wd26409") # Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).
    STRING_APPEND(suppress_warnings " /wd26410") # The parameter '...' is a reference to const unique pointer, use const T* or const T& instead(r.32)
    STRING_APPEND(suppress_warnings " /wd26411") # The parameter '...' is a reference to unique pointer and it is never reassigned or reset, use T* or T& instead (r.33).
    STRING_APPEND(suppress_warnings " /wd26414") # Move, copy, reassign or reset a local smart pointer '...' (r.5).
    STRING_APPEND(suppress_warnings " /wd26415") # Smart pointer parameter '...' is used only to access contained pointer. Use T* or T& instead (r.30)
    STRING_APPEND(suppress_warnings " /wd26418") # Shared pointer parameter '...' is not copied or moved. Use T* or T& instead (r.36)
    STRING_APPEND(suppress_warnings " /wd26426") # Global initializer calls a non-constexpr function 'range::range' (i.22).
    STRING_APPEND(suppress_warnings " /wd26427") # Global initializer accesses extern object '...' (i.22).
    STRING_APPEND(suppress_warnings " /wd26429") # Symbol '...' is never tested for nullness, it can be marked as not_null (f.23).
    STRING_APPEND(suppress_warnings " /wd26430") # Symbol '...' is not tested for nullness on all paths (f.23).
    STRING_APPEND(suppress_warnings " /wd26432") # If you define or delete any default operation in the type '...', define or delete them all (c.21).
    STRING_APPEND(suppress_warnings " /wd26434") # Function '..' hides a non-virtual function '..'.
    STRING_APPEND(suppress_warnings " /wd26436") # The type '...' with a virtual function needs either public virtual or protected non-virtual destructor (c.35).
    STRING_APPEND(suppress_warnings " /wd26438") # Avoid 'goto' (es.76)
    STRING_APPEND(suppress_warnings " /wd26439") # This kind of function should not throw. Declare it 'noexcept' (f.6).
    STRING_APPEND(suppress_warnings " /wd26440") # Function '...' can be declared 'noexcept' (f.6)
    STRING_APPEND(suppress_warnings " /wd26446") # Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
    STRING_APPEND(suppress_warnings " /wd26447") # The function is declared 'noexcept' but calls function '...()' which may throw exceptions (f.6)
    STRING_APPEND(suppress_warnings " /wd26448") # Consider using gsl::finally if final action is intended (gsl.util).
    STRING_APPEND(suppress_warnings " /wd26450") # Arithmetic overflow: '+' operation causes overflow at compile time. Use a wider type to store the operands (io.1)
    STRING_APPEND(suppress_warnings " /wd26451") # Arithmetic overflow: Using operator '+' on a 4 byte value and then casting the result to a 8 byte value
    STRING_APPEND(suppress_warnings " /wd26455") # Default constructor should not throw. Declare it 'noexcept' (f.6).
    STRING_APPEND(suppress_warnings " /wd26456") # Operator '...' hides a non-virtual operator '...' (c.128).
    STRING_APPEND(suppress_warnings " /wd26457") # (void) should not be used to ignore return values, use 'std::ignore =' instead (es.48).
    STRING_APPEND(suppress_warnings " /wd26459") # You called an STL function '...' with a raw pointer parameter at position '..' that may be unsafe - this relies on the caller to check that the passed values are correct. Consider wrapping your range in a gsl::span and pass as a span iterator (stl.1).
    STRING_APPEND(suppress_warnings " /wd26460") # The reference argument '...' for function '...' can be marked as const (con.3).
    STRING_APPEND(suppress_warnings " /wd26461") # The pointer argument '...' for function '...' can be marked as a pointer to const (con.3).
    STRING_APPEND(suppress_warnings " /wd26462") # The value pointed to by 'end' is assigned only once, mark it as a pointer to const (con.4).
    STRING_APPEND(suppress_warnings " /wd26465") # Don't use const_cast to cast away const or volatile. const_cast is not required; constness or volatility is not being removed by this conversion (type.3).
    STRING_APPEND(suppress_warnings " /wd26466") # Don't use static_cast downcasts. A cast from a polymorphic type should use dynamic_cast (type.2).
    STRING_APPEND(suppress_warnings " /wd26467") # Converting from floating point to unsigned integral types
    STRING_APPEND(suppress_warnings " /wd26471") # Don't use reinterpret_cast. A cast from void* can use static_cast (type.1).
    STRING_APPEND(suppress_warnings " /wd26472") # Don't use a static_cast for arithmetic conversions. Use brace initialization
    STRING_APPEND(suppress_warnings " /wd26473") # Don't cast between pointer types where the source type and the target type are the same (type.1).
    STRING_APPEND(suppress_warnings " /wd26474") # Don't cast between pointer types when the conversion could be implicit (type.1).
    STRING_APPEND(suppress_warnings " /wd26475") # Do not use function style casts (es.49). Prefer 'Type{value}' over 'Type(value)'
    STRING_APPEND(suppress_warnings " /wd26476") # Expression/symbol '..' uses a naked union '...' with multiple type pointers: Use variant instead (type.7)
    STRING_APPEND(suppress_warnings " /wd26478") # Don't use std::move on constant variables. (es.56).
    STRING_APPEND(suppress_warnings " /wd26481") # Don't use pointer arithmetic. Use span instead (bounds.1).
    STRING_APPEND(suppress_warnings " /wd26482") # Only index into arrays using constant expressions (bounds.2).
    STRING_APPEND(suppress_warnings " /wd26483") # Value .. is outside the bounds (..,..) of variable '...'. Only index into arrays using constant expressions that are within bounds of the array (bounds.2)
    STRING_APPEND(suppress_warnings " /wd26485") # Expression '...': No array to pointer decay (bounds.3).
    STRING_APPEND(suppress_warnings " /wd26490") # Don't use reinterpret_cast (type.1).
    STRING_APPEND(suppress_warnings " /wd26491") # Don't use static_cast downcasts (type.2)
    STRING_APPEND(suppress_warnings " /wd26492") # Don't use const_cast to cast away const or volatile (type.3).
    STRING_APPEND(suppress_warnings " /wd26493") # Don't use C-style casts (type.4)
    STRING_APPEND(suppress_warnings " /wd26494") # Variable '...' is uninitialized. Always initialize an object (type.5).
    STRING_APPEND(suppress_warnings " /wd26495") # Variable '...' is uninitialized. Always initialize a member variable (type.6).
    STRING_APPEND(suppress_warnings " /wd26496") # The variable '...' does not change after construction, mark it as const (con.4).
    STRING_APPEND(suppress_warnings " /wd26497") # You can attempt to make '...' constexpr unless it contains any undefined behavior (f.4)
    STRING_APPEND(suppress_warnings " /wd26498") # The function '...' is constexpr, mark variable '...' constexpr if compile-time evaluation is desired (con.5).
    STRING_APPEND(suppress_warnings " /wd26800") # Use of a moved from object (lifetime.1)
    STRING_APPEND(suppress_warnings " /wd26813") # Use 'bitwise and' to check if a flag is set
    STRING_APPEND(suppress_warnings " /wd26814") # The const variable '...' can be computed at compile-time. Consider using constexpr (con.5).
    STRING_APPEND(suppress_warnings " /wd26817") # Potentially expensive copy of variable '..' in range-for loop. Consider making it a const reference (es.71).
    STRING_APPEND(suppress_warnings " /wd26818") # Switch statement does not cover all cases. Consider adding a 'default' label (es.79).
    STRING_APPEND(suppress_warnings " /wd26819") # Unannotated fallthrough between switch labels (es.78).
    STRING_APPEND(suppress_warnings " /wd26826") # Don't use C-style variable arguments (f.55).
    STRING_APPEND(suppress_warnings " /wd26830") # Potentially empty optional '...' is unwrapped
    STRING_APPEND(suppress_warnings " /wd28182") # Dereferencing NULL pointer. '..' contains the same NULL value
    STRING_APPEND(suppress_warnings " /wd28183") # '..' could be '0', and is a copy of the value found in '...'.
    STRING_APPEND(suppress_warnings " /wd28193") # '...' holds a value that must be examined.
    STRING_APPEND(suppress_warnings " /wd28199") # Using possiby uninitialized memory '...'
    STRING_APPEND(suppress_warnings " /wd33010") # Unchecked lower bound for enum ... used as index.
    STRING_APPEND(suppress_warnings " /wd33011") # Unchecked upper bound for enum ... used as index..

    STRING_APPEND(suppress_warnings " /wd6001") # Using unitialized memory '...'
    STRING_APPEND(suppress_warnings " /wd6011") # Dereferencing NULL pointer '...'.
    STRING_APPEND(suppress_warnings " /wd6031") # Return value ignored: ReadFile
    STRING_APPEND(suppress_warnings " /wd6054") # String '..' might not be zero-terminated.
    STRING_APPEND(suppress_warnings " /wd6200") # Index '..' is out of valid index range '..' to '..' for non-stack buffer '...'.
    STRING_APPEND(suppress_warnings " /wd6237") # (zero && expression) is always zero. expression is never evaluated and might have side effects
    STRING_APPEND(suppress_warnings " /wd6239") # (non-zero constant && expression) always evaluates to the result of expression.
    STRING_APPEND(suppress_warnings " /wd6240") # (<expression> && <non-zero constant>) always evaluates to the result of <expression>
    STRING_APPEND(suppress_warnings " /wd6244") # Local declaration of '...' hides previous declaration at line '...' of '...'
    STRING_APPEND(suppress_warnings " /wd6255") # _alloca indicates failure by raising a stack overflow exception. Consider using _malloca instead
    STRING_APPEND(suppress_warnings " /wd6258") # Using TerminateThread does not allow proper thread clean up.
    STRING_APPEND(suppress_warnings " /wd6262") # Function uses ... bytes of stack.
    STRING_APPEND(suppress_warnings " /wd6263") # Using ... in a loop.
    STRING_APPEND(suppress_warnings " /wd6282") # Incorrect operator
    STRING_APPEND(suppress_warnings " /wd6287") # Redundant code
    STRING_APPEND(suppress_warnings " /wd6297") # Arithemtic overflow. Results might not be an expected value
    STRING_APPEND(suppress_warnings " /wd6305") # Potential mismatch between sizeof and countof quantities. Use sizeof() to scale byte sizes
    STRING_APPEND(suppress_warnings " /wd6308") # '...' might return null pointer: assigning null pointer to '...', which is passed as an argument to '...'
    STRING_APPEND(suppress_warnings " /wd6313") # Incorrect operator. Use an equality test to check for zero-valued flags.
    STRING_APPEND(suppress_warnings " /wd6320") # Exception-filter expression is the constant ... This might mask exceptions that were not intended to be handled.
    STRING_APPEND(suppress_warnings " /wd6323") # Use of arithmetic operator on Boolean type(s)
    STRING_APPEND(suppress_warnings " /wd6326") # Potential comparison of a constant with another constant
    STRING_APPEND(suppress_warnings " /wd6336") # Arithmetic operator has precedence over question operator, use parentheses to clarify intent
    STRING_APPEND(suppress_warnings " /wd6340") # Mismatch of sign: '...' passed as '...' when some ... type is required in call to '...'
    STRING_APPEND(suppress_warnings " /wd6384") # Dividing sizeof a pointer by another value
    STRING_APPEND(suppress_warnings " /wd6385") # Reading invalid data from '...'
    STRING_APPEND(suppress_warnings " /wd6386") # Buffer overrun while writing to '...'
    STRING_APPEND(suppress_warnings " /wd6387") # '...' could be '0'

    STRING_APPEND(CMAKE_C_FLAGS "${suppress_warnings}")
    STRING_APPEND(CMAKE_CXX_FLAGS "${suppress_warnings}")
  ENDIF()
ENDMACRO()

MACRO(MSVC_CPPCHECK_ADD_ANALYZE)
  IF (MSVC AND MSVC_CPPCHECK)
    STRING_APPEND(CMAKE_C_FLAGS
      " /analyze /analyze:external- /analyze:pluginEspXEngine.dll")
    STRING_APPEND(CMAKE_CXX_FLAGS
      " /analyze /analyze:external- /analyze:pluginEspXEngine.dll")
    # cmake pre 3.24 doesn't support /external:I for older compilers,
    # so use angle brackets as a substitute.
    IF((CMAKE_VERSION VERSION_LESS 3.24) OR
      (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.29.30036.3))
      STRING_APPEND(CMAKE_C_FLAGS " /external:anglebrackets")
      STRING_APPEND(CMAKE_CXX_FLAGS " /external:anglebrackets")
    ENDIF()
  ENDIF()
ENDMACRO()

MACRO(MSVC_CPPCHECK)
  MSVC_CPPCHECK_ADD_ANALYZE()
  MSVC_CPPCHECK_ADD_SUPPRESSIONS()
ENDMACRO()

#TODO Remove all calls to this macro in all non-3d party bundled code
MACRO(MSVC_CPPCHECK_DISABLE)
  IF (MSVC AND MSVC_CPPCHECK)
    STRING(REPLACE "/analyze:pluginEspXEngine.dll" ""
      CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    STRING(REPLACE "/analyze:pluginEspXEngine.dll" ""
      CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    STRING(REPLACE "/analyze:external-" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    STRING(REPLACE "/analyze:external-" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    STRING(REPLACE "/analyze" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    STRING(REPLACE "/analyze" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  ENDIF()
ENDMACRO()
