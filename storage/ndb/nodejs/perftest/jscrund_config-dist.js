/* Copyright (c) 2016, 2017 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/


/* To customize jscrund behavior, rename this file to jscrund.config and edit here.
   Properties on the command line override these options.
*/


/* jscrund options
 */
exports.options = {
  'adapter'       : 'ndb',
  'database'      : 'jscrund',
  'modes'         : 'indy,each,bulk',
  'tests'         : 'persist,find,remove',
  'iterations'    : 4000,
  'stats'         : false
};

