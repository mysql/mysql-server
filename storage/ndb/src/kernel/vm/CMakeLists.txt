# Copyright (C) 2007 MySQL AB
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/storage/ndb/src/mgmapi)
INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_kernel.cmake)

ADD_LIBRARY(ndbkernel STATIC
            VMSignal.cpp
            Emulator.cpp
            Configuration.cpp
            WatchDog.cpp
            SimplePropertiesSection.cpp
            SectionReader.cpp
            Mutex.cpp
            SafeCounter.cpp
            ndbd_malloc_impl.cpp
            ndbd_malloc.cpp
            DynArr256.cpp
            LockQueue.cpp
            Pool.cpp
            RWPool.cpp
            Rope.cpp
            WOPool.cpp
	    GloablData.cpp
)
ADD_LIBRARY(ndbsched STATIC
            TimeQueue.cpp
            ThreadConfig.cpp
            FastScheduler.cpp
            TransporterCallback_nonmt.cpp
            SimulatedBlock_nonmt.cpp
	    dummy_nonmt.cpp
)
