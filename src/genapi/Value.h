#ifndef KICKCAM_GENAPI_VALUE_H
#define KICKCAM_GENAPI_VALUE_H

#include <optional>
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

    // A constant, or the value of an Integer, Enumeration, Boolean or Float node.
    struct IntValue
    {
        std::expected<int64_t, std::error_code> get() const;
        std::expected<void, std::error_code> set(int64_t value);
        std::expected<Access, std::error_code> access() const;

        // The bounds of an Integer or Float node; the full int64 range and 1 otherwise.
        std::expected<int64_t, std::error_code> min() const;
        std::expected<int64_t, std::error_code> max() const;
        std::expected<int64_t, std::error_code> inc() const;

        std::variant<int64_t, Integer*, Enumeration*, Boolean*, Float*> source{int64_t{0}};
    };

    // A constant, or the value of a Float, Integer or Enumeration node.
    struct FloatValue
    {
        std::expected<double, std::error_code> get() const;
        std::expected<void, std::error_code> set(double value);
        std::expected<Access, std::error_code> access() const;

        // The bounds of a Float or Integer node; the full double range and no increment otherwise.
        std::expected<double, std::error_code> min() const;
        std::expected<double, std::error_code> max() const;
        std::expected<std::optional<double>, std::error_code> inc() const;

        std::variant<double, Float*, Integer*, Enumeration*> source{0.0};
    };
}

#endif
