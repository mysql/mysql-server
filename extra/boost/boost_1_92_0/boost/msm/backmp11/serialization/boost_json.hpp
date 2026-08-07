// Copyright 2026 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_SERIALIZATION_BOOST_JSON_HPP
#define BOOST_MSM_BACKMP11_SERIALIZATION_BOOST_JSON_HPP

#include <stack>

#include <boost/json.hpp>

#include <boost/msm/backmp11/state_machine.hpp>

namespace boost::msm::backmp11::serialization
{

// Serializer for Boost.JSON.
class boost_json_serializer
{
    using json = boost::json::object;

  public:
    boost_json_serializer(boost::json::value& json)
        : m_json({&json.emplace_object()})
    {
    }

    template <typename FrontEnd>
    void visit_front_end(FrontEnd&& front_end)
    {
        if constexpr (!std::is_empty_v<std::decay_t<FrontEnd>>)
        {
            top()["front_end"] = boost::json::value_from(front_end);
        }
    }

    template <typename FrontEnd, typename Reflect>
    void visit_front_end(FrontEnd&& /*front_end*/, Reflect&& reflect)
    {
        auto& json_front_end = top()["front_end"].emplace_object();
        m_json.push(&json_front_end);
        reflect();
        m_json.pop();
    }

    template <typename Member>
    void visit_member(const char* key, Member&& member)
    {
        top()[key] = boost::json::value_from(member);
    }

    template <typename T, std::size_t N>
    void visit_member(const char* key, const std::array<T, N>& member)
    {
        boost::json::array& arr = top()[key].emplace_array();
        for (const auto& elem : member)
        {
            arr.push_back(elem);
        }
    }

    template <typename State>
    void visit_state(size_t state_id, State&& state)
    {
        if constexpr (!std::is_empty_v<std::decay_t<State>>)
        {
            if (!top().contains("states"))
            {
                top()["states"].emplace_object();
            }
            top()["states"].as_object()[std::to_string(state_id)] =
                boost::json::value_from(state);
        }
    }

    template <typename State, typename Reflect>
    void visit_state(size_t state_id, State&& /*state*/, Reflect&& reflect)
    {
        if (!top().contains("states"))
        {
            top()["states"].emplace_object();
        }
        auto& json_state = top()["states"]
                               .as_object()[std::to_string(state_id)]
                               .emplace_object();
        m_json.push(&json_state);
        reflect();
        m_json.pop();
    }

  private:
    json& top()
    {
        return *m_json.top();
    }

    std::stack<json*> m_json;
};

// Deserializer for Boost.JSON.
class boost_json_deserializer
{
    using json = boost::json::object;

  public:
    boost_json_deserializer(const boost::json::value& json)
        : m_json({&json.as_object()})
    {
    }

    template <typename FrontEnd>
    void visit_front_end(FrontEnd&& front_end)
    {
        if constexpr (!std::is_empty_v<std::decay_t<FrontEnd>>)
        {
            front_end = boost::json::value_to<std::decay_t<FrontEnd>>(
                top().at("front_end"));
        }
    }

    template <typename FrontEnd, typename Reflect>
    void visit_front_end(FrontEnd&& /*front_end*/, Reflect&& reflect)
    {
        auto& json_state = top().at("front_end").as_object();
        m_json.push(&json_state);
        reflect();
        m_json.pop();
    }

    template <typename Member>
    void visit_member(const char* key, Member&& member)
    {
        member = boost::json::value_to<std::decay_t<Member>>(top().at(key));
    }

    template <typename T, std::size_t N>
    void visit_member(const char* key, std::array<T, N>& member)
    {
        const auto& arr = top().at(key).as_array();
        for (std::size_t i = 0; i < N; ++i)
            member[i] = boost::json::value_to<T>(arr[i]);
    }

    template <typename State>
    void visit_state(size_t state_id, State&& state)
    {
        if constexpr (!std::is_empty_v<std::decay_t<State>>)
        {
            state = boost::json::value_to<std::decay_t<State>>(
                top().at("states").at(std::to_string(state_id)));
        }
    }

    template <typename State, typename Reflect>
    void visit_state(size_t state_id, State& /*state*/, Reflect&& reflect)
    {
        auto& json_state =
            top().at("states").at(std::to_string(state_id)).as_object();
        m_json.push(&json_state);
        reflect();
        m_json.pop();
    }

  private:
    const json& top()
    {
        return *m_json.top();
    }

    std::stack<const json*> m_json;
};

} // namespace boost::msm::backmp11::serialization

namespace boost::msm::backmp11
{

inline void tag_invoke(const boost::json::value_from_tag&,
                       boost::json::value& json, const machine_state& state)
{
    json = static_cast<std::underlying_type_t<machine_state>>(state);
}

inline machine_state tag_invoke(const boost::json::value_to_tag<machine_state>&,
                                const boost::json::value& json)
{
    return static_cast<machine_state>(
        json.to_number<std::underlying_type_t<machine_state>>());
}

template <typename StateMachine,
          typename = std::enable_if_t<detail::is_state_machine_v<StateMachine>>>
void tag_invoke(const boost::json::value_from_tag&, boost::json::value& json,
                const StateMachine& sm)
{
    reflect(sm, serialization::boost_json_serializer{json});
}

template <typename StateMachine,
          typename = std::enable_if_t<detail::is_state_machine_v<StateMachine>>>
StateMachine tag_invoke(const boost::json::value_to_tag<StateMachine>&,
                        const boost::json::value& json)
{
    StateMachine state_machine;
    reflect(state_machine,
                    serialization::boost_json_deserializer{json});
    return state_machine;
}

} // namespace boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_SERIALIZATION_BOOST_JSON_HPP
