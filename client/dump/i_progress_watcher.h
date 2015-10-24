/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef I_PROGRESS_WATCHER_INCLUDED
#define I_PROGRESS_WATCHER_INCLUDED

#include "i_chain_element.h"
#include "item_processing_data.h"
#include "i_crawler.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class I_progress_watcher : public virtual I_chain_element
{
public:
  /**
    Reports new non-empty chain being created by Chain Maker or new row
    fetched from table by Table Reader. Called from Crawler or Table Reader.
   */
  virtual void new_chain_created(
    Item_processing_data* new_chain_process_data)= 0;
  /**
    Report new object(table, row or any other) was started processing by
    specified Object Reader, Table Reader, Formatter or Row Formatter. Reported
    by these types. Is not reported by queues on enqueue but on dequeue.
   */
  virtual void object_processing_started(
    Item_processing_data* process_data)= 0;
  /**
    Report object(table, row or any other) finished being processed. In case of
    table, this does not necessarily mean that all rows were processed. That
    does not necessarily mean that object was successfully written by
    Output Writers.
   */
  virtual void object_processing_ended(
    Item_processing_data* finished_process_data)= 0;
  /**
    Reports crawler ended enumerating objects and creating chains for them.
   */
  virtual void crawler_completed(I_crawler* crawler)= 0;
};

}
}
}

#endif
