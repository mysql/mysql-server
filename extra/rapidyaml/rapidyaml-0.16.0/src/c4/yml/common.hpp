#ifndef C4_YML_COMMON_HPP_
#define C4_YML_COMMON_HPP_

/** @file common.hpp Common utilities and infrastructure used by ryml. */

#include <cstddef>
#include <utility> // for std::forward

#ifndef C4_SUBSTR_HPP_
#include <c4/substr.hpp>
#endif
#ifndef C4_YML_EXPORT_HPP_
#include <c4/yml/export.hpp>
#endif


//-----------------------------------------------------------------------------

#ifndef RYML_DEFAULT_TREE_CAPACITY
/// default capacity for the tree when not set explicitly
#define RYML_DEFAULT_TREE_CAPACITY (16)
#endif

#ifndef RYML_DEFAULT_TREE_ARENA_CAPACITY
/// default capacity for the tree's arena when not set explicitly
#define RYML_DEFAULT_TREE_ARENA_CAPACITY (0)
#endif

#ifndef RYML_DEFAULT_TREE_ARENA_CAPACITY_START
/// default starting capacity for the tree's arena when it is first
/// allocated. should be larger than @ref RYML_DEFAULT_TREE_ARENA_CAPACITY
#define RYML_DEFAULT_TREE_ARENA_CAPACITY_START (256)
#endif


#ifndef RYML_LOCATIONS_SMALL_THRESHOLD
/// threshold at which a location search will revert from linear to
/// binary search.
#define RYML_LOCATIONS_SMALL_THRESHOLD (30)
#endif


#ifndef RYML_ERRMSG_SIZE
/// size for the error message buffer
#define RYML_ERRMSG_SIZE (1024)
#endif


#ifndef RYML_LOGBUF_SIZE
/// size for the buffer used to format individual values to string
/// while preparing an error message. This is only used for formatting
/// individual values in the message; final messages will be larger
/// than this value (see @ref RYML_ERRMSG_SIZE). This is also used for
/// the detailed debug log messages when RYML_DBG is defined.
#define RYML_LOGBUF_SIZE (256)
#endif
static_assert(RYML_LOGBUF_SIZE < RYML_ERRMSG_SIZE, "invalid size");


#ifndef RYML_LOGBUF_SIZE_MAX
/// size for the fallback larger log buffer. When @ref
/// RYML_LOGBUF_SIZE is not large enough to convert a value to string,
/// then temporary stack memory is allocated up to
/// RYML_LOGBUF_SIZE_MAX. This limit is in place to prevent a stack
/// overflow. If the printed value requires more than
/// RYML_LOGBUF_SIZE_MAX, the value is silently skipped.
#define RYML_LOGBUF_SIZE_MAX (1024)
#endif


//-----------------------------------------------------------------------------

/** @cond dev */

#ifndef RYML_USE_ASSERT
#   define RYML_USE_ASSERT C4_USE_ASSERT
#endif

#if RYML_USE_ASSERT
#   define RYML_NOEXCEPT
#else
#   define RYML_NOEXCEPT noexcept
#endif

#define RYML_DEPRECATED(msg) C4_DEPRECATED(msg)

#ifdef RYML_WITH_LEGACY_OPERATORS
#   define RYML_LEGACY_OPERATOR(txt)
#else
#   define RYML_LEGACY_OPERATOR(txt) RYML_DEPRECATED(txt ". To avoid this warning, define the symbol RYML_WITH_LEGACY_OPERATORS while compiling")
#endif

#define RYML_CHECK_TYPE_IS_WRAPPER_LIKE_(type)                          \
    static_assert(!std::is_fundamental<type>::value,                    \
                  "did you forget to use '&'? "                         \
                  "This overload is for wrapper types such as c4::fmt::base64()");

/** @endcond */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")

class Tree;


#ifndef RYML_ID_TYPE
/** The type of a node id in the YAML tree. In the future, the default
 * will likely change to int32_t, which was observed to be faster.
 * @see id_type */
#define RYML_ID_TYPE size_t
#endif


/** The type of a node id in the YAML tree; to override the default
 * type, define the macro @ref RYML_ID_TYPE to a suitable integer
 * type. */
using id_type = RYML_ID_TYPE;
static_assert(std::is_integral<id_type>::value, "id_type must be an integer type");


C4_SUPPRESS_WARNING_GCC_WITH_PUSH("-Wuseless-cast")
enum : id_type { // NOLINT
    /** an index to none */
    NONE = id_type(-1), // NOLINT
};
C4_SUPPRESS_WARNING_GCC_CLANG_POP


enum : size_t { // NOLINT
    /** a null string position */
    npos = size_t(-1) // NOLINT
};


typedef enum Encoding_ { // NOLINT
    NOBOM,         //!< No Byte Order Mark was found
    UTF8,          //!< UTF8
    UTF16LE,       //!< UTF16, Little-Endian
    UTF16BE,       //!< UTF16, Big-Endian
    UTF32LE,       //!< UTF32, Little-Endian
    UTF32BE,       //!< UTF32, Big-Endian
} Encoding_e;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** A lightweight truthy type, used to enable reporting the offending
 * node when a deserializing error happens in nested reads. It reports
 * the innermost node causing the error, or true (empty-initialized)
 * when there is no error.
 */
struct RYML_EXPORT ReadResult
{
    id_type node;
    enum : id_type { VALID = NONE - 1 }; // NOLINT

public:

    /** convert to boolean to signify success/error */
    operator bool() const noexcept { return node == VALID; }

public:

    /** construct as success */
    C4_ALWAYS_INLINE ReadResult() noexcept : node(VALID) {}

    /** construct as failure on the given node id */
    C4_ALWAYS_INLINE explicit ReadResult(id_type node_) noexcept
        : node(node_) {}

    /** construct as failure on the given parent_, IF node_ is NONE */
    C4_ALWAYS_INLINE explicit ReadResult(id_type parent_, id_type node_) noexcept
        : node(node_ != NONE ? VALID : parent_) {}

public:

    // These adapter ctors are used by rapidyaml in the functions
    // calling read(), and enable working both with legacy and
    // up-to-date user implementations of read(). See for example
    // Tree::deserialize().

    /** adapter: will match legacy user code (`%read()`
     * implementations returning bool). On error, this will report
     * node_ as the offending node.
     *
     * This is an adapter ctor used by rapidyaml in the functions
     * calling `%read()`, and enables rapidyaml to work both with
     * legacy and up-to-date user implementations of `%read()`. See
     * for example @ref Tree::deserialize().
     */
    C4_ALWAYS_INLINE explicit ReadResult(bool ok, id_type node_) noexcept
        : node(ok ? VALID : node_) {}

    /** adapter: will match up-to-date user code (`%read()`
     * implementations returning @ref ReadResult). On error, this will
     * report the node in the original @ref ReadResult.
     *
     * This is an adapter ctor used by rapidyaml in the functions
     * calling `%read()`, and enables rapidyaml to work both with
     * legacy and up-to-date user implementations of `%read()`. See
     * for example @ref Tree::deserialize().
     */
    C4_ALWAYS_INLINE explicit ReadResult(ReadResult result, id_type) noexcept
        : node(result.node) {}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4251) // csubstr needs to have dll-interface to be used by clients of Location

/** holds a source or yaml file position, for example when an error is
 * detected; See also @ref location_format() and @ref
 * location_format_with_context().
 *
 * @ingroup doc_error_handling */
struct RYML_EXPORT Location
{
    size_t offset; ///< number of bytes from the beginning of the source buffer
    size_t line;   ///< line
    size_t col;    ///< column
    csubstr name;  ///< name of the file

    operator bool () const noexcept { return !name.empty() || line != npos || offset != npos || col != npos; }

    C4_NO_INLINE Location() noexcept : offset(npos), line(npos), col(npos), name() {}
    C4_NO_INLINE Location(                         size_t l          ) noexcept : offset(npos), line(l), col(npos), name() {}
    C4_NO_INLINE Location(                         size_t l, size_t c) noexcept : offset(npos), line(l), col(c   ), name() {}
    C4_NO_INLINE Location(               size_t b, size_t l, size_t c) noexcept : offset(b   ), line(l), col(c   ), name() {}
    C4_NO_INLINE Location(    csubstr n,           size_t l          ) noexcept : offset(npos), line(l), col(npos), name(n) {}
    C4_NO_INLINE Location(    csubstr n,           size_t l, size_t c) noexcept : offset(npos), line(l), col(c   ), name(n) {}
    C4_NO_INLINE Location(    csubstr n, size_t b, size_t l, size_t c) noexcept : offset(b   ), line(l), col(c   ), name(n) {}
    C4_NO_INLINE Location(const char *n,           size_t l          ) noexcept : offset(npos), line(l), col(npos), name(to_csubstr(n)) {}
    C4_NO_INLINE Location(const char *n,           size_t l, size_t c) noexcept : offset(npos), line(l), col(c   ), name(to_csubstr(n)) {}
    C4_NO_INLINE Location(const char *n, size_t b, size_t l, size_t c) noexcept : offset(b   ), line(l), col(c   ), name(to_csubstr(n)) {}
};
static_assert(std::is_standard_layout<Location>::value, "Location not trivial");

C4_SUPPRESS_WARNING_MSVC_POP

/// @cond dev
#define RYML_LOC_HERE() (::c4::yml::Location(__FILE__, static_cast<size_t>(__LINE__)))
/// @endcond


/** Data for a basic error.
 * @ingroup doc_error_handling */
struct RYML_EXPORT ErrorDataBasic
{
    Location location; ///< location where the error was detected (may be from YAML or C++ source code)
    ErrorDataBasic() noexcept = default;
    ErrorDataBasic(Location const& cpploc_) noexcept : location(cpploc_) {}
};

/** Data for a parse error.
 * @ingroup doc_error_handling */
struct RYML_EXPORT ErrorDataParse
{
    Location cpploc; ///< location in the C++ source file where the error was detected.
    Location ymlloc; ///< location in the YAML source buffer where the error was detected.
    ErrorDataParse() noexcept = default;
    ErrorDataParse(Location const& cpploc_, Location const& ymlloc_) noexcept : cpploc(cpploc_), ymlloc(ymlloc_) {}
};

/** Data for a visit error.
 * @ingroup doc_error_handling */
struct RYML_EXPORT ErrorDataVisit
{
    Location cpploc;  ///< location in the C++ source file where the error was detected.
    Tree const* tree; ///< tree where the error was detected
    id_type node;     ///< node where the error was detected
    ErrorDataVisit() noexcept = default;
    ErrorDataVisit(Location const& cpploc_, Tree const *tree_ , id_type node_) noexcept : cpploc(cpploc_), tree(tree_), node(node_) {}
};


//-----------------------------------------------------------------------------

/** @addtogroup doc_callbacks
 *
 * @{ */

struct Callbacks;


/** set the global callbacks for the library; after a call to this
 * function, these callbacks will be used by newly created objects
 * (unless they are copying older objects with different
 * callbacks). If @ref RYML_NO_DEFAULT_CALLBACKS is defined, it is
 * mandatory to call this function prior to using any other library
 * facility.
 *
 * @warning This function is NOT thread-safe, because it sets global static data
 * */
RYML_EXPORT void set_callbacks(Callbacks const& c);

/** get the global callbacks
 *
 * @warning This function is NOT thread-safe, because it reads global static data
 * */
RYML_EXPORT Callbacks const& get_callbacks();

/** set the global callbacks back to their defaults.
 *
 * @warning This function is NOT thread-safe, because it sets global static data
 * */
RYML_EXPORT void reset_callbacks();


/** the type of the function used to allocate memory; ryml will only
 * allocate memory through this callback. */
using pfn_allocate = void* (*)(size_t len, void* hint, void *user_data);


/** the type of the function used to free memory; ryml will only free
 * memory through this callback. */
using pfn_free = void (*)(void* mem, size_t size, void *user_data);


/** the type of the function used to report basic errors.
 *
 * @warning Must not return. When implemented by the user, this
 * function MUST interrupt execution (and ideally be marked with
 * `[[noreturn]]`). If the function returned, the caller could enter
 * into an infinite loop, or the program may crash. It is up to the
 * user to choose the interruption mechanism; typically by either
 * throwing an exception, or using `std::longjmp()` ([see
 * documentation](https://en.cppreference.com/w/cpp/utility/program/setjmp))
 * or ultimately by calling `std::abort()`. */
using pfn_error_basic = void (*) (csubstr msg, ErrorDataBasic const& errdata, void *user_data);
/** the type of the function used to report parse errors.
 *
 * @warning Must not return. When implemented by the user, this
 * function MUST interrupt execution (and ideally be marked with
 * `[[noreturn]]`). If the function returned, the caller could enter
 * into an infinite loop, or the program may crash. It is up to the
 * user to choose the interruption mechanism; typically by either
 * throwing an exception, or using `std::longjmp()` ([see
 * documentation](https://en.cppreference.com/w/cpp/utility/program/setjmp))
 * or ultimately by calling `std::abort()`. */
using pfn_error_parse = void (*) (csubstr msg, ErrorDataParse const& errdata, void *user_data);
/** the type of the function used to report visit errors.
 *
 * @warning Must not return. When implemented by the user, this
 * function MUST interrupt execution (and ideally be marked with
 * `[[noreturn]]`). If the function returned, the caller could enter
 * into an infinite loop, or the program may crash. It is up to the
 * user to choose the interruption mechanism; typically by either
 * throwing an exception, or using `std::longjmp()` ([see
 * documentation](https://en.cppreference.com/w/cpp/utility/program/setjmp))
 * or ultimately by calling `std::abort()`. */
using pfn_error_visit = void (*) (csubstr msg, ErrorDataVisit const& errdata, void *user_data);

/// @cond dev
using pfn_error RYML_DEPRECATED("use a more specific error type: `basic`, `parse` or `visit`") = void (*) (const char* msg, size_t msg_len, Location const& cpploc, void *user_data);
/// @endcond


/** A c-style callbacks class to customize behavior on errors or
 * allocation. Can be used globally by the library and/or locally by
 * @ref Tree and @ref Parser objects. */
struct RYML_EXPORT Callbacks
{
    void *          m_user_data;   ///< data to be forwarded in every call to a callback
    pfn_allocate    m_allocate;    ///< a pointer to an allocate handler function
    pfn_free        m_free;        ///< a pointer to a free handler function
    pfn_error_basic m_error_basic; ///< a pointer to a basic error handler function
    pfn_error_parse m_error_parse; ///< a pointer to a parse error handler function
    pfn_error_visit m_error_visit; ///< a pointer to a visit error handler function

public:

    /** Construct an object with the default callbacks. If
     * @ref RYML_NO_DEFAULT_CALLBACKS is defined, the object will be set with null
     * members.*/
    Callbacks() noexcept;

    RYML_DEPRECATED("use the default constructor, followed by the appropriate setters")
    Callbacks(void *user_data, pfn_allocate alloc, pfn_free free, pfn_error_basic error_basic);

public:

    /** Set the user data. */
    Callbacks& set_user_data(void* user_data);

    /** Set or reset the allocate callback. When the parameter is
     * null, m_allocate will fall back to ryml's default allocate
     * implementation, unless @ref RYML_NO_DEFAULT_CALLBACKS is
     * defined. */
    Callbacks& set_allocate(pfn_allocate allocate=nullptr);

    /** Set or reset the free callback. When the parameter is null,
     * m_free will fall back to ryml's default free implementation,
     * unless @ref RYML_NO_DEFAULT_CALLBACKS is defined. */
    Callbacks& set_free(pfn_free free=nullptr);

    /** Set or reset the error_basic callback. When the parameter is null,
     * m_error_basic will fall back to ryml's default error_basic implementation,
     * unless @ref RYML_NO_DEFAULT_CALLBACKS is defined. */
    Callbacks& set_error_basic(pfn_error_basic error_basic=nullptr);

    /** Set or reset the error_parse callback. When the parameter is null,
     * m_error_parse will fall back to ryml's default error_parse implementation,
     * unless @ref RYML_NO_DEFAULT_CALLBACKS is defined. */
    Callbacks& set_error_parse(pfn_error_parse error_parse=nullptr);

    /** Set or reset the error_visit callback. When the parameter is null,
     * m_error_visit will fall back to ryml's default error_visit implementation,
     * unless @ref RYML_NO_DEFAULT_CALLBACKS is defined. */
    Callbacks& set_error_visit(pfn_error_visit error_visit=nullptr);

public:

    bool operator!= (Callbacks const& that) const { return !operator==(that); }
    bool operator== (Callbacks const& that) const
    {
        return (m_user_data == that.m_user_data &&
                m_allocate == that.m_allocate &&
                m_free == that.m_free &&
                m_error_basic == that.m_error_basic &&
                m_error_parse == that.m_error_parse &&
                m_error_visit == that.m_error_visit);
    }
};

/** @} */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** @cond dev */

C4_SUPPRESS_WARNING_PUSH
C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated")
C4_SUPPRESS_WARNING_GCC_CLANG("-Wdeprecated-declarations")
C4_SUPPRESS_WARNING_MSVC(4996) // deprecated

/** A tag type to select the key when building the tree, or when
 * (de)serializing with operator<< or operator>> */
template<class K>
struct RYML_LEGACY_OPERATOR("prefer .set_key() methods in the Tree and node")
Key
{
    K &&k; // NOLINT
};

/** A tag function to select the key when building the tree, or when
 * (de)serializing with operator<< or operator>> */
template<class K>
RYML_LEGACY_OPERATOR("prefer .set_key() methods in the Tree and node")
C4_ALWAYS_INLINE Key<K> key(K && k)
{
    return Key<K>{std::forward<K>(k)};
}

C4_SUPPRESS_WARNING_POP

/** @endcond */

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/// @cond dev

#define RYML_CB_ALLOC_HINT_(cb, T, num, hint) (T*) (cb).m_allocate((num) * sizeof(T), (hint), (cb).m_user_data)
#define RYML_CB_ALLOC_(cb, T, num) RYML_CB_ALLOC_HINT_((cb), T, (num), nullptr)
#define RYML_CB_FREE_(cb, buf, T, num)                              \
    do {                                                            \
        (cb).m_free((buf), (num) * sizeof(T), (cb).m_user_data);    \
        (buf) = nullptr;                                            \
    } while(false)

namespace detail {
template<int8_t signedval, uint8_t unsignedval>
struct _charconstant_t // is there a better way to do this?
    : public std::conditional<std::is_signed<char>::value,
                              std::integral_constant<int8_t, static_cast<int8_t>(unsignedval)>,
                              std::integral_constant<uint8_t, unsignedval>>::type
{};
#define RYML_CHCONST_(signedval, unsignedval) ::c4::yml::detail::_charconstant_t<INT8_C(signedval), UINT8_C(unsignedval)>::value
} // namespace detail

inline csubstr _c4prc(const char &C4_RESTRICT c) // pass by reference!
{
    switch(c)
    {
    case '\n': return csubstr("\\n");
    case '\t': return csubstr("\\t");
    case '\0': return csubstr("\\0");
    case '\r': return csubstr("\\r");
    case '\f': return csubstr("\\f");
    case '\b': return csubstr("\\b");
    case '\v': return csubstr("\\v");
    case '\a': return csubstr("\\a");
    default: return csubstr(&c, 1);
    }
}

#if C4_CPP >= 17                                  \
    || (defined(__GNUC__) && __GNUC__ >= 6)       \
    || (defined(_MSC_VER) && !defined(__clang__))
#define RYML_HAS_DEPRECATED_ENUMS_
#endif

/// @endcond

C4_SUPPRESS_WARNING_GCC_POP

} // namespace yml
} // namespace c4

#endif /* C4_YML_COMMON_HPP_ */
