#!/bin/sh

# Copyright (c) 2013, Oracle and/or its affiliates. All rights
# reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2 of
# the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301  USA

node tweet put user caligula 'Gaius Julius Casear Germanicus'
node tweet put user uncle_claudius 'Tiberius Claudius Nero Germanicus'
node tweet put user nero 'Lucius Domitius Ahenobarus'
node tweet put user agrippina 'Julia Augusta Agrippina Minor'

node tweet put follow nero agrippina
node tweet put follow agrippina nero
node tweet put follow agrippina uncle_claudius
node tweet put follow agrippina caligula

node tweet post tweet caligula '@agrippina You really are my favorite sister.'
node tweet post tweet agrippina '@nero Remember to be nice to Uncle Claudius!' 
node tweet post tweet nero 'I love to sing!'
node tweet post tweet nero 'I am the best #poet and the best #gladiator!'
node tweet post tweet agrippina \
 '@uncle_claudius Please come over for dinner, we have some fantastic #mushrooms'
node tweet post tweet uncle_claudius 'I am writing a new history of #carthage'
node tweet post tweet caligula '@agrippina you are my worst sister! worst!' 
node tweet post tweet caligula '@agrippina Rome is terrible!!!'
 
node tweet get tweets-at agrippina
node tweet get tweets-about carthage
node tweet get tweets-by nero
node tweet get tweets-recent 5

node tweet get followers uncle_claudius # nobody
node tweet get following agrippina

node tweet start server 7800 & 

# Note that with curl -d 'data' you cannot start data with an @ sign
(
  sleep 2
  curl http://localhost:7800/tweets-at/agrippina 
  curl http://localhost:7800/tweets-about/carthage 
  curl http://localhost:7800/tweets-about/aqueduct
  curl http://localhost:7800/tweet/nero -d \
   'help! my #aqueduct has run dry again! @uncle_claudius!'
  curl http://localhost:7800/tweets-about/aqueduct
  curl http://localhost:7800/tweets-at/uncle_claudius 

  # Now delete everyone.  Afterwards the database will be empty
  # due to cascading deletes.
  curl -X DELETE http://localhost:7800/user/caligula
  curl -X DELETE http://localhost:7800/user/uncle_claudius
  curl -X DELETE http://localhost:7800/user/agrippina
  curl -X DELETE http://localhost:7800/user/nero

  # Try to delete someone a second time & you get an error:
  curl -X DELETE http://localhost:7800/user/uncle_claudius

  curl http://localhost:7800/tweets-recent/10  # empty
)

kill %1
