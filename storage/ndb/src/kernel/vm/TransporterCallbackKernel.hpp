/* Copyright (C) 2008 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

class TransporterCallbackKernel: public TransporterCallback
{
public:
  /* TransporterCallback interface. */
  void deliver_signal(SignalHeader * const header,
                      Uint8 prio,
                      Uint32 * const signalData,
                      LinearSectionPtr ptr[3]);
  void reportReceiveLen(NodeId nodeId, Uint32 count, Uint64 bytes);
  void reportConnect(NodeId nodeId);
  void reportDisconnect(NodeId nodeId, Uint32 errNo);
  void reportError(NodeId nodeId, TransporterError errorCode,
                   const char *info = 0);
  void transporter_recv_from(NodeId node);
  virtual ~TransporterCallbackKernel() { }
};
