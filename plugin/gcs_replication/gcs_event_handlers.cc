#include "gcs_event_handlers.h"
#include "gcs_plugin.h"
#include <sql_class.h>


void handle_view_change(GCS::View& view, GCS::Member_set& total,
                        GCS::Member_set& left, GCS::Member_set& joined,
                        bool quorate)
{
  log_message(MY_INFORMATION_LEVEL,
              "A view change was detected. Current number of members: "
              "%d, left %d, joined %d",
              (int)total.size(), (int)left.size(), (int)joined.size());
}

void handle_message_delivery(GCS::Message *msg, const GCS::View& view)
{
  if (applier)
  {
    applier->handle((const char*)msg->get_data(), msg->get_length());
  }
  else
  {
    log_message(MY_ERROR_LEVEL, "Message received without a proper applier");
  }
};
