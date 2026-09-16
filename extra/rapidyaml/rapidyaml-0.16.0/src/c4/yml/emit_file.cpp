#ifndef C4_YML_EMIT_FILE_HPP_
#include "c4/yml/emit_file.hpp"
#endif
#ifndef C4_YML_WRITER_FILE_HPP_
#include "c4/yml/writer_file.hpp"
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
template class RYML_EXPORT Emitter<WriterFile>;


// emit from root -------------------------

void emit_yaml(Tree const& t, EmitOptions const& opts, FILE *f)
{
    EmitterFile em(opts, f);
    em.emit_as(EMIT_YAML, &t);
}

void emit_yaml(Tree const& t, FILE *f)
{
    EmitterFile em(EmitOptions{}, f);
    em.emit_as(EMIT_YAML, &t);
}


void emit_json(Tree const& t, EmitOptions const& opts, FILE *f)
{
    EmitterFile em(opts, f);
    em.emit_as(EMIT_JSON, &t);
}

void emit_json(Tree const& t, FILE *f)
{
    EmitterFile em(EmitOptions{}, f);
    em.emit_as(EMIT_JSON, &t);
}


// emit from tree and node id -----------------------

void emit_yaml(Tree const& t, id_type id, EmitOptions const& opts, FILE *f)
{
    EmitterFile em(opts, f);
    em.emit_as(EMIT_YAML, &t, id);
}

void emit_yaml(Tree const& t, id_type id, FILE *f)
{
    EmitterFile em(EmitOptions{}, f);
    em.emit_as(EMIT_YAML, &t, id);
}

void emit_json(Tree const& t, id_type id, EmitOptions const& opts, FILE *f)
{
    EmitterFile em(opts, f);
    em.emit_as(EMIT_JSON, &t, id);
}

void emit_json(Tree const& t, id_type id, FILE *f)
{
    EmitterFile em(EmitOptions{}, f);
    em.emit_as(EMIT_JSON, &t, id);
}


// emit from ConstNodeRef ------------------------

void emit_yaml(ConstNodeRef const& r, EmitOptions const& opts, FILE *f)
{
    EmitterFile em(opts, f);
    em.emit_as(EMIT_YAML, r.tree(), r.id());
}

void emit_yaml(ConstNodeRef const& r, FILE *f)
{
    EmitterFile em(EmitOptions{}, f);
    em.emit_as(EMIT_YAML, r.tree(), r.id());
}

void emit_json(ConstNodeRef const& r, EmitOptions const& opts, FILE *f)
{
    EmitterFile em(opts, f);
    em.emit_as(EMIT_JSON, r.tree(), r.id());
}

void emit_json(ConstNodeRef const& r, FILE *f)
{
    EmitterFile em(EmitOptions{}, f);
    em.emit_as(EMIT_JSON, r.tree(), r.id());
}

} // namespace yml
} // namespace c4
