// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_OPENMETHOD_COMPILER_HPP
#define BOOST_OPENMETHOD_COMPILER_HPP

#include <boost/openmethod/core.hpp>
#include <boost/openmethod/detail/ostdstream.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/assert.hpp>
#include <boost/dynamic_bitset.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4456)
#pragma warning(disable : 4458)
#pragma warning(disable : 4702) // unreachable code
#endif

namespace boost::openmethod {

namespace detail {

template<class Reports, class Facets, typename = void>
struct aggregate_reports;

template<class... Reports, class Facet, class... MoreFacets>
struct aggregate_reports<
    mp11::mp_list<Reports...>, mp11::mp_list<Facet, MoreFacets...>,
    std::void_t<typename Facet::report>> {
    using type = typename aggregate_reports<
        mp11::mp_list<Reports..., typename Facet::report>,
        mp11::mp_list<MoreFacets...>>::type;
};

template<class... Reports, class Facet, class... MoreFacets, typename Void>
struct aggregate_reports<
    mp11::mp_list<Reports...>, mp11::mp_list<Facet, MoreFacets...>, Void> {
    using type = typename aggregate_reports<
        mp11::mp_list<Reports...>, mp11::mp_list<MoreFacets...>>::type;
};

template<class... Reports, typename Void>
struct aggregate_reports<mp11::mp_list<Reports...>, mp11::mp_list<>, Void> {
    struct type : Reports... {};
};

inline void merge_into(boost::dynamic_bitset<>& a, boost::dynamic_bitset<>& b) {
    if (b.size() < a.size()) {
        b.resize(a.size());
    }

    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i]) {
            b[i] = true;
        }
    }
}

inline void set_bit(boost::dynamic_bitset<>& mask, std::size_t bit) {
    if (bit >= mask.size()) {
        mask.resize(bit + 1);
    }

    mask[bit] = true;
}

struct generic_compiler {

    struct method;

    struct parameter {
        struct method* method;
        std::size_t param;
    };

    struct vtbl_entry {
        std::size_t method_index, vp_index, group_index;
    };

    struct class_ {
        bool is_abstract = false;
        std::vector<type_id> type_ids;
        std::vector<class_*> transitive_bases;
        std::vector<class_*> direct_bases;
        std::vector<class_*> direct_derived;
        std::unordered_set<class_*> transitive_derived;
        std::vector<parameter> used_by_vp;
        boost::dynamic_bitset<> used_slots;
        boost::dynamic_bitset<> reserved_slots;
        std::size_t first_slot = 0;
        std::size_t mark = 0; // temporary mark to detect cycles
        std::vector<vtbl_entry> vtbl;
        vptr_type* static_vptr;

        auto is_base_of(class_* other) const -> bool {
            return transitive_derived.find(other) != transitive_derived.end();
        }

        auto vptr() const -> const vptr_type& {
            return *static_vptr;
        }

        auto type_id_begin() const {
            return type_ids.begin();
        }

        auto type_id_end() const {
            return type_ids.end();
        }
    };

    struct overrider {
        detail::overrider_info* info = nullptr;
        overrider* next = nullptr;
        std::vector<class_*> vp;
        class_* covariant_return_type = nullptr;
        void (*pf)();
        std::size_t method_index, spec_index;
    };

    using bitvec = boost::dynamic_bitset<>;

    struct group {
        std::vector<class_*> classes;
        bool has_concrete_classes{false};
    };

    using group_map = std::map<bitvec, group>;

    struct method_report {
        std::size_t cells = 0;
        std::size_t not_implemented = 0;
        std::size_t ambiguous = 0;
    };

    struct report : method_report {};

    static void accumulate(const method_report& partial, report& total);

    struct method {
        detail::method_info* info;
        std::vector<class_*> vp;
        class_* covariant_return_type = nullptr;
        std::vector<overrider> overriders;
        std::vector<std::size_t> slots;
        std::vector<std::size_t> strides;
        std::vector<const overrider*> dispatch_table;
        // following two are dummies, when converting to a function pointer, we will
        // get the corresponding pointer from method_info
        overrider not_implemented;
        overrider ambiguous;
        vptr_type gv_dispatch_table = nullptr;
        auto arity() const {
            return vp.size();
        }
        method_report report;
    };

    const method* operator[](const detail::method_info& info) const {
        auto iter = std::find_if(
            methods.begin(), methods.end(),
            [&info](const method& m) { return m.info == &info; });

        if (iter != methods.end()) {
            return &*iter;
        }

        return nullptr;
    }

    std::deque<class_> classes;

    auto classes_begin() const {
        return classes.begin();
    }

    auto classes_end() const {
        return classes.end();
    }

    std::vector<method> methods;
    std::size_t class_mark = 0;
    bool compilation_done = false;
};

template<class Compiler>
struct trace_stream {
    bool on = false;
    std::size_t indentation_level{0};

    auto operator++() -> trace_stream& {
        if constexpr (Compiler::has_trace) {
            if (on) {
                for (std::size_t i = 0; i < indentation_level; ++i) {
                    Compiler::Registry::output::os << "  ";
                }
            }
        }

        return *this;
    }

    struct indent {
        trace_stream& trace;
        int by;

        explicit indent(trace_stream& trace, int by = 2)
            : trace(trace), by(by) {
            trace.indentation_level += by;
        }

        ~indent() {
            trace.indentation_level -= by;
        }
    };
};

struct rflush {
    std::size_t width;
    std::size_t value;
    explicit rflush(std::size_t width, std::size_t value)
        : width(width), value(value) {
    }
};

struct type_name {
    type_name(type_id type) : type(type) {
    }
    type_id type;
};

template<class Compiler>
auto operator<<(trace_stream<Compiler>& tr, const generic_compiler::class_& cls)
    -> trace_stream<Compiler>& {
    if constexpr (Compiler::has_trace) {
        tr << type_name(cls.type_ids[0]);
    }

    return tr;
}

template<class Compiler, template<typename...> class Container, typename... T>
auto operator<<(
    trace_stream<Compiler>& tr,
    Container<generic_compiler::class_*, T...>& classes)
    -> trace_stream<Compiler>& {
    if constexpr (Compiler::has_trace) {
        tr << "(";
        const char* sep = "";
        for (auto cls : classes) {
            tr << sep << *cls;
            sep = ", ";
        }

        tr << ")";
    }

    return tr;
}

struct spec_name {
    spec_name(
        const detail::generic_compiler::method& method,
        const detail::generic_compiler::overrider* def)
        : method(method), def(def) {
    }
    const detail::generic_compiler::method& method;
    const detail::generic_compiler::overrider* def;
};

template<class Compiler>
auto operator<<(trace_stream<Compiler>& tr, const spec_name& sn)
    -> trace_stream<Compiler>& {
    if (sn.def == &sn.method.not_implemented) {
        tr << "not implemented";
    } else if (sn.def == &sn.method.ambiguous) {
        tr << "ambiguous";
    } else {
        tr << type_name(sn.def->info->type);
    }

    return tr;
}

template<typename Iterator>
struct range;

template<class Compiler, typename T, typename F>
auto write_range(trace_stream<Compiler>& tr, range<T> range, F fn) -> auto& {
    if constexpr (Compiler::has_trace) {
        if (tr.on) {
            tr << "(";
            const char* sep = "";
            for (auto value : range) {
                tr << sep << fn(value);
                sep = ", ";
            }

            tr << ")";
        }
    }

    return tr;
}

template<class Compiler, typename T>
auto operator<<(trace_stream<Compiler>& tr, const T& value) -> auto& {
    if constexpr (Compiler::has_trace) {
        if (tr.on) {
            Compiler::Registry::output::os << value;
        }
    }
    return tr;
}

template<class Compiler>
auto operator<<(trace_stream<Compiler>& tr, const rflush& rf) -> auto& {
    if constexpr (Compiler::has_trace) {
        if (tr.on) {
            std::size_t digits = 1;
            auto tmp = rf.value / 10;

            while (tmp) {
                ++digits;
                tmp /= 10;
            }

            while (digits < rf.width) {
                tr << " ";
                ++digits;
            }

            tr << rf.value;
        }
    }

    return tr;
}

template<class Compiler>
auto operator<<(trace_stream<Compiler>& tr, const boost::dynamic_bitset<>& bits)
    -> auto& {
    if constexpr (Compiler::has_trace) {
        if (tr.on) {
            auto i = bits.size();
            while (i != 0) {
                --i;
                Compiler::Registry::output::os << bits[i];
            }
        }
    }

    return tr;
}

template<class Compiler>
auto operator<<(trace_stream<Compiler>& tr, const range<type_id*>& tips)
    -> auto& {
    return write_range(tr, tips, [](auto tip) { return type_name(tip); });
}

template<class Compiler, typename T>
auto operator<<(trace_stream<Compiler>& tr, const range<T>& range) -> auto& {
    return write_range(tr, range, [](auto value) { return value; });
}

template<class Compiler>
auto operator<<(trace_stream<Compiler>& tr, const type_name& manip) -> auto& {
    if constexpr (Compiler::has_trace) {
        Compiler::Registry::rtti::type_name(manip.type, tr);
    }

    return tr;
}
} // namespace detail

// Definition of the nested template struct outside the registry class
template<class... Policies>
template<class... Options>
struct registry<Policies...>::compiler : detail::generic_compiler {
    using type_index_type = decltype(rtti::type_index(0));

    typename detail::aggregate_reports<mp11::mp_list<report>, policy_list>::type
        report;

    std::unordered_map<type_index_type, class_*> class_map;

    using Registry = registry;

    compiler(Options... opts);

    auto compile();
    void initialize();
    void install_global_tables();

    void augment_classes();
    void collect_transitive_bases(class_* cls, class_* base);
    void calculate_transitive_derived(class_& cls);
    void augment_methods();
    void assign_slots();
    void assign_tree_slots(class_& cls, std::size_t base_slot);
    void assign_lattice_slots(class_& cls);
    void build_dispatch_tables();
    void build_dispatch_table(
        method& m, std::size_t dim,
        std::vector<group_map>::const_iterator group, const bitvec& candidates,
        bool concrete);
    void write_global_data();
    void print(const method_report& report) const;
    static void select_dominant_overriders(
        std::vector<overrider*>& dominants, std::size_t& pick,
        std::size_t& remaining);
    static auto
    is_more_specific(const overrider* a, const overrider* b) -> bool;
    static auto is_base(const overrider* a, const overrider* b) -> bool;

    std::tuple<Options...> options;

    template<class Option>
    static constexpr bool has_option =
        mp11::mp_contains<mp11::mp_list<Options...>, Option>::value;

    static constexpr bool has_trace = has_option<trace>;
    static constexpr bool has_n2216 = has_option<n2216>;

    mutable detail::trace_stream<compiler> tr;
    using indent = typename detail::trace_stream<compiler>::indent;
};

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::install_global_tables() {
    if (!compilation_done) {
        abort();
    }

    write_global_data();

    print(report);
    ++tr << "Finished\n";
}

template<class... Policies>
template<class... Options>
auto registry<Policies...>::compiler<Options...>::compile() {
    augment_classes();
    augment_methods();
    assign_slots();
    build_dispatch_tables();

    compilation_done = true;

    return report;
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::initialize() {
    compile();
    install_global_tables();
    registry<Policies...>::initialized = true;
}

#ifdef _MSC_VER
namespace detail {

template<bool HasTrace, typename T>
struct msvc_tuple_get;

template<typename T>
struct msvc_tuple_get<true, T> {
    template<class Tuple>
    static decltype(auto) fn(const Tuple& t) {
        return std::get<T>(t);
    }
};

template<typename T>
struct msvc_tuple_get<false, T> {
    template<class Tuple>
    static decltype(auto) fn(const Tuple&) {
        return T();
    }
};
} // namespace detail
#endif

template<class... Policies>
template<class... Options>
registry<Policies...>::compiler<Options...>::compiler(Options... opts)
    : options(opts...) {
    if constexpr (has_trace) {
#ifdef _MSC_VER
        tr.on = detail::msvc_tuple_get<has_trace, trace>::fn(options).on;
#else
        // Even with the constexpr has_trace guard, msvc errors on this.
        tr.on = std::get<trace>(options).on;
#endif
    }
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::collect_transitive_bases(
    class_* cls, class_* base) {
    if (base->mark == class_mark) {
        return;
    }

    cls->transitive_bases.push_back(base);
    base->mark = class_mark;

    for (auto base_base : base->transitive_bases) {
        collect_transitive_bases(cls, base_base);
    }
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::augment_classes() {
    using namespace detail;

    // scope
    {
        ++tr << "Static class info:\n";

        // The standard does not guarantee that there is exactly one
        // type_info object per class. However, it guarantees that the
        // type_index for a class has a unique value.
        for (auto& cr : registry::classes) {
            if constexpr (has_deferred_static_rtti) {
                static_cast<deferred_class_info&>(cr).resolve_type_ids();
            }

            {
                indent _(tr);
                ++tr << type_name(cr.type) << ": "
                     << range{cr.first_base, cr.last_base} << "\n";
            }

            auto& rtc = class_map[rtti::type_index(cr.type)];

            if (rtc == nullptr) {
                rtc = &classes.emplace_back();
                rtc->is_abstract = cr.is_abstract;
                rtc->static_vptr = cr.static_vptr;
            }

            if (std::find(
                    rtc->type_ids.begin(), rtc->type_ids.end(), cr.type) ==
                rtc->type_ids.end()) {
                rtc->type_ids.push_back(cr.type);
            }
        }
    }

    // All known classes now have exactly one associated class_* in the
    // map. Collect the bases.

    for (auto& cr : registry::classes) {
        auto rtc = class_map[rtti::type_index(cr.type)];

        for (auto& base : range{cr.first_base, cr.last_base}) {
            auto rtb = class_map[rtti::type_index(base)];

            if (!rtb) {
                missing_class error;
                error.type = base;

                if constexpr (has_error_handler) {
                    error_handler::error(error);
                }

                abort();
            }

            if (rtc != rtb) {
                // At compile time we collected the class as its own
                // improper base, as per std::is_base_of. Eliminate that.
                ++class_mark;
                collect_transitive_bases(rtc, rtb);
            }
        }
    }

    // At this point bases may contain duplicates, and also indirect
    // bases. Clean that up.

    std::size_t mark = ++class_mark;

    for (auto& rtc : classes) {
        decltype(rtc.transitive_bases) bases;
        mark = ++class_mark;

        for (auto rtb : rtc.transitive_bases) {
            if (rtb->mark != mark) {
                bases.push_back(rtb);
                rtb->mark = mark;
            }
        }

        rtc.transitive_bases.swap(bases);
    }

    for (auto& rtc : classes) {
        // Sort base classes by number of transitive bases. This ensures that a
        // base class is never preceded by one if its own base classes.
        std::sort(
            rtc.transitive_bases.begin(), rtc.transitive_bases.end(),
            [](auto a, auto b) {
                return a->transitive_bases.size() > b->transitive_bases.size();
            });
        mark = ++class_mark;

        // Collect the direct base classes. The first base is certainly a
        // direct one. Remove *its* bases from the candidates, by marking
        // them. Continue with the next base that is not marked. It is the
        // next direct base. And so on...

        for (auto rtb : rtc.transitive_bases) {
            if (rtb->mark == mark) {
                continue;
            }

            rtc.direct_bases.push_back(rtb);

            for (auto rtbb : rtb->transitive_bases) {
                rtbb->mark = mark;
            }
        }
    }

    for (auto& rtc : classes) {
        for (auto rtb : rtc.direct_bases) {
            rtb->direct_derived.push_back(&rtc);
        }
    }

    for (auto& rtc : classes) {
        calculate_transitive_derived(rtc);
    }

    if constexpr (has_trace) {
        ++tr << "Inheritance lattice:\n";

        for (auto& rtc : classes) {
            indent _2(tr);
            ++tr << rtc << "\n";

            {
                indent _3(tr);
                ++tr << "bases:      " << rtc.direct_bases << "\n";
                ++tr << "derived:    " << rtc.direct_derived << "\n";
                ++tr << "covariant:  " << rtc.transitive_derived << "\n";
            }
        }
    }
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::calculate_transitive_derived(
    class_& cls) {
    if (!cls.transitive_derived.empty()) {
        return;
    }

    cls.transitive_derived.insert(&cls);

    for (auto derived : cls.direct_derived) {
        if (derived->transitive_derived.empty()) {
            calculate_transitive_derived(*derived);
        }

        std::copy(
            derived->transitive_derived.begin(),
            derived->transitive_derived.end(),
            std::inserter(
                cls.transitive_derived, cls.transitive_derived.end()));
    }
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::augment_methods() {
    using namespace policies;
    using namespace detail;

    methods.resize(registry::methods.size());

    ++tr << "Methods:\n";
    indent _(tr);

    auto meth_iter = methods.begin();

    for (auto& meth_info : registry::methods) {
        if constexpr (has_deferred_static_rtti) {
            static_cast<deferred_method_info&>(meth_info).resolve_type_ids();
        }

        ++tr << type_name(meth_info.method_type_id) << " "
             << range{meth_info.vp_begin, meth_info.vp_end} << "\n";

        indent _(tr);

        meth_iter->info = &meth_info;
        meth_iter->vp.reserve(meth_info.arity());
        meth_iter->slots.resize(meth_info.arity());

        {
            std::size_t param_index = 0;

            for (auto ti : range{meth_info.vp_begin, meth_info.vp_end}) {
                auto class_ = class_map[rtti::type_index(ti)];
                if (!class_) {
                    ++tr << "unknown class " << ti << "(" << type_name(ti)
                         << ") for parameter #" << (param_index + 1) << "\n";
                    missing_class error;
                    error.type = ti;

                    if constexpr (has_error_handler) {
                        error_handler::error(error);
                    }

                    abort();
                }

                meth_iter->vp.push_back(class_);
            }
        }

        if (rtti::type_index(meth_info.return_type_id) !=
            rtti::type_index(rtti::template static_type<void>())) {
            auto covariant_return_iter =
                class_map.find(rtti::type_index(meth_info.return_type_id));

            if (covariant_return_iter != class_map.end()) {
                meth_iter->covariant_return_type =
                    covariant_return_iter->second;
            }
        }

        // initialize the function pointer in the synthetic not_implemented
        // overrider
        const auto method_index = meth_iter - methods.begin();
        auto spec_size = meth_info.overriders.size();
        meth_iter->not_implemented.pf = meth_iter->info->not_implemented;
        meth_iter->not_implemented.method_index = method_index;
        meth_iter->not_implemented.spec_index = spec_size;
        meth_iter->ambiguous.pf = meth_iter->info->ambiguous;
        meth_iter->ambiguous.method_index = method_index;
        meth_iter->ambiguous.spec_index = spec_size + 1;

        meth_iter->overriders.resize(spec_size);
        auto spec_iter = meth_iter->overriders.begin();

        for (auto& overrider_info : meth_info.overriders) {
            if constexpr (has_deferred_static_rtti) {
                static_cast<deferred_overrider_info&>(overrider_info)
                    .resolve_type_ids();
            }

            spec_iter->method_index = method_index;
            spec_iter->spec_index = spec_iter - meth_iter->overriders.begin();

            ++tr << type_name(overrider_info.type) << " (" << overrider_info.pf
                 << ")\n";
            spec_iter->info = &overrider_info;
            spec_iter->vp.reserve(meth_info.arity());
            std::size_t param_index = 0;

            for (auto type :
                 range{overrider_info.vp_begin, overrider_info.vp_end}) {
                indent _(tr);
                auto class_ = class_map[rtti::type_index(type)];

                if (!class_) {
                    ++tr << "unknown class error for *virtual* parameter #"
                         << (param_index + 1) << "\n";
                    missing_class error;
                    error.type = type;

                    if constexpr (has_error_handler) {
                        error_handler::error(error);
                    }

                    abort();
                }

                spec_iter->pf = spec_iter->info->pf;
                spec_iter->vp.push_back(class_);
            }

            if (meth_iter->covariant_return_type) {
                auto covariant_return_iter = class_map.find(
                    rtti::type_index(overrider_info.return_type));

                if (covariant_return_iter != class_map.end()) {
                    spec_iter->covariant_return_type =
                        covariant_return_iter->second;
                } else {
                    missing_class error;
                    error.type = overrider_info.return_type;

                    if constexpr (has_error_handler) {
                        error_handler::error(error);
                    }

                    abort();
                }
            }

            ++spec_iter;
        }

        ++meth_iter;
    }

    for (auto& method : methods) {
        std::size_t param_index = 0;

        for (auto vp : method.vp) {
            for (auto& overrider : method.overriders) {
                if (overrider.vp[param_index] == vp) {
                    continue;
                }

                if (!vp->is_base_of(overrider.vp[param_index])) {
                    missing_base error;
                    error.base = overrider.vp[param_index]->type_ids[0];
                    error.derived = vp->type_ids[0];

                    if constexpr (has_error_handler) {
                        error_handler::error(error);
                    }

                    abort();
                }
            }

            vp->used_by_vp.push_back({&method, param_index++});
        }
    }
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::assign_slots() {
    ++tr << "Allocating slots...\n";

    {
        indent _(tr);

        ++class_mark;

        for (auto& cls : classes) {
            if (cls.direct_bases.size() == 0) {
                if (std::find_if(
                        cls.transitive_derived.begin(),
                        cls.transitive_derived.end(), [](auto cls) {
                            return cls->direct_bases.size() > 1;
                        }) == cls.transitive_derived.end()) {
                    indent _(tr);
                    assign_tree_slots(cls, 0);
                } else {
                    assign_lattice_slots(cls);
                }
            }
        }
    }

    ++tr << "Allocating MI v-tables...\n";

    {
        indent _(tr);

        for (auto& cls : classes) {
            if (cls.used_slots.empty()) {
                // not involved in multiple inheritance
                continue;
            }

            auto first_slot = cls.used_slots.find_first();
            cls.first_slot =
                first_slot == boost::dynamic_bitset<>::npos ? 0u : first_slot;
            cls.vtbl.resize(cls.used_slots.size() - cls.first_slot);
            ++tr << cls << " vtbl: " << cls.first_slot << "-"
                 << cls.used_slots.size() << " slots " << cls.used_slots
                 << "\n";
        }
    }
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::assign_tree_slots(
    class_& cls, std::size_t base_slot) {
    auto next_slot = base_slot;
    using namespace detail;

    for (const auto& mp : cls.used_by_vp) {
        ++tr << " in " << cls << " for "
             << type_name(mp.method->info->method_type_id) << " parameter "
             << mp.param << ": " << next_slot << "\n";
        mp.method->slots[mp.param] = next_slot++;
    }

    cls.first_slot = 0;
    cls.vtbl.resize(next_slot);

    for (auto pd : cls.direct_derived) {
        assign_tree_slots(*pd, next_slot);
    }
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::assign_lattice_slots(
    class_& cls) {
    using namespace detail;

    if (cls.mark == class_mark) {
        return;
    }

    cls.mark = class_mark;

    if (!cls.used_by_vp.empty()) {
        for (const auto& mp : cls.used_by_vp) {
            ++tr << " in " << cls << " for "
                 << type_name(mp.method->info->method_type_id) << " parameter "
                 << mp.param << "\n";

            indent _(tr);

            ++tr << "reserved slots: " << cls.reserved_slots
                 << " used slots: " << cls.used_slots << "\n";

            auto unavailable_slots = cls.used_slots;
            detail::merge_into(cls.reserved_slots, unavailable_slots);

            ++tr << "unavailable slots: " << unavailable_slots << "\n";

            std::size_t slot = 0;

            for (; slot < unavailable_slots.size(); ++slot) {
                if (!unavailable_slots[slot]) {
                    break;
                }
            }

            ++tr << "first available slot: " << slot << "\n";

            mp.method->slots[mp.param] = slot;
            detail::set_bit(cls.used_slots, slot);
            detail::set_bit(cls.reserved_slots, slot);

            {
                ++tr << "reserve slots " << cls.used_slots << " in:\n";
                indent _(tr);

                for (auto base : cls.transitive_bases) {
                    ++tr << *base << "\n";
                    detail::merge_into(cls.used_slots, base->reserved_slots);
                }
            }

            {
                ++tr << "assign slots " << cls.used_slots << " in:\n";
                indent _(tr);

                for (auto covariant : cls.transitive_derived) {
                    if (&cls != covariant) {
                        ++tr << *covariant << "\n";
                        detail::merge_into(
                            cls.used_slots, covariant->used_slots);

                        for (auto base : covariant->transitive_bases) {
                            ++tr << *base << "\n";
                            detail::merge_into(
                                cls.used_slots, base->reserved_slots);
                        }
                    }
                }
            }
        }
    }

    for (auto pd : cls.direct_derived) {
        assign_lattice_slots(*pd);
    }
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::build_dispatch_tables() {
    using namespace detail;

    for (auto& m : methods) {
        ++tr << "Building dispatch table for "
             << type_name(m.info->method_type_id) << "\n";
        indent _(tr);

        auto dims = m.arity();

        std::vector<group_map> groups;
        groups.resize(dims);

        {
            std::size_t dim = 0;

            for (auto vp : m.vp) {
                auto& dim_group = groups[dim];
                ++tr << "make groups for param #" << dim << ", class " << *vp
                     << "\n";
                indent _(tr);

                for (auto covariant_class : vp->transitive_derived) {
                    ++tr << "overriders applicable to " << *covariant_class
                         << "\n";
                    bitvec mask;
                    mask.resize(m.overriders.size());

                    std::size_t group_index = 0;
                    indent _2(tr);

                    for (auto& spec : m.overriders) {
                        if (spec.vp[dim]->transitive_derived.find(
                                covariant_class) !=
                            spec.vp[dim]->transitive_derived.end()) {
                            ++tr << type_name(spec.info->type) << "\n";
                            mask[group_index] = 1;
                        }
                        ++group_index;
                    }

                    auto& group = dim_group[mask];
                    group.classes.push_back(covariant_class);
                    group.has_concrete_classes = group.has_concrete_classes ||
                        !covariant_class->is_abstract;

                    ++tr << "-> mask: " << mask << "\n";
                }

                ++dim;
            }
        }

        {
            std::size_t stride = 1;
            m.strides.reserve(dims - 1);

            for (std::size_t dim = 1; dim < m.arity(); ++dim) {
                stride *= groups[dim - 1].size();
                ++tr << "    stride for dim " << dim << " = " << stride << "\n";
                m.strides.push_back(stride);
            }
        }

        for (std::size_t dim = 0; dim < m.arity(); ++dim) {
            indent _(tr);
            std::size_t group_num = 0;

            for (auto& [mask, group] : groups[dim]) {
                ++tr << "groups for dim " << dim << ":\n";
                indent _(tr);
                ++tr << group_num << " mask " << mask << ":\n";

                for (auto cls : group.classes) {
                    indent _(tr);
                    ++tr << type_name(cls->type_ids[0]) << "\n";
                    auto& entry = cls->vtbl[m.slots[dim] - cls->first_slot];
                    entry.method_index = &m - &methods[0];
                    entry.vp_index = dim;
                    entry.group_index = group_num;
                }

                ++group_num;
            }
        }

        {
            ++tr << "building dispatch table\n";
            bitvec all(m.overriders.size());
            all = ~all;
            build_dispatch_table(m, dims - 1, groups.end() - 1, all, true);

            if (m.arity() > 1) {
                indent _(tr);
                m.report.cells = 1;
                ++tr << "dispatch table rank: ";
                const char* prefix = "";

                for (const auto& dim_groups : groups) {
                    m.report.cells *= dim_groups.size();
                    tr << prefix << dim_groups.size();
                    prefix = " x ";
                }

                prefix = ", concrete only: ";

                for (const auto& dim_groups : groups) {
                    auto cells = std::count_if(
                        dim_groups.begin(), dim_groups.end(),
                        [](const auto& group) {
                            return group.second.has_concrete_classes;
                        });
                    tr << prefix << cells;
                    prefix = " x ";
                }

                tr << "\n";
            }

            print(m.report);
            accumulate(m.report, report);
        }
    }
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::build_dispatch_table(
    method& m, std::size_t dim,
    std::vector<group_map>::const_iterator group_iter, const bitvec& candidates,
    bool concrete) {
    using namespace detail;

    indent _(tr);
    std::size_t group_index = 0;

    for (const auto& [group_mask, group] : *group_iter) {
        auto mask = candidates & group_mask;

        if constexpr (has_trace) {
            ++tr << "group " << dim << "/" << group_index << " mask " << mask
                 << "\n";
            indent _(tr);
            for (auto cls : range{group.classes.begin(), group.classes.end()}) {
                ++tr << type_name(cls->type_ids[0]) << "\n";
            }
        }

        if (dim == 0) {
            std::vector<overrider*> overriders;
            std::size_t i = 0;

            for (auto& spec : m.overriders) {
                if (mask[i]) {
                    overriders.push_back(&spec);
                }
                ++i;
            }

            if constexpr (has_trace) {
                ++tr << "select best of:\n";
                indent _(tr);

                for (auto& app : overriders) {
                    ++tr << "#" << app->spec_index << " "
                         << type_name(app->info->type) << "\n";
                }
            }

            std::vector<overrider*> dominants = overriders;
            std::size_t pick, remaining;

            select_dominant_overriders(dominants, pick, remaining);

            if (remaining == 0) {
                indent _(tr);
                ++tr << "not implemented\n";
                m.dispatch_table.push_back(&m.not_implemented);
                ++m.report.not_implemented;
            } else {
                if constexpr (!has_option<n2216>) {
                    if (remaining > 1) {
                        ++tr << "ambiguous\n";
                        m.dispatch_table.push_back(&m.ambiguous);
                        ++m.report.ambiguous;
                        continue;
                    }
                }

                auto overrider = dominants[pick];
                m.dispatch_table.push_back(overrider);
                ++tr;

                tr << "-> #" << overrider->spec_index << " "
                   << type_name(overrider->info->type)
                   << " pf = " << overrider->info->pf;

                if (remaining > 1) {
                    tr << " (ambiguous)";
                    ++m.report.ambiguous;
                }

                tr << "\n";

                // -------------------------------------------------------------
                // next

                // First remove the dominant overriders from the overriders.
                // Note that the dominants appear in the overriders in the same
                // relative order.
                auto candidate = overriders.begin();
                remaining = 0;

                for (auto dominant : dominants) {
                    if (*candidate == dominant) {
                        *candidate = nullptr;
                    } else {
                        ++remaining;
                    }

                    ++candidate;
                }

                if (remaining == 0) {
                    ++tr << "no 'next'\n";
                    overrider->next = &m.not_implemented;
                } else {
                    if constexpr (has_trace) {
                        ++tr << "for 'next', select best of:\n";
                        indent _(tr);

                        for (auto& app : overriders) {
                            if (app) {
                                ++tr << "#" << app->spec_index << " "
                                     << type_name(app->info->type) << "\n";
                            }
                        }
                    }

                    select_dominant_overriders(overriders, pick, remaining);

                    if constexpr (!has_option<n2216>) {
                        if (remaining > 1) {
                            ++tr << "ambiguous 'next'\n";
                            overrider->next = &m.ambiguous;
                            continue;
                        }
                    }

                    auto next_overrider = overriders[pick];
                    overrider->next = next_overrider;

                    ++tr << "-> #" << next_overrider->spec_index << " "
                         << type_name(next_overrider->info->type)
                         << " pf = " << next_overrider->info->pf;

                    if (remaining > 1) {
                        tr << " (ambiguous)";
                        // do not increment m.report.ambiguous, for same reason
                    }

                    tr << "\n";
                }
            }
        } else {
            build_dispatch_table(
                m, dim - 1, group_iter - 1, mask,
                concrete && group.has_concrete_classes);
        }

        ++group_index;
    }
}

inline void detail::generic_compiler::accumulate(
    const method_report& partial, report& total) {
    total.cells += partial.cells;
    total.not_implemented += partial.not_implemented != 0;
    total.ambiguous += partial.ambiguous != 0;
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::write_global_data() {
    using namespace policies;
    using namespace detail;

    auto dispatch_data_size = std::accumulate(
        methods.begin(), methods.end(), std::size_t(0),
        [](std::size_t sum, const method& m) {
            // msvc doesn't like (auto sum, auto& m) (C2187), go figure...
            return sum + m.dispatch_table.size();
        });
    dispatch_data_size = std::accumulate(
        classes.begin(), classes.end(), dispatch_data_size,
        [](auto sum, const auto& cls) { return sum + cls.vtbl.size(); });

    std::vector<detail::word> new_dispatch_data(dispatch_data_size);
    auto gv_first = new_dispatch_data.data();
    [[maybe_unused]] auto gv_last = gv_first + dispatch_data_size;
    auto gv_iter = gv_first;

    ++tr << "Initializing multi-method dispatch tables at " << gv_iter << "\n";

    for (auto& m : methods) {
        if (m.info->arity() == 1) {
            // Uni-methods just need an index in the method table.
            m.info->slots_strides_ptr[0] = m.slots[0];
        } else {
            auto strides_iter = std::copy(
                m.slots.begin(), m.slots.end(), m.info->slots_strides_ptr);
            std::copy(m.strides.begin(), m.strides.end(), strides_iter);

            if constexpr (has_trace) {
                ++tr << rflush(4, dispatch_data_size) << " " << " method #"
                     << m.dispatch_table[0]->method_index << " "
                     << type_name(m.info->method_type_id) << "\n";
                indent _(tr);

                for (auto& entry : m.dispatch_table) {
                    ++tr << "spec #" << entry->spec_index << " "
                         << spec_name(m, entry) << "\n";
                }
            }

            m.gv_dispatch_table = gv_iter;
            BOOST_ASSERT(gv_iter + m.dispatch_table.size() <= gv_last);
            gv_iter = std::transform(
                m.dispatch_table.begin(), m.dispatch_table.end(), gv_iter,
                [](auto spec) { return spec->pf; });
        }
    }

    ++tr << "Setting 'next' pointers\n";

    for (auto& m : methods) {
        indent _(tr);
        ++tr << "method #" << " " << type_name(m.info->method_type_id) << "\n";

        for (auto& overrider : m.overriders) {
            if (overrider.next) {
                ++tr << "#" << overrider.spec_index << " "
                     << spec_name(m, &overrider) << " -> ";

                tr << "#" << overrider.next->spec_index << " "
                   << spec_name(m, overrider.next);
                *overrider.info->next =
                    reinterpret_cast<void (*)()>(overrider.next->pf);
            } else {
                tr << "none";
            }

            tr << "\n";
        }
    }

    ++tr << "Initializing v-tables at " << gv_iter << "\n";

    for (auto& cls : classes) {
        *cls.static_vptr = gv_iter - cls.first_slot;

        ++tr << rflush(4, gv_iter - gv_first) << " " << gv_iter << " vtbl for "
             << cls << " slots " << cls.first_slot << "-"
             << (cls.first_slot + cls.vtbl.size() - 1) << "\n";
        indent _(tr);

        for (auto& entry : cls.vtbl) {
            ++tr << "method #" << entry.method_index << " ";
            auto& method = methods[entry.method_index];

            if (method.arity() == 1) {
                auto spec = method.dispatch_table[entry.group_index];
                tr << "spec #" << spec->spec_index << "\n";
                indent _(tr);
                ++tr << type_name(method.info->method_type_id) << "\n";
                ++tr << spec_name(method, spec);
                BOOST_ASSERT(gv_iter + 1 <= gv_last);
                *gv_iter++ = spec->pf;
            } else {
                tr << "vp #" << entry.vp_index << " group #"
                   << entry.group_index << "\n";
                indent _2(tr);
                ++tr << type_name(method.info->method_type_id);
                BOOST_ASSERT(gv_iter + 1 <= gv_last);

                if (entry.vp_index == 0) {
                    *gv_iter++ = std::uintptr_t(
                        method.gv_dispatch_table + entry.group_index);
                } else {
                    *gv_iter++ = entry.group_index;
                }
            }

            tr << "\n";
        }
    }

    ++tr << rflush(4, dispatch_data_size) << " " << gv_iter << " end\n";

    if constexpr (has_vptr) {
        vptr::initialize(*this, options);
    }

    new_dispatch_data.swap(dispatch_data);
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::select_dominant_overriders(
    std::vector<overrider*>& candidates, std::size_t& pick,
    std::size_t& remaining) {

    pick = 0;
    remaining = 0;

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i]) {
            for (size_t j = i + 1; j < candidates.size(); ++j) {
                if (candidates[j]) {
                    if (is_more_specific(candidates[i], candidates[j])) {
                        candidates[j] = nullptr;
                    } else if (is_more_specific(candidates[j], candidates[i])) {
                        candidates[i] = nullptr;
                        break; // this one is dead
                    }
                }
            }
        }

        if (candidates[i]) {
            pick = i;
            ++remaining;
        }
    }

    if (remaining <= 1) {
        return;
    }

    if constexpr (has_option<n2216>) {
        if (!candidates[pick]->covariant_return_type) {
            return;
        }

        remaining = 0;

        for (size_t i = 0; i < candidates.size(); ++i) {
            if (candidates[i]) {
                for (size_t j = i + 1; j < candidates.size(); ++j) {
                    if (candidates[j]) {
                        BOOST_ASSERT(candidates[i] != candidates[j]);

                        if (candidates[i]->covariant_return_type->is_base_of(
                                candidates[j]->covariant_return_type)) {
                            candidates[i] = nullptr;
                        } else if (candidates[j]
                                       ->covariant_return_type->is_base_of(
                                           candidates[i]
                                               ->covariant_return_type)) {
                            candidates[j] = nullptr;
                        }
                    }
                }
            }

            if (candidates[i]) {
                pick = i;
                ++remaining;
            }
        }
    }
}

template<class... Policies>
template<class... Options>
auto registry<Policies...>::compiler<Options...>::is_more_specific(
    const overrider* a, const overrider* b) -> bool {
    bool result = false;

    auto a_iter = a->vp.begin(), a_last = a->vp.end(), b_iter = b->vp.begin();

    for (; a_iter != a_last; ++a_iter, ++b_iter) {
        if (*a_iter != *b_iter) {
            if ((*b_iter)->transitive_derived.find(*a_iter) !=
                (*b_iter)->transitive_derived.end()) {
                result = true;
            } else if (
                (*a_iter)->transitive_derived.find(*b_iter) !=
                (*a_iter)->transitive_derived.end()) {
                return false;
            }
        }
    }

    return result;
}

template<class... Policies>
template<class... Options>
auto registry<Policies...>::compiler<Options...>::is_base(
    const overrider* a, const overrider* b) -> bool {
    bool result = false;

    auto a_iter = a->vp.begin(), a_last = a->vp.end(), b_iter = b->vp.begin();

    for (; a_iter != a_last; ++a_iter, ++b_iter) {
        if (*a_iter != *b_iter) {
            if ((*a_iter)->transitive_derived.find(*b_iter) ==
                (*a_iter)->transitive_derived.end()) {
                return false;
            } else {
                result = true;
            }
        }
    }

    return result;
}

template<class... Policies>
template<class... Options>
void registry<Policies...>::compiler<Options...>::print(
    const method_report& r) const {
    ++tr;

    if (r.cells) {
        // only for multi-methods, uni-methods don't have dispatch tables
        ++tr << r.cells << " dispatch table cells, ";
    }

    tr << r.not_implemented << " not implemented, " << r.ambiguous
       << " ambiguous\n";
}

//! Initialize a registry.
//!
//! Initialize the @ref registry passed as an explicit function template
//! argument, or @ref default_registry if the registry is not specified. The
//! default can be changed by defining {{BOOST_OPENMETHOD_DEFAULT_REGISTRY}}.
//! Option objects can be passed to change the behavior of the function.
//! Currently two options exist:
//! @li @ref trace Enable tracing of the initialization process.
//! @li @ref n2216 Enable resolution of ambiguities according to the N2216
//! paper.
//!
//! `initialize` must be called, typically at the beginning of `main`, before
//! using any of the methods in a registry. It sets up the v-tables,
//! multi-method dispatch tables, and any other data required by the policies.
//!
//! The function returns an object of an unspecified type that contains a
//! `report` member, itself an object of an unspecified type, thatcontains the
//! following members:
//! @li `std::size_t cells`: The number of cells in all multi-method dispatch
//! tables.
//! @li `std::size_t not_implemented`: The number of multi-method dispatch tables that
//! contain at least one not implemented entry.
//! @li `std::size_t ambiguous`: The number of multi-method dispatch tables that contain at
//! least one ambiguous entry.
//!
//! @note
//! A translation unit that calls `initialize` must include the
//! `<boost/openmethod/initialize.hpp>` header.
//!
//! @tparam Registry The registry to initialize.
//! @tparam Options... Zero or more option types, deduced from the function
//! arguments.
//! @param options Zero or more option objects.
//! @return An object of an unspecified type.
//!
//! @par Errors
//!
//! @li @ref missing_class: A class used in a virtual parameter was not
//! registered.
//! @li The registry's policies may report additional errors.
//!
//! @par Example
//!
//! Initialize the default registry with tracing enabled, and exit with an error
//! message if there were any possibility of a @reg bad_call error. User may run
//! the program again after setting environment variable
//! `BOOST_OPENMETHOD_TRACE` to `1` to troubleshoot.
//!
//! @code
//! #include <iostream>
//!
//! #include <boost/openmethod.hpp>
//! #include <boost/openmethod/initialize.hpp>
//!
//! int main() {
//!     namespace bom = boost::openmethod;
//!     auto report = bom::initialize(bom::trace::from_env()).report;
//!
//!     if (report.not_implemented != 0 || report.ambiguous != 0) {
//!         std::cerr << "missing overriders or ambiguous methods\n";
//!         return 1;
//!     }
//!
//!     // ...
//! }
//! @endcode
template<class Registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY, class... Options>
inline auto initialize(Options&&... options) {
    if (detail::odr_check<Registry>::count > 1) {
        // Multiple definitions of default_registry detected.
        // This indicates an ODR violation.
        // Signal a final_error using the error handler, then abort.
        if constexpr (Registry::has_error_handler) {
            Registry::error_handler::error(odr_violation());
        }

        std::abort();
    }

    typename Registry::template compiler<Options...> comp(
        std::forward<Options>(options)...);
    comp.initialize();

    return comp;
}

namespace detail {

template<typename, class Policy, class... Options>
struct has_finalize_aux : std::false_type {};

template<class Policy, class... Options>
struct has_finalize_aux<
    std::void_t<decltype(Policy::finalize(
        std::declval<std::tuple<Options...>>()))>,
    Policy, Options...> : std::true_type {};

} // namespace detail

template<class... Policies>
template<class... Options>
auto registry<Policies...>::finalize(Options... opts) -> void {
    std::tuple<Options...> options(opts...); // gcc-8 doesn't like CTAD here
    mp11::mp_for_each<policy_list>([&options](auto policy) {
        using fn = typename decltype(policy)::template fn<registry>;
        if constexpr (detail::has_finalize_aux<void, fn, Options...>::value) {
            fn::finalize(options);
        }
    });

    dispatch_data.clear();
    initialized = false;
}

//! Release resources held by registry.
//!
//! `finalize` may be called to release any resources allocated by
//! @ref registry::initialize.
//!
//! @note
//! A translation unit that contains a call to `finalize` must include the
//! `<boost/openmethod/initialize.hpp>` header.
//!
//! @tparam Registry The registry to finalize.
//! @tparam Options... Zero or more option types, deduced from the function
//! arguments.
//! @param options Zero or more option objects.
template<class Registry = BOOST_OPENMETHOD_DEFAULT_REGISTRY, class... Options>
inline auto finalize(Options&&... opts) -> void {
    Registry::finalize(std::forward<Options>(opts)...);
}

} // namespace boost::openmethod

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
