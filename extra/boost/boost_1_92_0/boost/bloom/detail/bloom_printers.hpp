// Copyright 2025 Braden Ganetsky
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Generated on 2025-06-29T10:15:10

#ifndef BOOST_BLOOM_DETAIL_BLOOM_PRINTERS_HPP
#define BOOST_BLOOM_DETAIL_BLOOM_PRINTERS_HPP

#ifndef BOOST_ALL_NO_EMBEDDED_GDB_SCRIPTS
#if defined(__ELF__)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverlength-strings"
#endif
__asm__(".pushsection \".debug_gdb_scripts\", \"MS\",%progbits,1\n"
        ".ascii \"\\4gdb.inlined-script.BOOST_BLOOM_DETAIL_BLOOM_PRINTERS_HPP\\n\"\n"
        ".ascii \"import gdb.printing\\n\"\n"
        ".ascii \"import gdb.xmethod\\n\"\n"

        ".ascii \"class BoostBloomFilterPrinter:\\n\"\n"
        ".ascii \"    def __init__(self, val):\\n\"\n"
        ".ascii \"        self.void_pointer = gdb.lookup_type(\\\"void\\\").pointer()\\n\"\n"
        ".ascii \"        nullptr = gdb.Value(0).cast(self.void_pointer)\\n\"\n"

        ".ascii \"        has_array = val[\\\"ar\\\"][\\\"data\\\"] != nullptr\\n\"\n"

        ".ascii \"        if has_array:\\n\"\n"
        ".ascii \"            stride = int(val[\\\"stride\\\"])\\n\"\n"
        ".ascii \"            used_value_size = int(val[\\\"used_value_size\\\"])\\n\"\n"
        ".ascii \"            self.array_size = int(val[\\\"hs\\\"][\\\"rng\\\"]) * stride + (used_value_size - stride)\\n\"\n"
        ".ascii \"        else:\\n\"\n"
        ".ascii \"            self.array_size = 0\\n\"\n"
        ".ascii \"        self.capacity = self.array_size * 8\\n\"\n"
        ".ascii \"        if has_array:\\n\"\n"
        ".ascii \"            self.data = val[\\\"ar\\\"][\\\"array\\\"]\\n\"\n"
        ".ascii \"        else:\\n\"\n"
        ".ascii \"            self.data = nullptr\\n\"\n"

        ".ascii \"    def to_string(self):\\n\"\n"
        ".ascii \"        return f\\\"boost::bloom::filter with {{capacity = {self.capacity}, data = {self.data.cast(self.void_pointer)}, size = {self.array_size}}}\\\"\\n\"\n"

        ".ascii \"    def display_hint(self):\\n\"\n"
        ".ascii \"        return \\\"map\\\"\\n\"\n"
        ".ascii \"    def children(self):\\n\"\n"
        ".ascii \"        def generator():\\n\"\n"
        ".ascii \"            data = self.data\\n\"\n"
        ".ascii \"            for i in range(self.array_size):\\n\"\n"
        ".ascii \"                yield \\\"\\\", f\\\"{i}\\\"\\n\"\n"
        ".ascii \"                yield \\\"\\\", data.dereference()\\n\"\n"
        ".ascii \"                data = data + 1\\n\"\n"
        ".ascii \"        return generator()\\n\"\n"

        ".ascii \"def boost_bloom_build_pretty_printer():\\n\"\n"
        ".ascii \"    pp = gdb.printing.RegexpCollectionPrettyPrinter(\\\"boost_bloom\\\")\\n\"\n"
        ".ascii \"    add_template_printer = lambda name, printer: pp.add_printer(name, f\\\"^{name}<.*>$\\\", printer)\\n\"\n"

        ".ascii \"    add_template_printer(\\\"boost::bloom::filter\\\", BoostBloomFilterPrinter)\\n\"\n"

        ".ascii \"    return pp\\n\"\n"

        ".ascii \"gdb.printing.register_pretty_printer(gdb.current_objfile(), boost_bloom_build_pretty_printer())\\n\"\n"

        ".ascii \"# https://sourceware.org/gdb/current/onlinedocs/gdb.html/Writing-an-Xmethod.html\\n\"\n"
        ".ascii \"class BoostBloomFilterSubscriptMethod(gdb.xmethod.XMethod):\\n\"\n"
        ".ascii \"    def __init__(self):\\n\"\n"
        ".ascii \"        gdb.xmethod.XMethod.__init__(self, 'subscript')\\n\"\n"

        ".ascii \"    def get_worker(self, method_name):\\n\"\n"
        ".ascii \"        if method_name == 'operator[]':\\n\"\n"
        ".ascii \"            return BoostBloomFilterSubscriptWorker()\\n\"\n"

        ".ascii \"class BoostBloomFilterSubscriptWorker(gdb.xmethod.XMethodWorker):\\n\"\n"
        ".ascii \"    def get_arg_types(self):\\n\"\n"
        ".ascii \"        return [gdb.lookup_type('std::size_t')]\\n\"\n"

        ".ascii \"    def get_result_type(self, obj):\\n\"\n"
        ".ascii \"        return gdb.lookup_type('unsigned char')\\n\"\n"

        ".ascii \"    def __call__(self, obj, index):\\n\"\n"
        ".ascii \"        fp = BoostBloomFilterPrinter(obj)\\n\"\n"
        ".ascii \"        if fp.array_size == 0:\\n\"\n"
        ".ascii \"            print('Error: Filter is null')\\n\"\n"
        ".ascii \"            return\\n\"\n"
        ".ascii \"        elif index < 0 or index >= fp.array_size:\\n\"\n"
        ".ascii \"            print('Error: Out of bounds')\\n\"\n"
        ".ascii \"            return\\n\"\n"
        ".ascii \"        else:\\n\"\n"
        ".ascii \"            data = fp.data\\n\"\n"
        ".ascii \"            return (data + index).dereference()\\n\"\n"

        ".ascii \"class BoostBloomFilterMatcher(gdb.xmethod.XMethodMatcher):\\n\"\n"
        ".ascii \"    def __init__(self):\\n\"\n"
        ".ascii \"        gdb.xmethod.XMethodMatcher.__init__(self, 'BoostBloomFilterMatcher')\\n\"\n"
        ".ascii \"        self.methods = [BoostBloomFilterSubscriptMethod()]\\n\"\n"

        ".ascii \"    def match(self, class_type, method_name):\\n\"\n"
        ".ascii \"        if not class_type.tag.startswith('boost::bloom::filter<'):\\n\"\n"
        ".ascii \"            return None\\n\"\n"

        ".ascii \"        workers = []\\n\"\n"
        ".ascii \"        for method in self.methods:\\n\"\n"
        ".ascii \"            if method.enabled:\\n\"\n"
        ".ascii \"                worker = method.get_worker(method_name)\\n\"\n"
        ".ascii \"                if worker:\\n\"\n"
        ".ascii \"                    workers.append(worker)\\n\"\n"
        ".ascii \"        return workers\\n\"\n"

        ".ascii \"gdb.xmethod.register_xmethod_matcher(None, BoostBloomFilterMatcher())\\n\"\n"

        ".byte 0\n"
        ".popsection\n");
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif // defined(__ELF__)
#endif // !defined(BOOST_ALL_NO_EMBEDDED_GDB_SCRIPTS)

#endif // !defined(BOOST_BLOOM_DETAIL_BLOOM_PRINTERS_HPP)
