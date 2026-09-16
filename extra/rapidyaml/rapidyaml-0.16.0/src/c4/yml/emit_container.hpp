#ifndef C4_YML_EMIT_CONTAINER_HPP_
#define C4_YML_EMIT_CONTAINER_HPP_

/** @file emit.hpp Utilities to emit YAML and JSON to resizeable containers. */

#ifndef C4_YML_EMIT_BUF_HPP_
#include "c4/yml/emit_buf.hpp"
#endif
#ifndef C4_YML_EMIT_OPTIONS_HPP_
#include "c4/yml/emit_options.hpp"
#endif
#ifndef C4_YML_NODE_HPP_
#include "c4/yml/node.hpp"
#endif


namespace c4 {
namespace yml {


// emit from tree and node id ---------------------------

/** @addtogroup doc_emit_to_container_from_node_id
 *
 * @{
 */


/** (1) emit+resize: emit YAML to the given `std::string`/`std::vector<char>`-like
 * container, resizing it as needed to fit the emitted YAML. If @p append is
 * set to true, the emitted YAML is appended at the end of the container.
 *
 * @return a substr trimmed to the emitted YAML (excluding the initial contents, when appending) */
template<class CharOwningContainer>
substr emitrs_yaml(Tree const& t, id_type id, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    const size_t startpos = append ? cont->size() : 0u;
    cont->resize(cont->capacity()); // otherwise the first emit would be certain to fail
    substr buf = to_substr(*cont).sub(startpos);
    substr ret = emit_yaml(t, id, opts, buf, /*error_on_excess*/false);
    cont->resize(startpos + ret.len);
    if(ret.len && !ret.str)
    {
        buf = to_substr(*cont).sub(startpos);
        ret = emit_yaml(t, id, opts, buf, /*error_on_excess*/true);
    }
    return ret;
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_yaml(Tree const& t, id_type id, CharOwningContainer * cont, bool append=false)
{
    return emitrs_yaml(t, id, EmitOptions{}, cont, append);
}


/** (1) emit+resize: emit JSON to the given `std::string`/`std::vector<char>`-like
 * container, resizing it as needed to fit the emitted JSON. If @p append is
 * set to true, the emitted YAML is appended at the end of the container.
 *
 * @return a substr trimmed to the emitted JSON (excluding the initial contents, when appending) */
template<class CharOwningContainer>
substr emitrs_json(Tree const& t, id_type id, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    const size_t startpos = append ? cont->size() : 0u;
    cont->resize(cont->capacity()); // otherwise the first emit would be certain to fail
    substr buf = to_substr(*cont).sub(startpos);
    substr ret = emit_json(t, id, opts, buf, /*error_on_excess*/false);
    cont->resize(startpos + ret.len);
    if(ret.len && !ret.str)
    {
        buf = to_substr(*cont).sub(startpos);
        ret = emit_json(t, id, opts, buf, /*error_on_excess*/true);
    }
    return ret;
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_json(Tree const& t, id_type id, CharOwningContainer * cont, bool append=false)
{
    return emitrs_json(t, id, EmitOptions{}, cont, append);
}


/** (3) emit+resize: YAML to a newly-created `std::string`/`std::vector<char>`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_yaml(Tree const& t, id_type id, EmitOptions const& opts={})
{
    CharOwningContainer c;
    emitrs_yaml(t, id, opts, &c);
    return c;
}
/** (3) emit+resize: JSON to a newly-created `std::string`/`std::vector<char>`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_json(Tree const& t, id_type id, EmitOptions const& opts={})
{
    CharOwningContainer c;
    emitrs_json(t, id, opts, &c);
    return c;
}

/** @} */


// emit from root -------------------------

/** @addtogroup doc_emit_to_container_from_root
 *
 * @{
 */


/** (1) emit+resize: YAML to the given `std::string`/`std::vector<char>`-like
 * container, resizing it as needed to fit the emitted YAML.
 * @return a substr trimmed to the new emitted contents. */
template<class CharOwningContainer>
substr emitrs_yaml(Tree const& t, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    return emitrs_yaml(t, t.root_id_maybe(), opts, cont, append);
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_yaml(Tree const& t, CharOwningContainer * cont, bool append=false)
{
    return emitrs_yaml(t, t.root_id_maybe(), EmitOptions{}, cont, append);
}


/** (1) emit+resize: JSON to the given `std::string`/`std::vector<char>`-like
 * container, resizing it as needed to fit the emitted JSON.
 * @return a substr trimmed to the new emitted contents. */
template<class CharOwningContainer>
substr emitrs_json(Tree const& t, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    return emitrs_json(t, t.root_id_maybe(), opts, cont, append);
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_json(Tree const& t, CharOwningContainer * cont, bool append=false)
{
    return emitrs_json(t, t.root_id_maybe(), EmitOptions{}, cont, append);
}


/** (3) emit+resize: YAML to a newly-created `std::string`/`std::vector<char>`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_yaml(Tree const& t, EmitOptions const& opts={})
{
    CharOwningContainer c;
    emitrs_yaml(t, t.root_id_maybe(), opts, &c);
    return c;
}
/** (3) emit+resize: JSON to a newly-created `std::string`/`std::vector<char>`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_json(Tree const& t, EmitOptions const& opts={})
{
    CharOwningContainer c;
    emitrs_json(t, t.root_id_maybe(), opts, &c);
    return c;
}

/** @} */


// emit from ConstNodeRef ------------------------

/** @addtogroup doc_emit_to_container_from_noderef
 *
 * @{
 */

/** (1) emit+resize: YAML to the given `std::string`/`std::vector<char>`-like container,
 * resizing it as needed to fit the emitted YAML.
 * @return a substr trimmed to the new emitted contents */
template<class CharOwningContainer>
substr emitrs_yaml(ConstNodeRef const& n, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    if(!n.readable())
        return {};
    return emitrs_yaml(*n.tree(), n.id(), opts, cont, append);
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_yaml(ConstNodeRef const& n, CharOwningContainer * cont, bool append=false)
{
    if(!n.readable())
        return {};
    return emitrs_yaml(*n.tree(), n.id(), EmitOptions{}, cont, append);
}


/** (1) emit+resize: JSON to the given `std::string`/`std::vector<char>`-like container,
 * resizing it as needed to fit the emitted JSON.
 * @return a substr trimmed to the new emitted contents */
template<class CharOwningContainer>
substr emitrs_json(ConstNodeRef const& n, EmitOptions const& opts, CharOwningContainer * cont, bool append=false)
{
    if(!n.readable())
        return {};
    return emitrs_json(*n.tree(), n.id(), opts, cont, append);
}
/** (2) like (1), but use default emit options */
template<class CharOwningContainer>
substr emitrs_json(ConstNodeRef const& n, CharOwningContainer * cont, bool append=false)
{
    if(!n.readable())
        return {};
    return emitrs_json(*n.tree(), n.id(), EmitOptions{}, cont, append);
}


/** (3) emit+resize: YAML to a newly-created `std::string`/`std::vector<char>`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_yaml(ConstNodeRef const& n, EmitOptions const& opts={})
{
    CharOwningContainer c;
    if(n.readable())
        emitrs_yaml(*n.tree(), n.id(), opts, &c);
    return c;
}
/** (3) emit+resize: JSON to a newly-created `std::string`/`std::vector<char>`-like container. */
template<class CharOwningContainer>
CharOwningContainer emitrs_json(ConstNodeRef const& n, EmitOptions const& opts={})
{
    CharOwningContainer c;
    if(n.readable())
        emitrs_json(*n.tree(), n.id(), opts, &c);
    return c;
}

/** @} */

} // namespace yml
} // namespace c4

#endif /* C4_YML_EMIT_CONTAINER_HPP_ */
