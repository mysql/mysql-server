#ifndef C4_YML_PARSE_OPTIONS_HPP_
#define C4_YML_PARSE_OPTIONS_HPP_

/** @file parse_options.hpp */

#ifndef C4_YML_COMMON_HPP_
#include <c4/yml/common.hpp>
#endif
#ifndef C4_YML_NODE_TYPE_HPP_
#include <c4/yml/node_type.hpp>
#endif
#ifndef C4_YML_ERROR_HPP_
#include <c4/yml/error.hpp>
#endif


namespace c4 {
namespace yml {

/** Options to give to the @ref ParseEngine to control its behavior. */
struct RYML_EXPORT ParserOptions
{
private:

    typedef enum : uint32_t { // NOLINT
        DETECT_FLOW_ML = (1u << 0u),
        RESOLVE_TAGS = (1u << 1u),
        RESOLVE_TAGS_ALL = (1u << 2u),
        SCALAR_FILTERING = (1u << 3u),
        LOCATIONS = (1u << 4u),
        DEFAULTS = SCALAR_FILTERING|DETECT_FLOW_ML,
    } Flags_e;

    uint32_t m_flags = DEFAULTS;
    type_bits m_flow_ml_style = FLOW_ML1;

    ParserOptions& set_flags_(bool enabled, Flags_e f)
    {
        if(enabled)
            m_flags |= f;
        else
            m_flags &= ~f;
        return *this;
    }

public:

    ParserOptions() noexcept = default;

public:

    /** @name detection of @ref FLOW_ML container style
     * @{ */

    /** enable/disable detection of flow multiline container
     * style. When enabled, the parser will set either @ref FLOW_ML1
     * or @ref FLOW_MLN flow multiline style as the style of flow
     * containers which have the terminating bracket on a line
     * different from that of the opening bracket. Default is to
     * detect flow multiline. Use @ref ParserOptions::flow_ml_style()
     * to choose between the @ref FLOW_ML1 or @ref FLOW_MLN styles. */
    ParserOptions& detect_flow_ml(bool enabled) noexcept
    {
        return set_flags_(enabled, DETECT_FLOW_ML);
    }
    /** query status of detection of flow multiline container style. */
    C4_ALWAYS_INLINE bool detect_flow_ml() const noexcept { return (m_flags & DETECT_FLOW_ML); }

    /** choose the default style of multiline flow containers, when a
     * container is detected as flow multiline. Input should be @ref
     * FLOW_ML1 or @ref FLOW_MLN . Default is @ref FLOW_ML1 (the old
     * behavior). */
    ParserOptions& flow_ml_style(NodeType style) noexcept
    {
        RYML_ASSERT_BASIC_(style & (FLOW_ML1|FLOW_MLN));
        m_flow_ml_style = style & (FLOW_ML1|FLOW_MLN);
        return *this;
    }
    C4_ALWAYS_INLINE NodeType flow_ml_style() const noexcept { return m_flow_ml_style; }

    /** @} */

public:

    /** @name resolution of tags */
    /** @{ */

    /** enable/disable resolution of YAML tags during parsing. When
     * enabled, tags are resolved according to existing tag
     * directives. Disabled by default. See also @ref
     * ParserOptions::resolve_tags_all(). */
    ParserOptions& resolve_tags(bool enabled) noexcept
    {
        return set_flags_(enabled, RESOLVE_TAGS);
    }
    /** query status of tag resolution setting. */
    C4_ALWAYS_INLINE bool resolve_tags() const noexcept { return (m_flags & RESOLVE_TAGS); }

    /** When resolve_tags() is enabled, resolve not just prefixed tags
     * of the form <pre>!handle!tag</pre>, but also non-prefixed tags
     * (<pre>!!tag</pre> and <pre>!tag!</pre>). Disabled by default. */
    ParserOptions& resolve_tags_all(bool enabled) noexcept
    {
        return set_flags_(enabled, RESOLVE_TAGS_ALL);
    }
    /** query status of non-prefixed tag resolution setting. */
    C4_ALWAYS_INLINE bool resolve_tags_all() const noexcept { return (m_flags & RESOLVE_TAGS_ALL); }

    /** @} */

public:

    /** @name source location tracking */
    /** @{ */

    /** enable/disable source location tracking. Disabled by default. */
    ParserOptions& locations(bool enabled) noexcept
    {
        return set_flags_(enabled, LOCATIONS);
    }
    /** query source location tracking status */
    C4_ALWAYS_INLINE bool locations() const noexcept { return (m_flags & LOCATIONS); }

    /** @} */

public:

    /** @name scalar filtering status (experimental; disable at your discretion) */
    /** @{ */

    /** enable/disable scalar filtering while parsing. Enabled by default. */
    ParserOptions& scalar_filtering(bool enabled) noexcept
    {
        return set_flags_(enabled, SCALAR_FILTERING);
    }
    /** query scalar filtering status */
    C4_ALWAYS_INLINE bool scalar_filtering() const noexcept { return (m_flags & SCALAR_FILTERING); }

    /** @} */
};

} // namespace yml
} // namespace c4

#endif /* C4_YML_PARSE_OPTIONS_HPP_ */
