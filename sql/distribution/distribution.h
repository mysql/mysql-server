#pragma once

#include <string>

enum class DistributionFunc {
  EQUAL,
  PUSH_UP,
  PUSH_DOWN,
  CARDINALITYBASED
};

inline std::string DistributionFuncToString(DistributionFunc distributionFunc) {
  switch (distributionFunc) {
    case DistributionFunc::EQUAL:
      return "EQUAL";
    case DistributionFunc::PUSH_UP:
      return "PUSH_UP";
    case DistributionFunc::PUSH_DOWN:
      return "PUSH_DOWN";
    case DistributionFunc::CARDINALITYBASED:
      return "CARDINALITYBASED";
    default:
      return "UNKOWN";
  }
}