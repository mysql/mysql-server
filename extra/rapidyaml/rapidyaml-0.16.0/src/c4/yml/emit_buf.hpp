#ifndef C4_YML_EMIT_BUF_HPP_
#define C4_YML_EMIT_BUF_HPP_

/** @file emit_buf.hpp Utilities to emit YAML and JSON to buffers or containers. */

#ifndef C4_YML_COMMON_HPP_
#include "c4/yml/common.hpp"
#endif

namespace c4 {
namespace yml {


/** @cond dev */
// fwd declarations
template<class Writer>
class Emitter;
struct WriterBuf;
class Tree;
class ConstNodeRef;
struct EmitOptions;
/** @endcond */


/** @ingroup doc_emit_to_buffer */
using EmitterBuf = Emitter<WriterBuf>;


// emit from root -------------------------

/** @addtogroup doc_emit_to_buffer_from_root
 *
 * @{
 */

/** (1) emit YAML to the given buffer. Return a substr trimmed to the emitted YAML.
 * @param t the tree; will be emitted from the root node.
 * @param opts emit options.
 * @param buf the output buffer.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the
 * buffer is insufficient (when error_on_excess is false), the string
 * pointer of the result will be set to null, and the length will
 * report the required buffer size. For example usage, see @ref
 * emitrs_yaml() . */
RYML_EXPORT substr emit_yaml(Tree const& t, EmitOptions const& opts, substr buf, bool error_on_excess=true);
/** (2) like (1), but use default emit options */
RYML_EXPORT substr emit_yaml(Tree const& t, substr buf, bool error_on_excess=true);

/** (1) emit JSON to the given buffer. Return a substr trimmed to the emitted JSON.
 * @param t the tree; will be emitted from the root node.
 * @param opts emit options.
 * @param buf the output buffer.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
RYML_EXPORT substr emit_json(Tree const& t, EmitOptions const& opts, substr buf, bool error_on_excess=true);
/** (2) like (1), but use default emit options */
RYML_EXPORT substr emit_json(Tree const& t, substr buf, bool error_on_excess=true);

/** @} */


// emit from tree and node id -----------------------

/** @addtogroup doc_emit_to_buffer_from_node_id
 *
 * @{
 */

/** (1) emit YAML to the given buffer. Return a substr trimmed to the emitted YAML.
 * @param t the tree to emit.
 * @param id the node where to start emitting.
 * @param opts emit options.
 * @param buf the output buffer.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
RYML_EXPORT substr emit_yaml(Tree const& t, id_type id, EmitOptions const& opts, substr buf, bool error_on_excess=true);
/** (2) like (1), but use default emit options */
RYML_EXPORT substr emit_yaml(Tree const& t, id_type id, substr buf, bool error_on_excess=true);

/** (1) emit JSON to the given buffer. Return a substr trimmed to the emitted JSON.
 * @param t the tree to emit.
 * @param id the node where to start emitting.
 * @param opts emit options.
 * @param buf the output buffer.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
RYML_EXPORT substr emit_json(Tree const& t, id_type id, EmitOptions const& opts, substr buf, bool error_on_excess=true);
/** (2) like (1), but use default emit options */
RYML_EXPORT substr emit_json(Tree const& t, id_type id, substr buf, bool error_on_excess=true);

/** @} */


// emit from ConstNodeRef ------------------------

/** @addtogroup doc_emit_to_buffer_from_noderef
 *
 * @{
 */

/** (1) emit YAML to the given buffer. Return a substr trimmed to the emitted YAML.
 * @param r the starting node.
 * @param buf the output buffer.
 * @param opts emit options.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
RYML_EXPORT substr emit_yaml(ConstNodeRef const& r, EmitOptions const& opts, substr buf, bool error_on_excess=true);
/** (2) like (1), but use default emit options */
RYML_EXPORT substr emit_yaml(ConstNodeRef const& r, substr buf, bool error_on_excess=true);

/** (1) emit JSON to the given buffer. Return a substr trimmed to the emitted JSON.
 * @param r the starting node.
 * @param buf the output buffer.
 * @param opts emit options.
 * @param error_on_excess Raise an error if the space in the buffer is insufficient.
 * @return a substr trimmed to the result in the output buffer. If the buffer is
 * insufficient (when error_on_excess is false), the string pointer of the
 * result will be set to null, and the length will report the required buffer size. */
RYML_EXPORT substr emit_json(ConstNodeRef const& r, EmitOptions const& opts, substr buf, bool error_on_excess=true);
/** (2) like (1), but use default emit options */
RYML_EXPORT substr emit_json(ConstNodeRef const& r, substr buf, bool error_on_excess=true);

/** @} */

} // namespace yml
} // namespace c4

#endif /* C4_YML_EMIT_HPP_ */
