#include "mysql/binlog/event/large_transaction_header_event.h"

#include "mysql/binlog/event/control_events.h"  // Format_description_event
#include "mysql/binlog/event/event_reader_macros.h"

namespace mysql::binlog::event {

Large_transaction_header_event::
    Large_transaction_header_event(
        const char *buf, const Format_description_event *fde)
    : Binary_log_event(&buf, fde) {
  BAPI_ENTER(
      "Large_transaction_header_event::"
      "Large_transaction_header_event(const char*, ...)");
  READER_TRY_INITIALIZATION;
  READER_ASSERT_POSITION(fde->common_header_len);

  READER_TRY_SET(m_version, read<uint8_t>);
  if (m_version == 0 || m_version > kVersion) {
    READER_THROW("Invalid Large_transaction_header version");
  }
  READER_TRY_SET(m_terminating_event_offset, read<uint64_t>);
  /* Read unconditionally: a truncated event missing the type byte is
     reported as a read error rather than silently defaulting the type. */
  READER_TRY_SET(m_terminating_event_type, read<uint8_t>);

  /* The remainder of the body is padding; its contents are ignored. */
  m_padding_size = READER_CALL(available_to_read);

  READER_CATCH_ERROR;
  BAPI_VOID_RETURN;
}

Large_transaction_header_event::
    Large_transaction_header_event(uint64_t terminating_event_offset,
                                   uint8_t terminating_event_type,
                                   uint64_t padding_size)
    : Binary_log_event(LARGE_TRANSACTION_HEADER_EVENT),
      m_terminating_event_offset(terminating_event_offset),
      m_terminating_event_type(terminating_event_type),
      m_padding_size(padding_size) {}

#ifndef HAVE_MYSYS
void Large_transaction_header_event::print_event_info(std::ostream &info) {
  info << "terminating event offset " << m_terminating_event_offset;
}

void Large_transaction_header_event::print_long_info(std::ostream &info) {
  info << "Timestamp: " << header()->when.tv_sec;
  info << "\tVersion: " << static_cast<unsigned>(m_version);
  info << "\tTerminating event offset: " << m_terminating_event_offset;
  info << "\tTerminating event type: "
       << static_cast<unsigned>(m_terminating_event_type);
  info << "\tPadding: " << m_padding_size << " bytes";
  info << "\n";
}
#endif

}  // namespace mysql::binlog::event
