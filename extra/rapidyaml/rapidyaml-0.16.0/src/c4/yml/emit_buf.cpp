#ifndef C4_YML_EMIT_BUF_HPP_
#include "c4/yml/emit_buf.hpp"
#endif
#ifndef C4_YML_WRITER_BUF_HPP_
#include "c4/yml/writer_buf.hpp"
#endif
#ifndef C4_YML_TREE_HPP_
#include "c4/yml/tree.hpp"
#endif
#ifndef C4_YML_NODE_HPP_
#include "c4/yml/node.hpp"
#endif
#ifndef C4_YML_EMITTER_DEF_HPP_
#include "c4/yml/emitter.def.hpp"
#endif


namespace c4 {
namespace yml {


// instantiate the template class
template class RYML_EXPORT Emitter<WriterBuf>;


// emit from root -------------------------

substr emit_yaml(Tree const& t, EmitOptions const& opts, substr buf, bool error_on_excess)
{
    EmitterBuf em(opts, buf);
    em.emit_as(EMIT_YAML, &t);
    return em.get_result(error_on_excess);
}

substr emit_yaml(Tree const& t, substr buf, bool error_on_excess)
{
    EmitterBuf em(EmitOptions{}, buf);
    em.emit_as(EMIT_YAML, &t);
    return em.get_result(error_on_excess);
}

substr emit_json(Tree const& t, EmitOptions const& opts, substr buf, bool error_on_excess)
{
    EmitterBuf em(opts, buf);
    em.emit_as(EMIT_JSON, &t);
    return em.get_result(error_on_excess);
}

substr emit_json(Tree const& t, substr buf, bool error_on_excess)
{
    EmitterBuf em(EmitOptions{}, buf);
    em.emit_as(EMIT_JSON, &t);
    return em.get_result(error_on_excess);
}


// emit from tree and node id -----------------------

substr emit_yaml(Tree const& t, id_type id, EmitOptions const& opts, substr buf, bool error_on_excess)
{
    EmitterBuf em(opts, buf);
    em.emit_as(EMIT_YAML, &t, id);
    return em.get_result(error_on_excess);
}

substr emit_yaml(Tree const& t, id_type id, substr buf, bool error_on_excess)
{
    EmitterBuf em(EmitOptions{}, buf);
    em.emit_as(EMIT_YAML, &t, id);
    return em.get_result(error_on_excess);
}

substr emit_json(Tree const& t, id_type id, EmitOptions const& opts, substr buf, bool error_on_excess)
{
    EmitterBuf em(opts, buf);
    em.emit_as(EMIT_JSON, &t, id);
    return em.get_result(error_on_excess);
}

substr emit_json(Tree const& t, id_type id, substr buf, bool error_on_excess)
{
    EmitterBuf em(EmitOptions{}, buf);
    em.emit_as(EMIT_JSON, &t, id);
    return em.get_result(error_on_excess);
}


// emit from ConstNodeRef ------------------------

substr emit_yaml(ConstNodeRef const& r, EmitOptions const& opts, substr buf, bool error_on_excess)
{
    EmitterBuf em(opts, buf);
    em.emit_as(EMIT_YAML, r.tree(), r.id());
    return em.get_result(error_on_excess);
}

substr emit_yaml(ConstNodeRef const& r, substr buf, bool error_on_excess)
{
    EmitterBuf em(EmitOptions{}, buf);
    em.emit_as(EMIT_YAML, r.tree(), r.id());
    return em.get_result(error_on_excess);
}

substr emit_json(ConstNodeRef const& r, EmitOptions const& opts, substr buf, bool error_on_excess)
{
    EmitterBuf em(opts, buf);
    em.emit_as(EMIT_JSON, r.tree(), r.id());
    return em.get_result(error_on_excess);
}

substr emit_json(ConstNodeRef const& r, substr buf, bool error_on_excess)
{
    EmitterBuf em(EmitOptions{}, buf);
    em.emit_as(EMIT_JSON, r.tree(), r.id());
    return em.get_result(error_on_excess);
}

} // namespace yml
} // namespace c4
