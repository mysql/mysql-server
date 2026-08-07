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

#ifndef BOOST_MSM_BACKMP11_SERIALIZATION_NLOHMANN_JSON_HPP
#define BOOST_MSM_BACKMP11_SERIALIZATION_NLOHMANN_JSON_HPP

#include <stack>

#include <nlohmann/json.hpp>

#include <boost/msm/backmp11/state_machine.hpp>

namespace boost::msm::backmp11::serialization
{

// Serializer for nlohmann json.
class nlohmann_json_serializer
{
    using json = nlohmann::json;

  public:
    nlohmann_json_serializer(json& json) : m_json({&json}) {}

    template <typename FrontEnd>
    void visit_front_end(FrontEnd&& front_end)
    {
        if constexpr (!std::is_empty_v<std::decay_t<FrontEnd>>)
        {
            top()["front_end"] = front_end;
        }
    }

    template <typename FrontEnd, typename Reflect>
    void visit_front_end(FrontEnd&& /*front_end*/, Reflect&& reflect)
    {
        auto& json_front_end = top()["front_end"];
        m_json.push(&json_front_end);
        reflect();
        m_json.pop();
    }

    template <typename Member>
    void visit_member(const char* key, Member&& member)
    {
        top()[key] = member;
    }

    template <typename State>
    void visit_state(size_t state_id, State&& state)
    {
        if constexpr (!std::is_empty_v<std::decay_t<State>>)
        {
            top()["states"][std::to_string(state_id)] = state;
        }
    }

    template <typename State, typename Reflect>
    void visit_state(size_t state_id, State&& /*state*/, Reflect&& reflect)
    {
        auto& json_state = top()["states"][std::to_string(state_id)];
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

// Deserializer for nlohmann json.
class nlohmann_json_deserializer
{
    using json = nlohmann::json;

  public:
    nlohmann_json_deserializer(const json& json) : m_json({&json}) {}

    template <typename FrontEnd>
    void visit_front_end(FrontEnd&& front_end)
    {
        if constexpr (!std::is_empty_v<std::decay_t<FrontEnd>>)
        {
            front_end = top()["front_end"];
        }
    }

    template <typename FrontEnd, typename Reflect>
    void visit_front_end(FrontEnd&& /*front_end*/, Reflect&& reflect)
    {
        auto& json_state = top().at("front_end");
        m_json.push(&json_state);
        reflect();
        m_json.pop();
    }

    template <typename Member>
    void visit_member(const char* key, Member&& member)
    {
        member = top().at(key);
    }

    template <typename State>
    void visit_state(size_t state_id, State&& state)
    {
        if constexpr (!std::is_empty_v<std::decay_t<State>>)
        {
            state = top()["states"][std::to_string(state_id)];
        }
    }

    template <typename State, typename Reflect>
    void visit_state(size_t state_id, State& /*state*/, Reflect&& reflect)
    {
        auto& json_state = top().at("states").at(std::to_string(state_id));
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

namespace boost::msm::backmp11::detail
{

template <typename StateMachine,
          typename = std::enable_if_t<is_state_machine_v<StateMachine>>>
void to_json(nlohmann::json& json, const StateMachine& state_machine)
{
    reflect(state_machine, serialization::nlohmann_json_serializer{json});
}

template <typename StateMachine,
          typename = std::enable_if_t<is_state_machine_v<StateMachine>>>
void from_json(const nlohmann::json& json, StateMachine& state_machine)
{
    reflect(state_machine, serialization::nlohmann_json_deserializer{json});
}

} // namespace boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_SERIALIZATION_NLOHMANN_JSON_HPP
