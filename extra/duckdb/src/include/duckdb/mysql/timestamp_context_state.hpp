#include "duckdb/main/connection.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/client_context_state.hpp"
#include "duckdb/common/types/timestamp.hpp"

namespace duckdb {
    struct TimestampContextState : ClientContextState
    {
        timestamp_t start_timestamp;
        void QueryBegin(ClientContext &context) override {
            start_timestamp = Timestamp::GetCurrentTimestamp();
        }
    };
}
