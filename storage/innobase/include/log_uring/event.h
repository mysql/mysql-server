#pragma once

#include <stdint.h>
#include <vector>

#define EVENT_TYPE_WRITE 1
#define EVENT_TYPE_FSYNC 2

typedef struct io_write_event {
  uint64_t lsn_;
  uint64_t index_;
  std::vector<uint8_t> buffer_;
} io_write_event_t;



class io_event {
public:
  uint32_t type_;
  io_write_event_t event_;
  io_event() {

  }
  ~io_event() {

  }
private:
};
