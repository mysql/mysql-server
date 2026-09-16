#ifndef C4_YML_TAG_HPP_
#define C4_YML_TAG_HPP_

#ifndef C4_YML_COMMON_HPP_
#include <c4/yml/common.hpp>
#endif
#ifndef C4_YML_DETAIL_STACK_HPP_
#include <c4/yml/detail/stack.hpp>
#endif

namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4251) // csubstr needs to have dll-interface to be used by clients of LookupResult

class Tree;

/** @addtogroup doc_tag_utils
 *
 * @{
 */


#ifndef RYML_MAX_TAG_DIRECTIVES
/** the maximum number of tag directives in a Tree */
#define RYML_MAX_TAG_DIRECTIVES 4
#endif

/** the integral type necessary to cover all the bits marking node tags */
using tag_bits = uint16_t;

/** a bit mask for marking tags for types */
typedef enum : tag_bits { // NOLINT
    TAG_NONE      =  0,
    // container types
    TAG_MAP       =  1, /**< !!map   Unordered set of key: value pairs without duplicates. @see https://yaml.org/type/map.html */
    TAG_OMAP      =  2, /**< !!omap  Ordered sequence of key: value pairs without duplicates. @see https://yaml.org/type/omap.html */
    TAG_PAIRS     =  3, /**< !!pairs Ordered sequence of key: value pairs allowing duplicates. @see https://yaml.org/type/pairs.html */
    TAG_SET       =  4, /**< !!set   Unordered set of non-equal values. @see https://yaml.org/type/set.html */
    TAG_SEQ       =  5, /**< !!seq   Sequence of arbitrary values. @see https://yaml.org/type/seq.html */
    // scalar types
    TAG_BINARY    =  6, /**< !!binary A sequence of zero or more octets (8 bit values). @see https://yaml.org/type/binary.html */
    TAG_BOOL      =  7, /**< !!bool   Mathematical Booleans. @see https://yaml.org/type/bool.html */
    TAG_FLOAT     =  8, /**< !!float  Floating-point approximation to real numbers. https://yaml.org/type/float.html */
    TAG_INT       =  9, /**< !!float  Mathematical integers. https://yaml.org/type/int.html */
    TAG_MERGE     = 10, /**< !!merge  Specify one or more mapping to be merged with the current one. https://yaml.org/type/merge.html */
    TAG_NULL      = 11, /**< !!null   Devoid of value. https://yaml.org/type/null.html */
    TAG_STR       = 12, /**< !!str    A sequence of zero or more Unicode characters. https://yaml.org/type/str.html */
    TAG_TIMESTAMP = 13, /**< !!timestamp A point in time https://yaml.org/type/timestamp.html */
    TAG_VALUE     = 14, /**< !!value  Specify the default value of a mapping https://yaml.org/type/value.html */
    TAG_YAML      = 15, /**< !!yaml   Specify the default value of a mapping https://yaml.org/type/yaml.html */
} YamlTag_e;

RYML_EXPORT YamlTag_e to_tag(csubstr tag);
RYML_EXPORT csubstr from_tag(YamlTag_e tag);
RYML_EXPORT csubstr from_tag_long(YamlTag_e tag);
RYML_EXPORT csubstr normalize_tag(csubstr tag);
RYML_EXPORT csubstr normalize_tag_long(csubstr tag);
RYML_EXPORT csubstr normalize_tag_long(csubstr tag, substr output);

/** is a tag of the form `!handle!tag`? */
RYML_EXPORT bool is_custom_tag(csubstr tag);
RYML_EXPORT bool is_valid_tag_handle(csubstr handle);


//-----------------------------------------------------------------------------

/** Accelerator structure to reduce memory requirements by enabling
 * reuse of resolved tags. */
struct TagCache
{
    struct Entry
    {
        csubstr tag;
        csubstr resolved;
        id_type doc_id;
    };
    using Entries = detail::stack<Entry>;
    using const_iterator = id_type;
    struct RYML_EXPORT LookupResult
    {
        csubstr resolved;
        const_iterator pos;
        operator bool() const noexcept { return resolved.len > 0; }
    };

public:

    TagCache() noexcept : m_entries() {}
    RYML_EXPORT LookupResult find(csubstr tag, id_type doc_id, id_type linear_threshold=Entries::sso_size) const noexcept;
    RYML_EXPORT void add(csubstr tag, csubstr resolved, id_type doc_id, const_iterator pos) RYML_NOEXCEPT;

    void clear() noexcept { m_entries.clear(); }

public:

    /** @cond dev */
    Entries m_entries;
    /** @endcond */

};


//-----------------------------------------------------------------------------

struct RYML_EXPORT TagDirective
{
    /** Eg <pre>!e!</pre> in <pre>%TAG !e! tag:example.com,2000:app/</pre> */
    csubstr handle;
    /** Eg <pre>tag:example.com,2000:app/</pre> in <pre>%TAG !e! tag:example.com,2000:app/</pre> */
    csubstr prefix;
    /** ID of the target document */
    id_type doc_id;
};

struct RYML_EXPORT TagDirectiveRange
{
    TagDirective const* C4_RESTRICT b;
    TagDirective const* C4_RESTRICT e;
    C4_ALWAYS_INLINE TagDirective const* begin() const noexcept { return b; }
    C4_ALWAYS_INLINE TagDirective const* end() const noexcept { return e; }
    id_type size() const noexcept { return static_cast<id_type>(e - b); }
};

struct RYML_EXPORT TagDirectives
{
    TagDirective m_directives[RYML_MAX_TAG_DIRECTIVES];
    bool redefines_qmrk() const noexcept;
    TagDirective const* add(csubstr handle, csubstr prefix, id_type doc_id) noexcept;
    void clear() noexcept;
    id_type size() const noexcept;
    TagDirective const* lookup(csubstr tag, id_type id) const noexcept;
    TagDirective * begin() noexcept { return m_directives; }
    TagDirective * end() noexcept { return m_directives + size(); }
    TagDirective const* begin() const noexcept { return m_directives; }
    TagDirective const* end() const noexcept { return m_directives + size(); }
    TagDirectiveRange directives() const noexcept { return TagDirectiveRange{m_directives, m_directives + size()}; }
    TagDirectiveRange lookup_range(id_type doc_id) const noexcept;
    /** @note the str member of the return value may be null, meaning
     * that the buffer was not enough to fit the transformed tag.
     *
     * @note the return value may actually be not a substring of the
     * input buffer. */
    csubstr resolve(substr buf, size_t *bufsz, csubstr tag, id_type doc_id, Location const& ymlloc, Callbacks const& callbacks, bool with_brackets=true) const;
};

/** returns the length of the transformed tag, or 0 to signal that the
 * tag is local and cannot be resolved */
RYML_EXPORT size_t transform_tag(substr output, csubstr handle, csubstr prefix, csubstr tag,
                                 Callbacks const& callbacks, Location const& ymlloc={},
                                 bool with_brackets=true);

/** @} */

C4_SUPPRESS_WARNING_MSVC_POP

} // namespace yml
} // namespace c4

#endif /* C4_YML_TAG_HPP_ */
