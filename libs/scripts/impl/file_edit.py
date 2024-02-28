#!/usr/bin/python3.11
# Copyright (c) 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

'''
Helper functions for editing files
'''

from   debug import trace
from   messages import *
import string

import re
import shlex
import subprocess
from   typing import List, Tuple

def append_to_file(*, path: str, text: str) -> None:
    '''Open the file *path* and append *text* to the end of its contents.'''

    with open(path, 'at') as fd:
        fd.write(text)

def overwrite_file(*, path: str, text: str) -> None:
    '''Replace the contents of the file *path* by *text*.'''

    with open(path, 'wt') as fd:
        fd.write(text)

def read_file(path: str) -> None:
    '''Read the file *path* and return the contents as a string.'''

    with open(path, 'rt') as fd:
        return fd.read()

def file_contains(*, path: str, text: str) -> bool:
    '''Return True if the file *path* contains the string *text*.'''

    return text in read_file(path)

@trace
def replace_in_file(*, path: str, old: str, new: str) -> None:
    '''Replace every occurrence of *old* by *new* in the given file.

    Either *old* and *new* are both strings, or *old* is a re.Pattern and *new*
    is string containing a regex substitution pattern.
    '''

    old_contents = read_file(path)
    if isinstance(old, str):
        new_contents = old_contents.replace(old, new)
    elif isinstance(old, re.Pattern):
        new_contents = old.sub(new, old_contents)
    overwrite_file(path=path, text=new_contents)

def multi_replace(
        text: str, old: List[str], new: List[str], words: bool
) -> Tuple[str, List[str]]:
    '''Replace multiple strings in the given string.

    This replaces every occurrence in the string *text*, of the Nth string in
    *old*, by the Nth string in *new*. If *words* is True, match only at word
    boundaries.

    :returns: 2-tuple, where the first component is the result and the second
        component is a list of strings that were *not* replaced.
    '''

    unchanged = []
    for o, n in zip(old, new):
        if n != o:
            new_text = string.replace(
                text=text, pattern=o, replacement=n, words=words
            )
            if new_text == text:
                unchanged.append(o)
            text = new_text
    return text, unchanged

@trace
def multi_replace_in_file(
        *, path: str, old: List[str], new: List[str], message: bool, words: bool
) -> List[str]:
    '''Replace multiple strings in the given file.

    This replaces every occurrence in the file *path*, of the Nth string/regex
    in *old*, by the Nth string/substitution in *new*. If *message* is True,
    prints a message. If *words* is True, match only at word boundaries.

    :returns: the list of strings that were *not* replaced.
    '''

    if message:
        log_info(
            'Replacing '
            + ', '.join([
                f'{old_str!r}->{new_str!r}'
                for old_str, new_str in zip(old, new)
            ])
            + f' in {path}'
        )
    new_contents, missed = multi_replace(
        text=read_file(path), old=old, new=new, words=words
    )
    overwrite_file(path=path, text=new_contents)
    return missed

@trace
def multi_replace_in_file_from_dict_with_warning(
        *,
        path: str,
        old_dict: dict,
        new_dict: dict,
        keys: List[str],
        no_warn: List[str]=[],
        message: bool,
        words: bool
) -> None:
    '''In *path*, replace *old_dict*[K] by *new_dict*[K] for K in *keys*.

    This will produce a warning for every K that did not result in a
    replacement, unless K is included in *no_warn*

    Parameters
    ----------
    :param path: Replace strings in this path.
    :param old_dict: Replace the values in this dict.
    :param new_dict: Replace by the values in this dict.
    :param keys: Replace only entries in the dict having keys in this set.
    :param no_warn: List of dict keys for which no warning should be generated,
        even if no replacement was done for the key.
    :param message: If True, print a message.
    :param words: If True, match only at word boundaries.
    '''

    missed = multi_replace_in_file(
        path=path,
        old=[old_dict[key] for key in keys],
        new=[new_dict[key] for key in keys],
        message=message,
        words=words
    )
    no_warn = {old_dict[item] for item in no_warn}
    missed = [item for item in missed if item not in no_warn]
    if missed:
        log_warning(
            f'Did not find the following in {path}:'
            + ''.join([f'\n  {item!r}' for item in missed])
        )

@trace
def replace_in_git_tree(
        *,
        old: str,
        new: str,
        words: bool,
        file_patterns: List[str]
) -> None:
    '''Replace *old* by *new* in all matching files in the current git tree.

    If *words* is True, match only at word boundaries.

    The file patterns may contain '?' and '*' wildcards.

    This uses rg, xargs, and sed to do the job (or if rg is not found, git,
    grep, xargs, and sed)'''

    def pattern_to_rg_glob(pattern: str) -> str:
        '''Given a pattern, produce a glob accepted by rg.'''
        out = ''
        try:
            it = iter(pattern)
            while True:
                c = next(it)
                if c == '\\':
                    c2 = next(it)
                    out += c + c2
                elif c in '{}':
                    # rg has special interpretation of curly braces. We disable
                    # that by quoting them, to make behavior consistent with
                    # the case that we rely on other tools.
                    out += '\\' + c
                else:
                    out += c
        except StopIteration:
            pass
        return out

    def pattern_to_regex(pattern):
        '''Given a pattern, produce a regex accepted by grep -E.'''
        out = ''
        try:
            it = iter(pattern)
            while True:
                c = next(it)
                if c == '\\':
                    c2 = next(it)
                    out += c + c2
                elif c in '?*':
                    out += '.' + c
                else:
                    out += re.escape(c)
        except StopIteration:
            pass
        return out

    old_regex = string.escape_regex(old)
    if words:
        old_regex = r'\b' + old_regex + r'\b'
    sed_script = f's&{old_regex}&{new}&g'
    xargs_sed = (
        f"xargs --no-run-if-empty sed --in-place -E -e "
        f"{shlex.quote(sed_script)}"
    )

    try:
        # Use rg if possible
        globs = ' '.join([
            f"--glob '{pattern_to_rg_glob(pattern)}'"
            for pattern in file_patterns
        ])
        subprocess.check_call(
            f"rg {globs} -l {shlex.quote(old_regex)} | "
            + xargs_sed,
            shell=True
        )
    except FileNotFoundError:
        # If rg doesn't exist, fall back to git grep
        extensions = '|'.join([
            pattern_to_regex(pattern)
            for pattern in file_patterns
        ])
        subprocess.check_call(
            f"git grep -E -l {shlex.quote(old_regex)} | "
            f"grep -E {extensions} |"
            + xargs_sed,
            shell=True
        )

# The file patterns that rg counts as "cpp", plus "yy".
cpp_file_patterns = ['C', 'h', 'H', 'cpp', 'hpp', 'cxx', 'hxx', 'cc', 'hh']
cpp_file_patterns += [f'{t}.in' for t in cpp_file_patterns]
cpp_file_patterns += ['inl', 'yy']
cpp_file_patterns = [f'*.{t}' for t in cpp_file_patterns]

cmakelists_file_patterns = ['CMakeLists.txt']

def _first_capture_group(match: re.Match) -> str:
    '''Return capture group 1 if it exists; otherwise the full match.'''
    try:
        return match.group(1)
    except:
        return match.group(0)

class ItemMatch:
    '''Helper data class for _get_section_match_list, representing one item.

    Stores the beginning and end of the item relative to the full string. (These
    differ from the positions in the re.Match objects used internally in the
    function, which are relative to the beginning of the list.)

    Also stores the substring of the item that will be used to compare items
    (i.e., the first capture group if one exists; otherwise the full item).
    '''

    def __init__(self, offset: int, match: re.Match):
        '''Create an ItemMatch from a given Match and the offset of the list.

        Parameters:
        ===========
        :param offset: Offset of the list in the string.
        :param match: re.Match object matching the item.
        '''

        self.start = offset + match.start(0)
        self.end = offset + match.end(0)
        self.text = _first_capture_group(match)

@trace
def _get_section_match_list(
        *,
        contents: str,
        start_regex: str,
        item_regex: str,
        end_regex: str
) -> List[ItemMatch] | None:
    '''Helper to get the list of re.Match objects matching items in a list.

    Find a section of *contents* that matches start_regex, followed by zero or
    more matches of item_regex, followed by a match of end_regex. Return the
    list of re.Match objects that match each item in the list.

    Parameters:
    ===========
    :param contents: The string to search.
    :param start_regex: Regular expression matching at the start of the list.
    :param item_regex: Regular expression matching an item in the list.
    :param end_regex: Regular expression matching the end of the list.
    :return: 2-tuple (start, end, item_list). If the section was found, start
        and end are the offsets to the beginning of the first element and the
        end of the last element, respectively, and item_list is the list of
        ItemMatch objects. If the section was not found, (0, 0, None).
    '''

    list_match = string.re_search(
        pattern=start_regex + '(?P<item>(' + item_regex + ')*)' + end_regex,
        string=contents
    )
    if not list_match:
        return 0, 0, None
    list_str = list_match.group('item')
    list_offset = list_match.start('item')
    return list_offset, list_match.end('item'), [
        ItemMatch(list_offset, match)
        for match in string.re_finditer(
            pattern=item_regex,
            string=list_str,
        )
    ]

@trace
def insert_in_text_section(
        *,
        contents: str,
        text: str,
        start_regex: str,
        item_regex: str,
        end_regex: str
) -> Tuple[int, int, str] | None:
    '''Insert text in alphabetic order in a list in the given string.

    Find a section of *contents* that matches *start_regex*, followed by zero or
    more matches of *item_regex*, followed by a match of *end_regex*. Insert
    *text* in this list, in alphabetic order, unless it already appears in the
    list in alphabetic order.

    If item_regex has any (unnamed) capture groups, then text is inserted in
    alphabetic order by capture group 1, comparing the match of each item with
    the match of item_regex with text. Also, text will be inserted only if
    capture group 1 in the match of item_regex with text does not appear as a
    capture group for one of the existing items.

    :param contents: Text that is expected to contain a list.
    :param text: Text to insert. This must match item_regex.
    :param start_regex: Regular expression matching at the start of the list.
    :param item_regex: Regular expression matching an item in the list.
    :param end_regex: Regular expression matching the end of the list.
    :return: 3-tuple (before, after, text), where before and after are the
        number of elements in the list before and after this call, and
        new_contents is contents where text has been inserted. If no list is
        found, returns (0, 0, contents).
    '''

    _, endpos, item_match_list = _get_section_match_list(
        contents=contents,
        start_regex=start_regex,
        item_regex=item_regex,
        end_regex=end_regex
    )
    if item_match_list is None:
        return 0, 0, contents
    compare_with = _first_capture_group(string.re_fullmatch(
        pattern=item_regex,
        string=text,
        must_match=True
    ))
    before_count = len(item_match_list)
    def insert_at_position(position):
        return before_count, before_count + 1, (
            contents[:position] + text + contents[position:]
        )
    for item_match in item_match_list:
        if item_match.text >= compare_with:
            if item_match.text == compare_with:
                return before_count, before_count, contents
            return insert_at_position(item_match.start)
    return insert_at_position(endpos)

@trace
def remove_from_text_section(
        *,
        contents: str,
        text: str,
        start_regex: str,
        item_regex: str,
        end_regex: str
) -> Tuple[int, int, str] | None:
    '''Remove text from a list in the given string.

    Find a section of *contents* that matches *start_regex*, followed by zero or
    more matches of *item_regex*, followed by a match of *end_regex*. Remove
    *text* from this list if it occurs.

    If item_regex has any (unnamed) capture groups, then capture group 1 of each
    item will be compared with capture group 1 when matching item_regex with
    text.

    :param contents: Text that is expected to contain a list.
    :param text: Text to remove. This must match item_regex.
    :param start_regex: Regular expression matching at the start of the list.
    :param item_regex: Regular expression matching an item in the list.
    :param end_regex: Regular expression matching the end of the list.
    :return: 3-tuple (before, after, text), where before and after are the
        number of elements in the list before and after this call, and
        new_contents is contents where text has been inserted. If no list is
        found, returns (0, 0, contents).
    '''
    _, _, item_match_list = _get_section_match_list(
        contents=contents,
        start_regex=start_regex,
        item_regex=item_regex,
        end_regex=end_regex
    )
    if not item_match_list:
        return 0, 0, contents
    compare_with = _first_capture_group(string.re_fullmatch(
        pattern=item_regex,
        string=text,
        must_match=True
    ))
    before_count = len(item_match_list)
    for item_match in item_match_list:
        if item_match.text == compare_with:
            return before_count, before_count - 1, (
                contents[:item_match.start] +
                contents[item_match.end:]
            )
    return before_count, before_count, contents

@trace
def insert_in_file_section(
        *,
        path: str,
        text: str,
        start_regex: str,
        item_regex: str,
        end_regex: str
):
    '''Insert text in alphabetic order in a list in the given file.

    See insert_in_text_section.

    :returns: 2-tuple (before, after), similar to insert_in_text_section.
    '''

    contents = read_file(path)
    before, after, new_contents = insert_in_text_section(
        contents=contents,
        text=text,
        start_regex=start_regex,
        item_regex=item_regex,
        end_regex=end_regex
    )
    if before != after:
        overwrite_file(path=path, text=new_contents)
    return before, after

@trace
def remove_from_file_section(
        *,
        path: str,
        text: str,
        start_regex: str,
        item_regex: str,
        end_regex: str
):
    '''Remove an item from a list in the given file.

    See remove_from_text_section.

    :returns: 2-tuple (before, after), similar to remove_from_text_section.
    '''

    contents = read_file(path)
    before, after, new_contents = remove_from_text_section(
        contents=contents,
        text=text,
        start_regex=start_regex,
        item_regex=item_regex,
        end_regex=end_regex
    )
    if before != after:
        overwrite_file(path=path, text=new_contents)
    return before, after
