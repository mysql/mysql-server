#ifndef C4_YML_EMIT_HPP_
#define C4_YML_EMIT_HPP_

/** @file emit.hpp Umbrella header with all utilities to emit YAML and JSON. */


#ifndef C4_YML_EMIT_BUF_HPP_
#include "c4/yml/emit_buf.hpp"
#endif
#ifndef C4_YML_EMIT_CONTAINER_HPP_
#include "c4/yml/emit_container.hpp"
#endif
#ifndef C4_YML_EMIT_FILE_HPP_
#include "c4/yml/emit_file.hpp"
#endif
#ifndef C4_YML_EMIT_OSTREAM_HPP_
#include "c4/yml/emit_ostream.hpp"
#endif


namespace c4 {
namespace yml {


//-------------------------------------------------
// deprecated code

/** @cond dev */ // LCOV_EXCL_START

#define RYML_DEPRECATE_EMIT                                             \
    RYML_DEPRECATED("use emit_yaml() instead. "                         \
                    "See https://github.com/biojppm/rapidyaml/issues/120")
#define RYML_DEPRECATE_EMITRS                                           \
    RYML_DEPRECATED("use emitrs_yaml() instead. "                       \
                    "See https://github.com/biojppm/rapidyaml/issues/120")

// workaround for Qt emit which is a macro;
// see https://github.com/biojppm/rapidyaml/issues/120.
// emit is defined in qobjectdefs.h (as an empty define).
#ifdef emit
#define RYML_TMP_EMIT_ emit
#undef emit
#endif

RYML_DEPRECATE_EMIT inline void emit(Tree const& t, id_type id, FILE *f)
{
    emit_yaml(t, id, f);
}
RYML_DEPRECATE_EMIT inline void emit(Tree const& t, FILE *f=nullptr)
{
    emit_yaml(t, f);
}
RYML_DEPRECATE_EMIT inline void emit(ConstNodeRef const& r, FILE *f=nullptr)
{
    emit_yaml(r, f);
}

RYML_DEPRECATE_EMIT inline substr emit(Tree const& t, id_type id, substr buf, bool error_on_excess=true)
{
    return emit_yaml(t, id, buf, error_on_excess);
}
RYML_DEPRECATE_EMIT inline substr emit(Tree const& t, substr buf, bool error_on_excess=true)
{
    return emit_yaml(t, buf, error_on_excess);
}
RYML_DEPRECATE_EMIT inline substr emit(ConstNodeRef const& r, substr buf, bool error_on_excess=true)
{
    return emit_yaml(r, buf, error_on_excess);
}

#ifdef RYML_TMP_EMIT_
#define emit RYML_TMP_EMIT_
#undef RYML_TMP_EMIT_
#endif

template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS substr emitrs(Tree const& t, id_type id, CharOwningContainer * cont)
{
    return emitrs_yaml(t, id, cont);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS CharOwningContainer emitrs(Tree const& t, id_type id)
{
    return emitrs_yaml<CharOwningContainer>(t, id);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS substr emitrs(Tree const& t, CharOwningContainer * cont)
{
    return emitrs_yaml(t, cont);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS CharOwningContainer emitrs(Tree const& t)
{
    return emitrs_yaml<CharOwningContainer>(t);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS substr emitrs(ConstNodeRef const& n, CharOwningContainer * cont)
{
    return emitrs_yaml(n, cont);
}
template<class CharOwningContainer>
RYML_DEPRECATE_EMITRS CharOwningContainer emitrs(ConstNodeRef const& n)
{
    return emitrs_yaml<CharOwningContainer>(n);
}

#undef RYML_DEPRECATE_EMIT
#undef RYML_DEPRECATE_EMITRS

/** @endcond */ // LCOV_EXCL_STOP


} // namespace yml
} // namespace c4

#endif /* C4_YML_EMIT_HPP_ */
