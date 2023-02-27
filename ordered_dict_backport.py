#
# Copyright (c) 2017, 2020, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
# This program is distributed in the hope that it will be useful,  but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
# the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#

# Backport of OrderedDict() class that runs on Python 2.6
# Based on the recipe suggested by the official Python documentation:
# https://docs.python.org/2/library/collections.html#collections.OrderedDict.

try:
    from thread import get_ident as _get_ident
except ImportError:
    from dummy_thread import get_ident as _get_ident

try:
    from _abcoll import KeysView, ValuesView, ItemsView
except ImportError:
    pass


class OrderedDict(dict):
    """Dictionary that remembers the order entries were added

    An inherited dict maps keys to values.
    The inherited dict provides __getitem__, __len__, __contains__, and get.
    The remaining methods are order-aware.

    The internal self.__map dictionary maps keys to links in a doubly linked
    list. The circular doubly linked list starts and ends with a sentinel
    element. The sentinel element never gets deleted (simplifying the
    algorithm). Each link is stored as a list of length three:
    [PREV, NEXT, KEY].
    """

    def __init__(self, *args, **kwds):
        """ Constructor

        Initialize an ordered dictionary. Signature is the same as for
        regular dictionaries, but keyword arguments are not recommended
        because their insertion order is arbitrary.
        """
        if len(args) > 1:
            raise TypeError('expected at most 1 arguments, got %d' % len(args))
        try:
            self.__root
        except AttributeError:
            self.__root = root = []  # sentinel node
            root[:] = [root, root, None]
            self.__map = {}
        self.__update(*args, **kwds)

    def __setitem__(self, key, value, dict_setitem=dict.__setitem__):
        """ Implements assignment to ordered_dict[key]:

        ordered_dict[key] = value

        Setting a new item creates a new link which goes at the end of the
        linked list, and the inherited dictionary is updated with the new
        key/value pair.
        """
        if key not in self:
            root = self.__root
            last = root[0]
            last[1] = root[0] = self.__map[key] = [last, root, key]
        dict_setitem(self, key, value)

    def __delitem__(self, key, dict_delitem=dict.__delitem__):
        """Implements deletion of ordered_dict[key]:
        del ordered_dict[key]

        Deleting an existing item uses self.__map to find the link which is
        then removed by updating the links in the predecessor and successor
        nodes.
        """
        dict_delitem(self, key)
        link_prev, link_next, key = self.__map.pop(key)
        link_prev[1] = link_next
        link_next[0] = link_prev

    def __iter__(self):
        """ Returns an iterator object during an iteration.
        """
        root = self.__root
        curr = root[1]
        while curr is not root:
            yield curr[2]
            curr = curr[1]

    def __reversed__(self):
        """ Returns an iterator object during a reverse iteration.
        """
        root = self.__root
        curr = root[0]
        while curr is not root:
            yield curr[2]
            curr = curr[0]

    def clear(self):
        """Remove all items from the dictionary.
        """
        try:
            for node in self.__map.itervalues():
                del node[:]
            root = self.__root
            root[:] = [root, root, None]
            self.__map.clear()
        except AttributeError:
            pass
        dict.clear(self)

    def popitem(self, last=True):
        """Remove and return a (key, value) pair from the dictionary.

        Pairs are returned in LIFO order if 'last' is true (default) or FIFO
        order if false.
        """
        if not self:
            raise KeyError('dictionary is empty')
        root = self.__root
        if last:
            link = root[0]
            link_prev = link[0]
            link_prev[1] = root
            root[0] = link_prev
        else:
            link = root[1]
            link_next = link[1]
            root[1] = link_next
            link_next[0] = root
        key = link[2]
        del self.__map[key]
        value = dict.pop(self, key)
        return key, value

    def keys(self):
        """ Return a list with a copy of the dictionary keys.
        """
        return list(self)

    def values(self):
        """ Return a list with a copy of the dictionary values.
        """
        return [self[key] for key in self]

    def items(self):
        """ Return a list with a copy of the dictionary (key, value) pairs.
        """
        return [(key, self[key]) for key in self]

    def iterkeys(self):
        """ Return an iterator over the dictionary keys.
        """
        return iter(self)

    def itervalues(self):
        """ Return an iterator over the dictionary values.
        """
        for k in self:
            yield self[k]

    def iteritems(self):
        """ Return an iterator over the dictionary (key, value) pairs.
        """
        for k in self:
            yield (k, self[k])

    def update(*args, **kwds):
        """ Update the dictionary with the key/value pairs from other.

        Update ordered_dict from dict/iterable D and I.

        If D is a dict instance, then: for k in D: ordered_dict[k] = D[k]
        If D has a keys() method, then:
            for k in D.keys(): ordered_dict[k] = D[k]
        Or if D is an iterable of items, then:
            for k, v in D: ordered_dict[k] = v
        In either case, this is followed by:
            for k, v in I.items(): ordered_dict[k] = v
        """
        if len(args) > 2:
            raise TypeError('update() takes at most 2 positional '
                            'arguments (%d given)' % (len(args),))
        elif not args:
            raise TypeError('update() takes at least 1 argument (0 given)')
        self = args[0]
        # Make progressively weaker assumptions about "other"
        other = ()
        if len(args) == 2:
            other = args[1]
        if isinstance(other, dict):
            for key in other:
                self[key] = other[key]
        elif hasattr(other, 'keys'):
            for key in other.keys():
                self[key] = other[key]
        else:
            for key, value in other:
                self[key] = value
        for key, value in kwds.items():
            self[key] = value
    # let subclasses override update without breaking __init__
    __update = update
    __marker = object()

    def pop(self, key, default=__marker):
        """ Remove the key from the dictionary and return its value.

        If the key does not exist the default is returned (if specified).

        Remove specified key and return the corresponding value. If key is not
        found, d is returned if given, otherwise KeyError is raised.
        """
        if key in self:
            result = self[key]
            del self[key]
            return result
        if default is self.__marker:
            raise KeyError(key)
        return default

    def __repr__(self, _repr_running={}):
        """ Compute the "official" string representation of the dictionary.

        Used by repr(). In this case also used by str() and print since
        __str__() is not implemented.
        """
        call_key = id(self), _get_ident()
        if call_key in _repr_running:
            return '...'
        _repr_running[call_key] = 1
        try:
            if not self:
                return '%s()' % (self.__class__.__name__,)
            return '%s(%r)' % (self.__class__.__name__, self.items())
        finally:
            del _repr_running[call_key]

    def __eq__(self, other):
        """ Used for equal comparison: ordered_dict == other

        Comparison to another OrderedDict is order-sensitive
        while comparison to a regular mapping is order-insensitive.
        """
        if isinstance(other, OrderedDict):
            return len(self)==len(other) and self.items() == other.items()
        return dict.__eq__(self, other)

    def __ne__(self, other):
        """ Used for not equal comparison: ordered_dict <> other
        """
        return not self == other
