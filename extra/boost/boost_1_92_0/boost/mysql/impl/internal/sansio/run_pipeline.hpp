//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RUN_PIPELINE_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_SANSIO_RUN_PIPELINE_HPP

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/is_fatal_error.hpp>
#include <boost/mysql/pipeline.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/next_action.hpp>
#include <boost/mysql/detail/pipeline.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>
#include <boost/mysql/impl/internal/sansio/execute.hpp>
#include <boost/mysql/impl/internal/sansio/ping.hpp>
#include <boost/mysql/impl/internal/sansio/prepare_statement.hpp>
#include <boost/mysql/impl/internal/sansio/reset_connection.hpp>
#include <boost/mysql/impl/internal/sansio/set_character_set.hpp>

#include <boost/assert.hpp>
#include <boost/core/span.hpp>

#include <cstddef>
#include <vector>

namespace boost {
namespace mysql {
namespace detail {

class run_pipeline_algo
{
    union any_read_algo
    {
        std::nullptr_t nothing;
        read_execute_response_algo execute;
        read_prepare_statement_response_algo prepare_statement;
        read_reset_connection_response_algo reset_connection;
        read_ping_response_algo ping;
        read_set_character_set_response_algo set_character_set;

        any_read_algo() noexcept : nothing{} {}
    };

    span<const std::uint8_t> request_buffer_;
    span<const pipeline_request_stage> stages_;
    std::vector<stage_response>* response_;

    int resume_point_{0};
    std::size_t current_stage_index_{0};
    error_code pipeline_ec_;  // Result of the entire operation
    bool has_hatal_error_{};  // If true, fail further stages with pipeline_ec_
    any_read_algo read_response_algo_;
    diagnostics temp_diag_;

    void setup_response()
    {
        if (response_)
        {
            // Create as many response items as request stages
            response_->resize(stages_.size());

            // Setup them
            for (std::size_t i = 0u; i < stages_.size(); ++i)
            {
                // Execution stages need to be initialized to results objects.
                // Otherwise, clear any previous content
                auto& impl = access::get_impl((*response_)[i]);
                if (stages_[i].kind == pipeline_stage_kind::execute)
                    impl.emplace_results();
                else
                    impl.emplace_error();
            }
        }
    }

    void setup_current_stage(const connection_state_data& st)
    {
        // Reset previous data
        temp_diag_.clear();

        // Setup read algo
        auto stage = stages_[current_stage_index_];
        switch (stage.kind)
        {
        case pipeline_stage_kind::execute:
        {
            BOOST_ASSERT(response_ != nullptr);  // we don't support execution ignoring the response
            auto& processor = access::get_impl((*response_)[current_stage_index_]).get_processor();
            processor.reset(stage.stage_specific.enc, st.meta_mode);
            processor.sequence_number() = stage.seqnum;
            read_response_algo_.execute = {&processor};
            break;
        }
        case pipeline_stage_kind::prepare_statement:
            read_response_algo_.prepare_statement = {stage.seqnum};
            break;
        case pipeline_stage_kind::close_statement:
            // Close statement doesn't have a response
            read_response_algo_.nothing = nullptr;
            break;
        case pipeline_stage_kind::set_character_set:
            read_response_algo_.set_character_set = {stage.stage_specific.charset, stage.seqnum};
            break;
        case pipeline_stage_kind::reset_connection:
            read_response_algo_.reset_connection = {stage.seqnum};
            break;
        case pipeline_stage_kind::ping: read_response_algo_.ping = {stage.seqnum}; break;
        default: BOOST_ASSERT(false);  // LCOV_EXCL_LINE
        }
    }

    void set_stage_error(error_code ec, diagnostics&& diag)
    {
        if (response_)
        {
            access::get_impl((*response_)[current_stage_index_]).set_error(ec, std::move(diag));
        }
    }

    void on_stage_finished(const connection_state_data& st, diagnostics& output_diag, error_code stage_ec)
    {
        if (stage_ec)
        {
            // If the error was fatal, fail successive stages.
            // This error is the result of the operation
            if (is_fatal_error(stage_ec))
            {
                pipeline_ec_ = stage_ec;
                output_diag = temp_diag_;
                has_hatal_error_ = true;
            }
            else if (!pipeline_ec_)
            {
                // In the absence of fatal errors, the first error we encounter is the result of the operation
                pipeline_ec_ = stage_ec;
                output_diag = temp_diag_;
            }

            // Propagate the error
            if (response_ != nullptr)
            {
                set_stage_error(stage_ec, std::move(temp_diag_));
            }
        }
        else
        {
            if (stages_[current_stage_index_].kind == pipeline_stage_kind::prepare_statement)
            {
                // Propagate results. We don't support prepare statements ignoring the response
                BOOST_ASSERT(response_ != nullptr);
                access::get_impl((*response_)[current_stage_index_])
                    .set_result(read_response_algo_.prepare_statement.result(st));
            }
        }
    }

    next_action resume_read_algo(connection_state_data& st, error_code ec)
    {
        switch (stages_[current_stage_index_].kind)
        {
        case pipeline_stage_kind::execute: return read_response_algo_.execute.resume(st, temp_diag_, ec);
        case pipeline_stage_kind::prepare_statement:
            return read_response_algo_.prepare_statement.resume(st, temp_diag_, ec);
        case pipeline_stage_kind::reset_connection:
            return read_response_algo_.reset_connection.resume(st, temp_diag_, ec);
        case pipeline_stage_kind::set_character_set:
            return read_response_algo_.set_character_set.resume(st, temp_diag_, ec);
        case pipeline_stage_kind::ping: return read_response_algo_.ping.resume(st, temp_diag_, ec);
        case pipeline_stage_kind::close_statement: return next_action();  // has no response
        default: BOOST_ASSERT(false); return next_action();               // LCOV_EXCL_LINE
        }
    }

public:
    run_pipeline_algo(run_pipeline_algo_params params) noexcept
        : request_buffer_(params.request_buffer), stages_(params.request_stages), response_(params.response)
    {
    }

    next_action resume(connection_state_data& st, diagnostics& diag, error_code ec)
    {
        next_action act;

        switch (resume_point_)
        {
        case 0:

            // Clear previous state
            setup_response();

            // If the request is empty, don't do anything
            if (stages_.empty())
                break;

            // Check status
            ec = st.check_status_ready();
            if (ec)
                return ec;

            // Write the request. use_ssl is attached by top_level_algo
            BOOST_MYSQL_YIELD(resume_point_, 1, next_action::write({request_buffer_, false}))

            // If writing the request failed, fail all the stages with the given error code
            if (ec)
            {
                pipeline_ec_ = ec;
                has_hatal_error_ = true;
            }

            // For each stage
            for (; current_stage_index_ < stages_.size(); ++current_stage_index_)
            {
                // If there was a fatal error, just set the error and move forward
                if (has_hatal_error_)
                {
                    set_stage_error(pipeline_ec_, diagnostics(diag));
                    continue;
                }

                // Setup the stage
                setup_current_stage(st);

                // Run it until completion
                ec.clear();
                while (!(act = resume_read_algo(st, ec)).is_done())
                    BOOST_MYSQL_YIELD(resume_point_, 2, act)

                // Process the stage's result
                on_stage_finished(st, diag, act.error());
            }
        }

        return pipeline_ec_;
    }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
