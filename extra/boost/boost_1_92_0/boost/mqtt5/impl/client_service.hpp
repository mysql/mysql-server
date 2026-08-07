//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_CLIENT_SERVICE_HPP
#define BOOST_MQTT5_CLIENT_SERVICE_HPP

#include <boost/mqtt5/detail/channel_traits.hpp>
#include <boost/mqtt5/detail/internal_types.hpp>
#include <boost/mqtt5/detail/log_invoke.hpp>

#include <boost/mqtt5/impl/assemble_op.hpp>
#include <boost/mqtt5/impl/async_sender.hpp>
#include <boost/mqtt5/impl/autoconnect_stream.hpp>
#include <boost/mqtt5/impl/replies.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/experimental/basic_channel.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/prepend.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <variant> // std::monostate

namespace boost::mqtt5::detail {

namespace asio = boost::asio;

template <
    typename StreamType, typename TlsContext,
    typename Enable = void
>
class stream_context;

template <
    typename StreamType, typename TlsContext
>
class stream_context<
    StreamType, TlsContext,
    std::enable_if_t<has_tls_layer<StreamType>>
> {
    using tls_context_type = TlsContext;

    mqtt_ctx _mqtt_context;
    std::shared_ptr<tls_context_type> _tls_context_ptr;

public:
    explicit stream_context(TlsContext tls_context) :
        _tls_context_ptr(std::make_shared<tls_context_type>(std::move(tls_context)))
    {}

    stream_context(const stream_context& other) :
        _mqtt_context(other._mqtt_context), _tls_context_ptr(other._tls_context_ptr)
    {}

    auto& mqtt_context() {
        return _mqtt_context;
    }

    const auto& mqtt_context() const {
        return _mqtt_context;
    }

    auto& tls_context() {
        return *_tls_context_ptr;
    }

    auto& session_state() {
        return _mqtt_context.state;
    }

    const auto& session_state() const {
        return _mqtt_context.state;
    }

    void will(will will) {
        _mqtt_context.will_msg = std::move(will);
    }

    template <prop::property_type p>
    const auto& connack_property(
        std::integral_constant<prop::property_type, p> prop
    ) const {
        return _mqtt_context.ca_props[prop];
    }

    const auto& connack_properties() const {
        return _mqtt_context.ca_props;
    }

    template <prop::property_type p>
    const auto& connect_property(
        std::integral_constant<prop::property_type, p> prop
    ) const {
        return _mqtt_context.co_props[prop];
    }

    template <prop::property_type p>
    auto& connect_property(
        std::integral_constant<prop::property_type, p> prop
    ) {
        return _mqtt_context.co_props[prop];
    }

    void connect_properties(connect_props props) {
        _mqtt_context.co_props = std::move(props);
    }

    void credentials(
        std::string client_id,
        std::string username = "", std::string password = ""
    ) {
        _mqtt_context.creds = {
            std::move(client_id),
            std::move(username), std::move(password)
        };
    }

    template <typename Authenticator>
    void authenticator(Authenticator&& authenticator) {
        _mqtt_context.authenticator = any_authenticator(
            std::forward<Authenticator>(authenticator)
        );
    }
};

template <typename StreamType>
class stream_context<
    StreamType, std::monostate,
    std::enable_if_t<!has_tls_layer<StreamType>>
> {
    mqtt_ctx _mqtt_context;
public:
    explicit stream_context(std::monostate) {}

    stream_context(const stream_context& other) :
        _mqtt_context(other._mqtt_context)
    {}

    auto& mqtt_context() {
        return _mqtt_context;
    }

    const auto& mqtt_context() const {
        return _mqtt_context;
    }

    auto& session_state() {
        return _mqtt_context.state;
    }

    const auto& session_state() const {
        return _mqtt_context.state;
    }

    void will(will will) {
        _mqtt_context.will_msg = std::move(will);
    }

    template <prop::property_type p>
    const auto& connack_property(
        std::integral_constant<prop::property_type, p> prop
    ) const {
        return _mqtt_context.ca_props[prop];
    }

    const auto& connack_properties() const {
        return _mqtt_context.ca_props;
    }

    template <prop::property_type p>
    const auto& connect_property(
        std::integral_constant<prop::property_type, p> prop
    ) const {
        return _mqtt_context.co_props[prop];
    }

    template <prop::property_type p>
    auto& connect_property(
        std::integral_constant<prop::property_type, p> prop
    ) {
        return _mqtt_context.co_props[prop];
    }

    void connect_properties(connect_props props) {
        _mqtt_context.co_props = std::move(props);
    }

    void credentials(
        std::string client_id,
        std::string username = "", std::string password = ""
    ) {
        _mqtt_context.creds = {
            std::move(client_id),
            std::move(username), std::move(password)
        };
    }

    template <typename Authenticator>
    void authenticator(Authenticator&& authenticator) {
        _mqtt_context.authenticator = any_authenticator(
            std::forward<Authenticator>(authenticator)
        );
    }
};

template <
    typename StreamType,
    typename TlsContext = std::monostate,
    typename LoggerType = noop_logger
>
class client_service {
    using self_type = client_service<StreamType, TlsContext, LoggerType>;
    using stream_context_type = stream_context<StreamType, TlsContext>;
    using stream_type = autoconnect_stream<
        StreamType, stream_context_type, LoggerType
    >;
public:
    using executor_type = typename stream_type::executor_type;
private:
    using tls_context_type = TlsContext;
    using logger_type = LoggerType;
    using receive_channel = asio::experimental::basic_channel<
        executor_type,
        channel_traits<>,
        void (error_code, std::string, std::string, publish_props)
    >;

    template <typename ClientService, typename Handler>
    friend class run_op;

    template <typename ClientService>
    friend class async_sender;

    template <typename ClientService, typename Handler>
    friend class assemble_op;

    template <typename ClientService, typename Handler>
    friend class ping_op;

    template <typename ClientService, typename Handler>
    friend class sentry_op;

    template <typename ClientService>
    friend class re_auth_op;

    executor_type _executor;

    log_invoke<logger_type> _log;

    stream_context_type _stream_context;
    stream_type _stream;

    packet_id_allocator _pid_allocator;
    replies _replies;
    async_sender<client_service> _async_sender;

    std::string _read_buff;
    data_span _active_span;

    receive_channel _rec_channel;

    timer_type _ping_timer;
    timer_type _sentry_timer;

    client_service(const client_service& other) :
        _executor(other._executor),
        _log(other._log),
        _stream_context(other._stream_context),
        _stream(_executor, _stream_context, _log),
        _replies(_executor),
        _async_sender(*this),
        _active_span(_read_buff.cbegin(), _read_buff.cbegin()),
        _rec_channel(_executor, (std::numeric_limits<size_t>::max)()),
        _ping_timer(_executor),
        _sentry_timer(_executor)
    {
        _stream.clone_endpoints(other._stream);
    }

public:

    explicit client_service(
        const executor_type& ex,
        tls_context_type tls_context = {}, logger_type logger = {}
    ) :
        _executor(ex),
        _log(std::move(logger)),
        _stream_context(std::move(tls_context)),
        _stream(ex, _stream_context, _log),
        _replies(ex),
        _async_sender(*this),
        _active_span(_read_buff.cbegin(), _read_buff.cbegin()),
        _rec_channel(ex, (std::numeric_limits<size_t>::max)()),
        _ping_timer(ex),
        _sentry_timer(ex)
    {}

    executor_type get_executor() const noexcept {
        return _executor;
    }

    auto dup() const {
        return std::shared_ptr<client_service>(new client_service(*this));
    }

    template <
        typename Ctx = TlsContext,
        std::enable_if_t<!std::is_same_v<Ctx, std::monostate>, bool> = true
    >
    decltype(auto) tls_context() {
        return _stream_context.tls_context();
    }

    void will(will will) {
        if (!is_open())
            _stream_context.will(std::move(will));
    }

    void credentials(
        std::string client_id,
        std::string username = "", std::string password = ""
    ) {
        if (!is_open())
            _stream_context.credentials(
                std::move(client_id),
                std::move(username), std::move(password)
            );
    }

    void brokers(std::string hosts, uint16_t default_port) {
        if (!is_open())
            _stream.brokers(std::move(hosts), default_port);
    }

    template <
        typename Authenticator,
        std::enable_if_t<is_authenticator<Authenticator>, bool> = true
    >
    void authenticator(Authenticator&& authenticator) {
        if (!is_open())
            _stream_context.authenticator(
                std::forward<Authenticator>(authenticator)
            );
    }

    uint16_t negotiated_keep_alive() const {
        return connack_property(prop::server_keep_alive)
            .value_or(_stream_context.mqtt_context().keep_alive);
    }

    size_t max_packet_size() const {
        return connack_property(prop::maximum_packet_size)
            .value_or(default_max_send_size);
    }

    void keep_alive(uint16_t seconds) {
        if (!is_open())
            _stream_context.mqtt_context().keep_alive = seconds;
    }

    template <prop::property_type p>
    const auto& connect_property(
        std::integral_constant<prop::property_type, p> prop
    ) const {
        return _stream_context.connect_property(prop);
    }

    template <prop::property_type p>
    void connect_property(
        std::integral_constant<prop::property_type, p> prop,
        prop::value_type_t<p> value
    ){
        if (!is_open())
            _stream_context.connect_property(prop) = value;
    }

    void connect_properties(connect_props props) {
        if (!is_open())
            _stream_context.connect_properties(std::move(props));
    }

    template <prop::property_type p>
    const auto& connack_property(
        std::integral_constant<prop::property_type, p> prop
    ) const {
        return _stream_context.connack_property(prop);
    }

    const auto& connack_properties() const {
        return _stream_context.connack_properties();
    }

    void open_stream() {
        _stream.open();
    }

    bool is_open() const {
        return _stream.is_open();
    }

    bool was_connected() const {
        return _stream.was_connected();
    }

    template <typename CompletionToken>
    decltype(auto) async_shutdown(CompletionToken&& token) {
        return _stream.async_shutdown(std::forward<CompletionToken>(token));
    }

    void cancel() {
        if (!_stream.is_open()) return;

        _ping_timer.cancel();
        _sentry_timer.cancel();

        _rec_channel.close();
        _replies.cancel_unanswered();
        _async_sender.cancel();
        _stream.cancel();
        _stream.close();
    }

    log_invoke<LoggerType>& log() {
        return _log;
    }

    uint16_t allocate_pid() {
        return _pid_allocator.allocate();
    }

    void free_pid(uint16_t pid, bool was_throttled = false) {
        _pid_allocator.free(pid);
        if (was_throttled)
            _async_sender.throttled_op_done();
    }

    serial_num_t next_serial_num() {
        return _async_sender.next_serial_num();
    }

    bool subscriptions_present() const {
        return _stream_context.session_state().subscriptions_present();
    }

    void subscriptions_present(bool present) {
        _stream_context.session_state().subscriptions_present(present);
    }

    void update_session_state() {
        auto& session_state = _stream_context.session_state();

        if (!session_state.session_present()) {
            _replies.clear_pending_pubrels();
            session_state.session_present(true);

            if (session_state.subscriptions_present()) {
                channel_store_error(client::error::session_expired);
                session_state.subscriptions_present(false);
            }
        }

        _ping_timer.cancel();
    }

    bool channel_store(decoders::publish_message message) {
        auto& [topic, packet_id, flags, props, payload] = message;
        return _rec_channel.try_send(
            error_code {}, std::move(topic),
            std::move(payload), std::move(props)
        );
    }

    bool channel_store_error(error_code ec) {
        return _rec_channel.try_send(
            ec, std::string {}, std::string {}, publish_props {}
        );
    }

    template <typename BufferType, typename CompletionToken>
    decltype(auto) async_send(
        const BufferType& buffer,
        serial_num_t serial_num, unsigned flags,
        CompletionToken&& token
    ) {
        return _async_sender.async_send(
            buffer, serial_num, flags, std::forward<CompletionToken>(token)
        );
    }

    template <typename CompletionToken>
    decltype(auto) async_assemble(CompletionToken&& token) {
        using Signature = void (error_code, uint8_t, byte_citer, byte_citer);

        auto initiation = [] (
            auto handler, self_type& self,
            std::string& read_buff, data_span& active_span
        ) {
            assemble_op {
                self, std::move(handler), read_buff, active_span
            }.perform(asio::transfer_at_least(0));
        };

        return asio::async_initiate<CompletionToken, Signature> (
            initiation, token, std::ref(*this),
            std::ref(_read_buff), std::ref(_active_span)
        );
    }

    template <typename CompletionToken>
    decltype(auto) async_wait_reply(
        control_code_e code, uint16_t packet_id, CompletionToken&& token
    ) {
        return _replies.async_wait_reply(
            code, packet_id, std::forward<CompletionToken>(token)
        );
    }

    template <typename CompletionToken>
    decltype(auto) async_channel_receive(CompletionToken&& token) {
        return _rec_channel.async_receive(std::forward<CompletionToken>(token));
    }

};

} // namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_CLIENT_SERVICE_HPP
