#include "gcs_event_handlers.h"
#include "gcs_plugin.h"
#include <sql_class.h>
#include "gcs_message.h"
#include "gcs_protocol.h"
#include "gcs_corosync.h" // todo: describe needs
#include <set>

using std::set;

void handle_view_change(GCS::View& view, GCS::Member_set& total,
                        GCS::Member_set& left, GCS::Member_set& joined,
                        bool quorate)
{
  DBUG_ASSERT(view.get_view_id() == 0 || quorate);

  log_view_change(view.get_view_id(), total, left, joined);
}

void handle_message_delivery(GCS::Message *msg, const GCS::View& view)
{
  switch (GCS::get_payload_code(msg))
  {
  case GCS::PAYLOAD_TRANSACTION_EVENT:
    if (applier)
    {
      // andrei todo: raw byte shall be the same in all GCS modules == uchar
      applier->handle((const char*) GCS::get_payload_data(msg),
                      GCS::get_data_len(msg));
    }
    else
    {
      log_message(MY_ERROR_LEVEL, "Message received without a proper applier");
    }
    break;

  default:
    DBUG_ASSERT(0);
  }
};
