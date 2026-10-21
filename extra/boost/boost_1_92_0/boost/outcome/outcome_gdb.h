// Inline GDB pretty printer for result
// (C) 2024 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
// File Created: Jun 2024
//
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License in the accompanying file
// Licence.txt or at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//
// Distributed under the Boost Software License, Version 1.0.
//     (See accompanying file Licence.txt or copy at
//           http://www.boost.org/LICENSE_1_0.txt)

// Generated on 2024-08-12T21:44:07

#ifndef BOOST_OUTCOME_INLINE_GDB_PRETTY_PRINTER_H
#define BOOST_OUTCOME_INLINE_GDB_PRETTY_PRINTER_H

#ifndef BOOST_OUTCOME_DISABLE_INLINE_GDB_PRETTY_PRINTERS
#if defined(__ELF__)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverlength-strings"
#endif
__asm__(".pushsection \".debug_gdb_scripts\", \"MS\",%progbits,1\n"
        ".ascii \"\\4gdb.inlined-script.BOOST_OUTCOME_INLINE_GDB_PRETTY_PRINTER_H\\n\"\n"
        ".ascii \"import gdb.printing\\n\"\n"
        ".ascii \"import os\\n\"\n"

        ".ascii \"def synthesise_gdb_value_from_string(s):\\n\"\n"
        ".ascii \"    '''For when you want to return a synthetic string from children()'''\\n\"\n"
        ".ascii \"    return gdb.Value(s + '\\\\0').cast(gdb.lookup_type('char').pointer())\\n\"\n"

        ".ascii \"class OutcomeBasicOutcomePrinter(object):\\n\"\n"
        ".ascii \"    '''Print an outcome::basic_outcome<T> and outcome::basic_result<T>'''\\n\"\n"

        ".ascii \"    def __init__(self, val):\\n\"\n"
        ".ascii \"        self.val = val\\n\"\n"

        ".ascii \"    def children(self):\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 1 == 1:\\n\"\n"
        ".ascii \"            yield ('value', self.val['_state']['_value'])\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 2 == 2:\\n\"\n"
        ".ascii \"            yield ('error', self.val['_state']['_error'])\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 4 == 4:\\n\"\n"
        ".ascii \"            yield ('exception', self.val['_ptr'])\\n\"\n"

        ".ascii \"    def display_hint(self):\\n\"\n"
        ".ascii \"        return None\\n\"\n"

        ".ascii \"    def to_string(self):\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 54 == 54:\\n\"\n"
        ".ascii \"            return 'errored (errno, moved from) + exceptioned'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 50 == 50:\\n\"\n"
        ".ascii \"            return 'errored (errno, moved from)'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 38 == 38:\\n\"\n"
        ".ascii \"            return 'errored + exceptioned (moved from)'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 36 == 36:\\n\"\n"
        ".ascii \"            return 'exceptioned (moved from)'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 35 == 35:\\n\"\n"
        ".ascii \"            return 'errored (moved from)'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 33 == 33:\\n\"\n"
        ".ascii \"            return 'valued (moved from)'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 22 == 22:\\n\"\n"
        ".ascii \"            return 'errored (errno) + exceptioned'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 18 == 18:\\n\"\n"
        ".ascii \"            return 'errored (errno)'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 6 == 6:\\n\"\n"
        ".ascii \"            return 'errored + exceptioned'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 4 == 4:\\n\"\n"
        ".ascii \"            return 'exceptioned'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 2 == 2:\\n\"\n"
        ".ascii \"            return 'errored'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 1 == 1:\\n\"\n"
        ".ascii \"            return 'valued'\\n\"\n"
        ".ascii \"        if self.val['_state']['_status']['status_value'] & 0xff == 0:\\n\"\n"
        ".ascii \"            return 'empty'\\n\"\n"

        ".ascii \"class OutcomeCResultStatusPrinter(object):\\n\"\n"
        ".ascii \"    '''Print a C result'''\\n\"\n"

        ".ascii \"    def __init__(self, val):\\n\"\n"
        ".ascii \"        self.val = val\\n\"\n"

        ".ascii \"    def children(self):\\n\"\n"
        ".ascii \"        if self.val['flags'] & 1 == 1:\\n\"\n"
        ".ascii \"            yield ('value', self.val['value'])\\n\"\n"
        ".ascii \"        if self.val['flags'] & 2 == 2:\\n\"\n"
        ".ascii \"            yield ('error', self.val['error'])\\n\"\n"

        ".ascii \"    def display_hint(self):\\n\"\n"
        ".ascii \"        return None\\n\"\n"

        ".ascii \"    def to_string(self):\\n\"\n"
        ".ascii \"        if self.val['flags'] & 50 == 50:\\n\"\n"
        ".ascii \"            return 'errored (errno, moved from)'\\n\"\n"
        ".ascii \"        if self.val['flags'] & 35 == 35:\\n\"\n"
        ".ascii \"            return 'errored (moved from)'\\n\"\n"
        ".ascii \"        if self.val['flags'] & 33 == 33:\\n\"\n"
        ".ascii \"            return 'valued (moved from)'\\n\"\n"
        ".ascii \"        if self.val['flags'] & 18 == 18:\\n\"\n"
        ".ascii \"            return 'errored (errno)'\\n\"\n"
        ".ascii \"        if self.val['flags'] & 2 == 2:\\n\"\n"
        ".ascii \"            return 'errored'\\n\"\n"
        ".ascii \"        if self.val['flags'] & 1 == 1:\\n\"\n"
        ".ascii \"            return 'valued'\\n\"\n"
        ".ascii \"        if self.val['flags'] & 0xff == 0:\\n\"\n"
        ".ascii \"            return 'empty'\\n\"\n"


        ".ascii \"class OutcomeCStatusCodePrinter(object):\\n\"\n"
        ".ascii \"    '''Print a C status code'''\\n\"\n"

        ".ascii \"    def __init__(self, val):\\n\"\n"
        ".ascii \"        self.val = val\\n\"\n"

        ".ascii \"    def children(self):\\n\"\n"
        ".ascii \"        yield ('domain', self.val['domain'])\\n\"\n"
        ".ascii \"        yield ('value', self.val['value'])\\n\"\n"

        ".ascii \"    def display_hint(self):\\n\"\n"
        ".ascii \"        return None\\n\"\n"

        ".ascii \"    def to_string(self):\\n\"\n"
        ".ascii \"        s = str(self.val['domain'])\\n\"\n"
        ".ascii \"        if 'posix_code_domain' in s or 'generic_code_domain' in s:\\n\"\n"
        ".ascii \"            return str(self.val['value']) + ' (' + os.strerror(int(self.val['value'])) + ')'\\n\"\n"
        ".ascii \"        else:\\n\"\n"
        ".ascii \"            return self.val['value']\\n\"\n"

        ".ascii \"def build_pretty_printer():\\n\"\n"
        ".ascii \"    pp = gdb.printing.RegexpCollectionPrettyPrinter('outcome_v2')\\n\"\n"
        ".ascii \"    pp.add_printer('outcome_v2::basic_result', '^(boost::)?outcome_v2[_0-9a-f]*::basic_result<.*>$', OutcomeBasicOutcomePrinter)\\n\"\n"
        ".ascii \"    pp.add_printer('outcome_v2::basic_outcome', '^(boost::)?outcome_v2[_0-9a-f]*::basic_outcome<.*>$', OutcomeBasicOutcomePrinter)\\n\"\n"
        ".ascii \"    pp.add_printer('cxx_result_status_code_*', '^cxx_result_status_code_.*$', OutcomeCResultStatusPrinter)\\n\"\n"
        ".ascii \"    pp.add_printer('cxx_status_code_*', '^cxx_status_code_.*$', OutcomeCStatusCodePrinter)\\n\"\n"
        ".ascii \"    return pp\\n\"\n"

        ".ascii \"def register_printers(obj = None):\\n\"\n"
        ".ascii \"    gdb.printing.register_pretty_printer(obj, build_pretty_printer(), replace = True)\\n\"\n"

        ".ascii \"register_printers(gdb.current_objfile())\\n\"\n"

        ".byte 0\n"
        ".popsection\n");
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif  // defined(__ELF__)
#endif  // !defined(BOOST_OUTCOME_DISABLE_INLINE_GDB_PRETTY_PRINTERS)

#endif  // !defined(BOOST_OUTCOME_INLINE_GDB_PRETTY_PRINTER_H)
