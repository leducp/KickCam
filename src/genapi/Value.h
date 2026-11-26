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

    // The table entry whose key is the index's current value; fallback without an index or a matching key.
    template<typename Value, typename Table>
    std::expected<Value*, std::error_code> select(std::optional<IntValue> const& index, Table& table, Value& fallback)
    {
        if (not index)
        {
            return &fallback;
        }
        auto key = index->get();
        if (not key)
        {
            return std::unexpected(key.error());
        }
        for (auto& [entry_key, entry] : table)
        {
            if (entry_key == *key)
            {
                return &entry;
            }
        }
        return &fallback;
    }
}

#endif
