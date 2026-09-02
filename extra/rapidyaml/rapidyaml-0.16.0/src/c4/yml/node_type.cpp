#ifndef C4_YML_NODE_TYPE_HPP_
#include "c4/yml/node_type.hpp"
#endif
#ifndef C4_YML_ERROR_HPP_
#include "c4/yml/error.hpp"
#endif


namespace c4 {
namespace yml {

const char* NodeType::type_str(type_bits ty) noexcept
{
    switch(ty & TYMASK_)
    {
    case KEYVAL:
        return "KEYVAL";
    case KEY:
        return "KEY";
    case VAL:
        return "VAL";
    case MAP:
        return "MAP";
    case SEQ:
        return "SEQ";
    case KEYMAP:
        return "KEYMAP";
    case KEYSEQ:
        return "KEYSEQ";
    case DOCSEQ:
        return "DOCSEQ";
    case DOCMAP:
        return "DOCMAP";
    case DOCVAL:
        return "DOCVAL";
    case DOC:
        return "DOC";
    case STREAM:
        return "STREAM";
    case NOTYPE:
        return "NOTYPE";
    default:
        if((ty & KEYVAL) == KEYVAL)
            return "KEYVAL***";
        if((ty & KEYMAP) == KEYMAP)
            return "KEYMAP***";
        if((ty & KEYSEQ) == KEYSEQ)
            return "KEYSEQ***";
        if((ty & DOCSEQ) == DOCSEQ)
            return "DOCSEQ***";
        if((ty & DOCMAP) == DOCMAP)
            return "DOCMAP***";
        if((ty & DOCVAL) == DOCVAL)
            return "DOCVAL***";
        if(ty & KEY)
            return "KEY***";
        if(ty & VAL)
            return "VAL***";
        if(ty & MAP)
            return "MAP***";
        if(ty & SEQ)
            return "SEQ***";
        if(ty & DOC)
            return "DOC***";
        return "(unk)";
    }
}

namespace {
struct type_and_name { const char* str; type_bits bits; };
constexpr const type_and_name type_names[] = {
    {"STREAM", STREAM},
    {"DOC", DOC},
    // key properties
    {"KEY", KEY},
    {"KNIL", KEYNIL},
    {"KTAG", KEYTAG},
    {"KANCH", KEYANCH},
    {"KREF", KEYREF},
    {"KLITERAL", KEY_LITERAL},
    {"KFOLDED", KEY_FOLDED},
    {"KSQUO", KEY_SQUO},
    {"KDQUO", KEY_DQUO},
    {"KPLAIN", KEY_PLAIN},
    {"KUNFILT", KEY_UNFILT},
    // val properties
    {"VAL", VAL},
    {"VNIL", VALNIL},
    {"VTAG", VALTAG},
    {"VANCH", VALANCH},
    {"VREF", VALREF},
    {"VLITERAL", VAL_LITERAL},
    {"VFOLDED", VAL_FOLDED},
    {"VSQUO", VAL_SQUO},
    {"VDQUO", VAL_DQUO},
    {"VPLAIN", VAL_PLAIN},
    {"VUNFILT", VAL_UNFILT},
    // container properties
    {"MAP", MAP},
    {"SEQ", SEQ},
    {"FLOWSL", FLOW_SL},
    {"FLOWML1", FLOW_ML1},
    {"FLOWMLN", FLOW_MLN},
    {"FLOWSPC", FLOW_SPC},
    {"BLCK", BLOCK},
};
} // namespace
size_t NodeType::type_str(substr buf, type_bits flags) noexcept
{
    detail::SubstrWriter_ writer(buf);
    for(type_and_name const tn : type_names)
    {
        if((flags & tn.bits) == tn.bits)
        {
            if(writer.pos)
                writer.append('|');
            writer.append(tn.str);
            flags = flags & ~tn.bits; // remove the flag
        }
    }
    if(!writer.pos)
        writer.append("NOTYPE");
    if(writer.pos < buf.len)
        buf[writer.pos] = '\0';
    return writer.pos;
}

} // namespace yml
} // namespace c4
