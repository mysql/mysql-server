#ifndef GCS_EVENT_HANDLERS_INCLUDE
#define GCS_EVENT_HANDLERS_INCLUDE

#include <gcs_protocol.h>
#include <gcs_protocol_factory.h>

/*
  The function is called at View change and receives the view and three set of
  members as arguments:

  @param[in]  view    the view being installed
  @param[in]  total   the members in the view being installed
  @param[in]  left    the members that left the view
  @param[in]  joined  the members that joined the view


  @note Using that info this function implements a prototype of Node manager
  that checks quorate condition and terminates this instance membership when
  it does not hold.

  @note This definition is only for testing purposes for now.
*/
void handle_view_change(GCS::View& view, GCS::Member_set& total,
                        GCS::Member_set& left, GCS::Member_set& joined,
                        bool quorate);

/*
  The function is invoked whenever a message is delivered from a group.

  @param[in] msg     pointer to Message object
  @param[in] view    pointer to the View in which delivery is happening
*/
void handle_message_delivery(GCS::Message *msg, const GCS::View& view);

#endif /* GCS_EVENT_HANDLERS_INCLUDE */