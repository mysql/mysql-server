/* Copyright (c) 2009, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef Defragger_H
#define Defragger_H


/*
  reception of fragmented signals
  - defragments signal based on nodeid and fragmentid
*/

class Defragger {
  struct DefragBuffer {
    // Key
    Uint32 m_fragment_id;
    NodeId m_node_id;
    // Data
    UtilBuffer m_buffer;
    DefragBuffer(NodeId nodeId, Uint32 fragId) :
      m_fragment_id(fragId), m_node_id(nodeId) {}
  };
  Vector<DefragBuffer*> m_buffers;

  DefragBuffer* find_buffer(NodeId nodeId, Uint32 fragId){
    for (size_t i = 0; i < m_buffers.size(); i++)
    {
      DefragBuffer* dbuf = m_buffers[i];
      if (dbuf->m_node_id == nodeId &&
          dbuf->m_fragment_id == fragId)
        return dbuf;
    }
    return NULL;
  }

  void erase_buffer(const DefragBuffer* dbuf){
    for (size_t i = 0; i < m_buffers.size(); i++)
    {
      if (m_buffers[i] == dbuf)
      {
        delete dbuf;
        m_buffers.erase(i);
        return;
      }
    }
    assert(false); // Should never be reached
  }

public:
  Defragger() {};
  ~Defragger()
  {
    for (size_t i = m_buffers.size(); i > 0; --i)
    {
      delete m_buffers[i-1]; // free the memory of the fragment
    }
    // m_buffers will be freed by ~Vector
  };

  /*
    return true when complete signal received
  */

  bool defragment(SimpleSignal* sig) {

    if (!sig->isFragmented())
      return true;

    Uint32 fragId = sig->getFragmentId();
    NodeId nodeId = refToNode(sig->header.theSendersBlockRef);

    DefragBuffer* dbuf;
    if(sig->isFirstFragment()){

      // Make sure buffer does not exist
      if (find_buffer(nodeId, fragId))
        abort();

      dbuf = new DefragBuffer(nodeId, fragId);
      m_buffers.push_back(dbuf);

    } else {
      dbuf = find_buffer(nodeId, fragId);
      if (dbuf == NULL)
        abort();
    }
    if (dbuf->m_buffer.append(sig->ptr[0].p, sig->ptr[0].sz * sizeof(Uint32)))
      abort(); // OOM

    if (!sig->isLastFragment())
      return false;

    // Copy defragmented data into signal...
    int length = dbuf->m_buffer.length();
    delete[] sig->ptr[0].p;
    sig->ptr[0].sz = (length+3)/4;
    sig->ptr[0].p = new Uint32[sig->ptr[0].sz];
    memcpy(sig->ptr[0].p, dbuf->m_buffer.get_data(), length);

    // erase the buffer data
    erase_buffer(dbuf);
    return true;
  }


  /*
    clear any unassembled signal buffers from node
  */
  void node_failed(NodeId nodeId) {
    for (size_t i = m_buffers.size(); i > 0; --i)
    {
      if (m_buffers[i-1]->m_node_id == nodeId)
      {
        delete m_buffers[i]; // free the memory of the signal fragment
	m_buffers.erase(i); // remove the reference from the vector.
      }
    }
  }

};

#endif
