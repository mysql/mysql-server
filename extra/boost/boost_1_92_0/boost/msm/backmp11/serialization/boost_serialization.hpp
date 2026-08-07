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

#ifndef BOOST_MSM_BACKMP11_SERIALIZATION_BOOST_SERIALIZATION_HPP
#define BOOST_MSM_BACKMP11_SERIALIZATION_BOOST_SERIALIZATION_HPP

#include <boost/serialization/array.hpp>

#include <boost/msm/backmp11/state_machine.hpp>

namespace boost::msm::backmp11::serialization
{

// Serializer for Boost.Serialization.
template <typename Archive>
class boost_serialization_serializer
{
  public:
    boost_serialization_serializer(Archive& archive) : m_archive(archive) {}

    template <typename FrontEnd>
    void visit_front_end(FrontEnd&& front_end)
    {
        if constexpr (!std::is_empty_v<std::decay_t<FrontEnd>>)
        {
            m_archive & front_end;
        }
    }

    template <typename FrontEnd, typename Reflect>
    void visit_front_end(FrontEnd&& /*front_end*/, Reflect&& reflect)
    {
        reflect();
    }

    template <typename Member>
    void visit_member(const char* /*key*/, Member&& member)
    {
        m_archive & member;
    }

    template <typename State>
    void visit_state(size_t /*state_id*/, State&& state)
    {
        if constexpr (!std::is_empty_v<std::decay_t<State>>)
        {
            m_archive & state;
        }
    }

    template <typename State, typename Reflect>
    void visit_state(size_t /*state_id*/, State&& /*state*/, Reflect&& reflect)
    {
        reflect();
    }

  private:
    Archive& m_archive;
};

} // namespace boost::msm::backmp11::serialization

#endif // BOOST_MSM_BACKMP11_SERIALIZATION_BOOST_SERIALIZATION_HPP
