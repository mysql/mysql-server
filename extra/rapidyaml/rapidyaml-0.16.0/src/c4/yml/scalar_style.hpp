#ifndef C4_YML_SCALAR_STYLE_HPP_
#define C4_YML_SCALAR_STYLE_HPP_

/** @file scalar_style.hpp */

#ifndef C4_YML_NODE_TYPE_HPP_
#include "c4/yml/node_type.hpp"
#endif

namespace c4 {
namespace yml {

/** @addtogroup doc_scalar_style
 *
 * @{
 */

/** query whether a scalar can be encoded using single quotes.
 * It may not be possible, notably when there is leading
 * whitespace after a newline. */
RYML_EXPORT bool scalar_style_query_squo(csubstr scalar) noexcept;


/** query whether a scalar can be encoded using plain style while in
 * flow mode. Plain scalars [have more constraints in flow mode than
 * in block
 * mode](https://www.yaml.info/learn/quote.html#noplain). @ref
 * scalar_style_query_plain_block() is the block mode analogous function.*/
RYML_EXPORT bool scalar_style_query_plain_flow(csubstr scalar) noexcept;


/** query whether a scalar can be encoded using plain style while in
 * block mode. Plain scalars [have more constraints in flow mode than
 * in block
 * mode](https://www.yaml.info/learn/quote.html#noplain). @ref
 * scalar_style_query_plain_flow() is the flow mode analogous function.*/
RYML_EXPORT bool scalar_style_query_plain_block(csubstr scalar) noexcept;


/** choose a YAML scalar style based on the scalar's contents, while
 * in flow mode. Plain scalars [have more constraints in flow mode
 * than in block
 * mode](https://www.yaml.info/learn/quote.html#noplain). @ref
 * scalar_style_choose_block() is the block mode analogous
 * function. */
inline NodeType scalar_style_choose_flow(csubstr scalar) noexcept
{
    if(scalar.len)
    {
        if(scalar_style_query_plain_flow(scalar))
            return SCALAR_PLAIN;
        else if(scalar_style_query_squo(scalar))
            return SCALAR_SQUO;
        return SCALAR_DQUO;
    }
    return scalar.str ? SCALAR_SQUO : SCALAR_PLAIN;
}


/** choose a YAML scalar style based on the scalar's contents, while
 * in block mode. Plain scalars [have more constraints in flow mode
 * than in block
 * mode](https://www.yaml.info/learn/quote.html#noplain). @ref
 * scalar_style_choose_block() is the flow mode analogous function. */
RYML_EXPORT NodeType scalar_style_choose_block(csubstr scalar) noexcept;


/** choose a json scalar style based on the scalar's contents */
RYML_EXPORT NodeType scalar_style_choose_json(csubstr scalar) noexcept;


//-----------------------------------------------------------------------------

/** @cond dev */ // LCOV_EXCL_START
RYML_DEPRECATED("use scalar_style_query_plain_{flow,block}()")
inline bool scalar_style_query_plain(csubstr s, bool flow=true) noexcept
{
    return flow ? scalar_style_query_plain_flow(s) : scalar_style_query_plain_block(s);
}
RYML_DEPRECATED("use scalar_style_choose_{flow,block}()")
inline NodeType scalar_style_choose(csubstr s, bool flow=true) noexcept
{
    return flow ? scalar_style_choose_flow(s) : scalar_style_choose_block(s);
}
RYML_DEPRECATED("use scalar_style_choose_json()")
inline NodeType scalar_style_json_choose(csubstr scalar) noexcept
{
    return scalar_style_choose_json(scalar);
}
/** @endcond */ // LCOV_EXCL_STOP

/** @} */

} // namespace yml
} // namespace c4

#endif /* C4_YML_SCALAR_STYLE_HPP_ */
