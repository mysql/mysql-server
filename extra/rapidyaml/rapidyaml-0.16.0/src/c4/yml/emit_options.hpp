#ifndef C4_YML_EMIT_OPTIONS_HPP_
#define C4_YML_EMIT_OPTIONS_HPP_

#ifndef C4_YML_COMMON_HPP_
#include <c4/yml/common.hpp>
#endif

namespace c4 {
namespace yml {

/** A lightweight object containing options to be used when emitting.
 * @ingroup doc_emit
 */
struct RYML_EXPORT EmitOptions
{
public:

    /** @cond dev */
    typedef enum : uint32_t { // NOLINT
        INDENT_FLOW_ML = 1u << 0u,
        FORCE_FLOW_SPC = 1u << 1u,
        EMIT_NONROOT_KEY = 1u << 2u,
        EMIT_NONROOT_DASH = 1u << 3u,
        EMIT_NONROOT_MARKUP = EMIT_NONROOT_KEY|EMIT_NONROOT_DASH,
        JSON_ERR_ON_STREAM = 1u << 4u,
        JSON_ERR_ON_TAG = 1u << 5u,
        JSON_ERR_ON_ANCHOR = 1u << 6u,
        JSON_ERR_MASK_ = JSON_ERR_ON_TAG|JSON_ERR_ON_ANCHOR,
        DEFAULT_FLAGS = EMIT_NONROOT_KEY|INDENT_FLOW_ML,
    } Flags_e;
    /** @endcond */

private:

    EmitOptions& set_flags_(bool enabled, Flags_e f)
    {
        if(enabled)
            m_flags |= f;
        else
            m_flags &= ~f;
        return *this;
    }

public:

    /** @name YAML flow customization
     *
     * Control formatting of YAML flow containers.
     *
     * @{ */

    /** Indent the contents of @ref FLOW_ML1 and @ref FLOW_MLN
     * containers. Enabled by default. */
    C4_ALWAYS_INLINE bool indent_flow_ml() const noexcept { return (m_flags & INDENT_FLOW_ML) != 0; }
    EmitOptions& indent_flow_ml(bool enabled) noexcept { return set_flags_(enabled, INDENT_FLOW_ML); }

    /** Force everywhere a space after comma in flow mode, overriding
     * the @ref FLOW_SPC status of individual containers. This only
     * applies to @ref FLOW_ML1 or @ref FLOW_MLN containers. Disabled
     * by default. */
    EmitOptions& force_flow_spc(bool enabled) noexcept { return set_flags_(enabled, FORCE_FLOW_SPC); }
    C4_ALWAYS_INLINE bool force_flow_spc() const noexcept { return (m_flags & FORCE_FLOW_SPC) != 0; }

    /** Set max columns for the emitted YAML in @ref FLOW_MLN
     * mode. This will make the emitted YAML wrap around when the line
     * reaches max cols, but only in containers with @ref FLOW_MLN
     * mode. Defaults to 80 columns. */
    EmitOptions& max_cols(id_type cols) noexcept { m_max_cols = cols; return *this; }
    C4_ALWAYS_INLINE id_type max_cols() const noexcept { return m_max_cols; }
    static constexpr const id_type max_cols_default = 80;

    /** @} */

public:

    /** @name Nested nodes
     *
     * Control emission of nested (non-root) nodes.
     *
     * @{ */

    /** When emit starts on a nested (non-root) node, emit the node's
     * key as well. This will maek the resulting YAML a map with the
     * node as its single element. Enabled by default. */
    EmitOptions& emit_nonroot_key(bool enabled) noexcept { return set_flags_(enabled, EMIT_NONROOT_KEY); }
    C4_ALWAYS_INLINE bool emit_nonroot_key() const noexcept { return (m_flags & EMIT_NONROOT_KEY) != 0; }

    /** When emit starts on a nested (non-root) node, emit a leading
     * dash. This will make the resulting YAML a seq with the node as
     * its single element. Disabled by default. */
    EmitOptions& emit_nonroot_dash(bool enabled) noexcept { return set_flags_(enabled, EMIT_NONROOT_DASH); }
    C4_ALWAYS_INLINE bool emit_nonroot_dash() const noexcept { return (m_flags & EMIT_NONROOT_DASH) != 0; }

    /** @} */

public:

    /** @name JSON behavior
     *
     * Controls the behavior when emitting JSON.
     * @{ */

    /** Whether to trigger an error (or emit as seq) when finding a
     * stream in json mode. Disabled by default. */
    EmitOptions& json_err_on_stream(bool enabled) noexcept { return set_flags_(enabled, JSON_ERR_ON_STREAM); }
    C4_ALWAYS_INLINE bool json_err_on_stream() const noexcept { return (m_flags & JSON_ERR_ON_STREAM) != 0; }

    /** Whether to trigger an error (or ignore the tag) when finding a
     * tag in json mode. Disabled by default. */
    EmitOptions& json_err_on_tag(bool enabled) noexcept { return set_flags_(enabled, JSON_ERR_ON_TAG); }
    C4_ALWAYS_INLINE bool json_err_on_tag() const noexcept { return (m_flags & JSON_ERR_ON_TAG) != 0; }

    /** Whether to trigger an error (or ignore the anchor) when
     * finding an anchor in json mode. Disabled by default. */
    EmitOptions& json_err_on_anchor(bool enabled) noexcept { return set_flags_(enabled, JSON_ERR_ON_ANCHOR); }
    C4_ALWAYS_INLINE bool json_err_on_anchor() const noexcept { return (m_flags & JSON_ERR_ON_ANCHOR) != 0; }

    /** @} */

public:

    /** @name maximum depth for the emitted tree
     *
     * This prevents stack overflows by making the emitter fail when
     * the tree exceeds the maximum depth.
     *
     * @{ */
    C4_ALWAYS_INLINE id_type max_depth() const noexcept { return m_max_depth; }
    EmitOptions& max_depth(id_type d) noexcept { m_max_depth = d; return *this; }
    static constexpr const id_type max_depth_default = 64;
    /** @} */

public:

    bool operator== (const EmitOptions& that) const noexcept
    {
        return m_max_depth == that.m_max_depth
            && m_max_cols == that.m_max_cols
            && m_flags == that.m_flags;
    }

private:

    /** @cond dev */
    id_type m_max_depth{max_depth_default};
    id_type m_max_cols{max_cols_default};
    uint32_t m_flags{DEFAULT_FLAGS};
    /** @endcond */

public: // deprecated methods

    /** @cond dev */
    RYML_DEPRECATED("use .json_err_on_{tag,anchor}()") C4_ALWAYS_INLINE Flags_e json_error_flags() const noexcept { return static_cast<Flags_e>(m_flags & JSON_ERR_MASK_); }
    RYML_DEPRECATED("use .json_err_on_{tag,anchor}()") EmitOptions& json_error_flags(Flags_e d) noexcept { m_flags = (d & JSON_ERR_MASK_); return *this; }
    /** @endcond */

};

} // namespace yml
} // namespace c4

#endif /* C4_YML_EMIT_HPP_ */
