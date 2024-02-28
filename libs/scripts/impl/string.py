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
String helpers.
'''

from   collections.abc import Callable, Iterator
from   functools import wraps
import re
from   typing import List

# Character Case Constants
class Upper: pass
class Lower: pass
class Camel: pass

def args_to_str(*args, **kwargs):
    return ', '.join(
        [
            f'{arg!r}' for arg in args
        ] + [
            f'{key}={value!r}' for key, value in kwargs.items()
        ]
    )

def interception(
        function: Callable, extra_kwargs={}, bad_return=None,
) -> Callable:
    @wraps(function)
    def interception_wrapper(*args, **kwargs):
        try:
            ret = function(*args, **kwargs)
            if bad_return:
                bad = (
                    bad_return(ret)
                    if callable(bad_return) else
                    (ret in bad_return)
                )
                if bad:
                    raise ValueError(f'{function.__name__} returned {ret!r}')
            return ret
        except Exception as e:
            raise RuntimeError(
                f'Thrown from {function.__name__}('
                + args_to_str(*args, **kwargs)
                + ') ['
                + args_to_str(**extra_kwargs)
                + ']'
            )
    return interception_wrapper

def re_search(*args, **kwargs) -> re.Match | None:
    return interception(
        re.search,
        bad_return=[None] if kwargs.pop('must_match', False) else None
    )(*args, **kwargs)

def re_match(*args, **kwargs) -> re.Match | None:
    return interception(
        re.match,
        bad_return=[None] if kwargs.pop('must_match', False) else None
    )(*args, **kwargs)

@interception
def re_fullmatch(*args, **kwargs) -> re.Match | None:
    return interception(
        re.fullmatch,
        bad_return=[None] if kwargs.pop('must_match', False) else None
    )(*args, **kwargs)

@interception
def re_split(*args, **kwargs) -> List[str]:
    return interception(
        re.split
    )(*args, **kwargs)

@interception
def re_compile(*args, **kwargs) -> re.Pattern:
    return interception(
        re.compile,
        bad_return=(
            (lambda x: len(x) < 2)
            if kwargs.pop('must_match', False) else
            None
        )
    )(*args, **kwargs)

@interception
def re_findall(*args, **kwargs) -> List[str]:
    return interception(
        re.findall,
        bad_return=[[]] if kwargs.pop('must_match', False) else None
    )(*args, **kwargs)

@interception
def re_finditer(*args, **kwargs) -> Iterator[re.Match]:
    return interception(
        re.finditer
    )(*args, **kwargs)

@interception
def re_sub(*args, **kwargs) -> str:
    return interception(
        re.sub,
        bad_return=[None] if kwargs.pop('must_match', False) else None
    )(*args, **kwargs)

@interception
def re_subn(*args, **kwargs) -> str:
    return interception(
        re.subn
    )(*args, **kwargs)

_split_word_pattern = re_compile(pattern=r'(?<=[^A-Z])(?=[A-Z])|[^a-zA-Z0-9]')
def convert_case(text: str | List[str], case, glue=None) -> str:
    '''Change character case for *text* according to *case*.

    If *text* is a string, it will be split into a list of words, delimited by
    non-alphanumeric letters and/or positions having a non-capital letter on
    the left side and a capital letter on the right side. Then each word is
    converted according to *case*. Finally, the words are joined together.

    Parameters
    ----------
    :param text: Input string or list of strings.
    :param case: One of the constants Uppe, Lower, or Camel, indicating the case
        used for each word.
    :param glue: The character to insert between words. If this is not given, it
        uses underscore when case is Upper or Lower, and empty string when case
        is Camel.
    '''

    if glue is None:
        glue = '' if case == Camel else '_'
    if isinstance(text, list):
        parts = text
    else:
        parts = _split_word_pattern.split(string=text)
    if case == Upper:
        return glue.join(parts).upper()
    elif case == Lower:
        return glue.join(parts).lower()
    elif case == Camel:
        return glue.join([part.capitalize() for part in parts])
    raise ValueError(f'Unknown case: "{case}"')

def word_pattern(
        *, pattern: str | re.Pattern, words: bool
) -> str | re.Pattern:
    '''Return regex pattern that optionally matches only outside words.

    Parameters:
    -----------
    :param pattern: The pattern.
    :param words: If True, return a regex Pattern that must not be preceded or
        followed by a word character. If False, just return pattern unmodified.
    '''
    if words:
        if isinstance(pattern, re.Pattern):
            return re_compile(
                pattern=r'(?<!\w)(?:' + pattern.pattern + r')(?!\w)',
                flags=pattern.flags
            )
        else:
            return re_compile(
                pattern=r'(?<!w)' + re.escape(pattern) + r'(?!\w)',
            )
    return pattern

def matches(*, text, pattern: str | re.Pattern, words: bool) -> bool:
    '''Determine if *text* matches a *pattern*, which may be a string or regex.

    If *pattern* is a regular expression pattern, return True if *text*
    fullmatches it. Otherwise, if *pattern* is a string, return True if
    *pattern* and *text* are equal.

    Parameters
    ----------
    :param text: String to check.
    :param pattern: Pattern to match.

    :returns: True if there is a match, False otherwise.
    '''

    pattern = word_pattern(pattern=pattern, words=words)
    if isinstance(pattern, re.Pattern):
        return pattern.fullmatch(text)
    if isinstance(pattern, str):
        return text == pattern
    raise TypeError(f'Invalid type for {pattern=}; expected str or re.Pattern')

def replace(
        *, text, pattern: str | re.Pattern, replacement: str, words: bool
) -> str:
    '''Replace all occurrences of a string or regex by another string.

    If *pattern* is a re.Pattern, replace all substrings in *text* that match
    *pattern* using the replacement pattern *replacement*, as with re.sub.
    Otherwise, if *pattern* is a string, replace all occurrences of *pattern* in
    *text* by the literal string *replacement*.

    Parameters
    ----------
    :param text: String to check.
    :param pattern: Pattern to match.
    :param replacement: Replacement string or pattern. If *pattern* is a string,
        this is used literally. If *pattern* is a re.Pattern, this may contain
        backreferences, as with re.sub.
    :param words: If True, match only when not preceded or followed by word a
        character.

    :return: The resulting string
    '''

    # Match word boundaries if requested
    if isinstance(pattern, re.Pattern):
        pattern = word_pattern(pattern=pattern.re, words=words)
    else:
        pattern = word_pattern(pattern=pattern, words=words)
        if isinstance(pattern, re.Pattern):
            # Escape special characters in the replacement pattern
            replacement = replacement.replace('\\', '\\\\')

    if isinstance(pattern, re.Pattern):
        return interception(pattern.sub, pattern)(
            repl=replacement, string=text
        )
    if isinstance(pattern, str):
        return text.replace(pattern, replacement)

def escape_regex(regex: str) -> str:
    '''Escape special regex characters in the given string.

    For better readability, does not escape space characters.
    '''

    return re.escape(regex).replace(r'\ ', ' ')
