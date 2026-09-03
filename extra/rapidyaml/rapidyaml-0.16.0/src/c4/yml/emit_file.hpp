#ifndef C4_YML_EMIT_FILE_HPP_
#define C4_YML_EMIT_FILE_HPP_

/** @file emit_file.hpp */

#include <cstdio>

#ifndef C4_YML_COMMON_HPP_
#include "c4/yml/common.hpp"
#endif


namespace c4 {
namespace yml {

// fwd declarations
/** @cond dev */
template<class Writer> class Emitter;
struct WriterFile;
class Tree;
class ConstNodeRef;
struct EmitOptions;
/** @endcond */


/** @ingroup doc_emit_to_file */
using EmitterFile = Emitter<WriterFile>;


// emit from root -------------------------

/** @addtogroup doc_emit_to_file_from_root
 *
 * @{
 */

/** (1) emit YAML to the given file, starting at the root node. A null file defaults to stdout. */
RYML_EXPORT void emit_yaml(Tree const& t, EmitOptions const& opts, FILE *f=nullptr);

/** (2) like (1), but use default emit options */
RYML_EXPORT void emit_yaml(Tree const& t, FILE *f=nullptr);


/** (1) emit JSON to the given file. A null file defaults to stdout. */
RYML_EXPORT void emit_json(Tree const& t, EmitOptions const& opts, FILE *f=nullptr);

/** (2) like (1), but use default emit options */
RYML_EXPORT void emit_json(Tree const& t, FILE *f=nullptr);

/** @} */


// emit from tree and node id -----------------------

/** @addtogroup doc_emit_to_file_from_node_id
 *
 * @{
 */


/** (1) emit YAML to the given file, starting at the given node. A null
 * file defaults to stdout. */
RYML_EXPORT void emit_yaml(Tree const& t, id_type id, EmitOptions const& opts, FILE *f);

/** (2) like (1), but use default emit options */
RYML_EXPORT void emit_yaml(Tree const& t, id_type id, FILE *f);


/** (1) emit JSON to the given file, starting at the given node. A null
 * file defaults to stdout. */
RYML_EXPORT void emit_json(Tree const& t, id_type id, EmitOptions const& opts, FILE *f);

/** (2) like (1), but use default emit options */
RYML_EXPORT void emit_json(Tree const& t, id_type id, FILE *f);


/** @} */


// emit from ConstNodeRef ------------------------

/** @addtogroup doc_emit_to_file_from_noderef
 *
 * @{
 */

/** (1) emit YAML to the given file. A null file defaults to stdout.
 * Return the number of bytes written. */
RYML_EXPORT void emit_yaml(ConstNodeRef const& r, EmitOptions const& opts, FILE *f=nullptr);

/** (2) like (1), but use default emit options */
RYML_EXPORT void emit_yaml(ConstNodeRef const& r, FILE *f=nullptr);


/** (1) emit JSON to the given file. A null file defaults to stdout.
 * Return the number of bytes written. */
RYML_EXPORT void emit_json(ConstNodeRef const& r, EmitOptions const& opts, FILE *f=nullptr);

/** (2) like (1), but use default emit options */
RYML_EXPORT void emit_json(ConstNodeRef const& r, FILE *f=nullptr);

/** @} */


} // namespace yml
} // namespace c4

#endif /* C4_YML_EMIT_FILE_HPP_ */
