#pragma once


#include <stdint.h>

#define EVENT_TYPE_WRITE 1
#define EVENT_TYPE_FSYNC 2


typedef struct io_write_event {
  uint64_t lsn_;
  uint64_t size_;
  uint64_t index_;
  uint8_t buffer_[0];
  
} io_write_event_t;

typedef struct io_fsync_event {
  uint64_t lsn_;
  uint64_t index_;
} io_fsync_event_t;


union event_union {
  io_write_event_t write_event_;
  io_fsync_event_t fsync_event_; 
};

class event {
public:
  uint32_t type_;
  event_union event_;
  event();
  ~event();

private:

};
