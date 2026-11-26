#ifndef KICKCAM_GENAPI_VALUE_H
#define KICKCAM_GENAPI_VALUE_H

#include <variant>

#include "kickcam/genapi/Node.h"

namespace kickcam::genapi
{
    // A visitor made of lambdas, for std::visit.
    template<typename... Lambdas>
    struct Overloaded : Lambdas...
    {
        using Lambdas::operator()...;
    };

    // Clamps to the int64 range; NaN gives 0.
    int64_t saturate(double value);

    // A constant, or the value of an Integer, Enumeration, Boolean or Float node.
    struct IntValue
    {
        std::expected<int64_t, std::error_code> get() const;
        std::expected<void, std::error_code> set(int64_t value);
        std::expected<Access, std::error_code> access() const;

        std::variant<int64_t, Integer*, Enumeration*, Boolean*, Float*> source{int64_t{0}};
    };

    // A constant, or the value of a Float, Integer or Enumeration node.
    struct FloatValue
    {
        std::expected<double, std::error_code> get() const;
        std::expected<void, std::error_code> set(double value);
        std::expected<Access, std::error_code> access() const;

        std::variant<double, Float*, Integer*, Enumeration*> source{0.0};
    };
}

#endif
