/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
*/


/* mySTL pair implements pair
 *
 */

#ifndef mySTL_PAIR_HPP
#define mySTL_PAIR_HPP



namespace mySTL {


template<typename T1, typename T2>
struct pair {
    typedef T1 first_type;
    typedef T2 second_type;

    first_type  first;
    second_type second;

    pair() {}
    pair(const T1& t1, const T2& t2) : first(t1), second(t2) {}

    template<typename U1, typename U2>
    pair(const pair<U1, U2>& p) : first(p.first), second(p.second) {}
};


template<typename T1, typename T2>
inline pair<T1, T2> make_pair(const T1& a, const T2& b)
{
    return pair<T1, T2>(a, b);
}



} // namespace mySTL

#endif // mySTL_PAIR_HPP
