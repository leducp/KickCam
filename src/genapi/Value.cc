#include <limits>

#include "genapi/Number.h"
#include "genapi/Value.h"

namespace kickcam::genapi
{
    std::expected<int64_t, std::error_code> IntValue::get() const
    {
        return std::visit<std::expected<int64_t, std::error_code>>(Overloaded
        {
            [](int64_t constant) { return constant; },
            [](Integer* integer) { return integer->get(); },
            [](Enumeration* enumeration) { return enumeration->get_int(); },
            [](Boolean* boolean) { return boolean->get(); },
            [](Float* real) { return real->get().transform(saturate); },
        }, source);
    }

    std::expected<void, std::error_code> IntValue::set(int64_t value)
    {
        return std::visit<std::expected<void, std::error_code>>(Overloaded
        {
            [value](int64_t& constant)
            {
                constant = value;
                return std::expected<void, std::error_code>{};
            },
            [value](Integer* integer) { return integer->set(value); },
            [value](Enumeration* enumeration) { return enumeration->set_int(value); },
            [value](Boolean* boolean) { return boolean->set(value != 0); },
            [value](Float* real) { return real->set(static_cast<double>(value)); },
        }, source);
    }

    std::expected<Access, std::error_code> IntValue::access() const
    {
        return std::visit<std::expected<Access, std::error_code>>(Overloaded
        {
            [](int64_t) { return Access::RW; },
            [](Node const* node) { return node->access(); },
        }, source);
    }

    std::expected<double, std::error_code> FloatValue::get() const
    {
        return std::visit<std::expected<double, std::error_code>>(Overloaded
        {
            [](double constant) { return constant; },
            [](Float* real) { return real->get(); },
            [](Integer* integer) { return integer->get(); },
            [](Enumeration* enumeration) { return enumeration->get_int(); },
        }, source);
    }

    std::expected<void, std::error_code> FloatValue::set(double value)
    {
        return std::visit<std::expected<void, std::error_code>>(Overloaded
        {
            [value](double& constant)
            {
                constant = value;
                return std::expected<void, std::error_code>{};
            },
            [value](Float* real) { return real->set(value); },
            [value](Integer* integer) { return integer->set(saturate(value)); },
            [value](Enumeration* enumeration) { return enumeration->set_int(saturate(value)); },
        }, source);
    }

    std::expected<Access, std::error_code> FloatValue::access() const
    {
        return std::visit<std::expected<Access, std::error_code>>(Overloaded
        {
            [](double) { return Access::RW; },
            [](Node const* node) { return node->access(); },
        }, source);
    }

    std::expected<int64_t, std::error_code> IntValue::min() const
    {
        return std::visit<std::expected<int64_t, std::error_code>>(Overloaded
        {
            [](Integer* integer) { return integer->min(); },
            [](Float* real) { return real->min().transform(saturate); },
            [](auto) { return std::numeric_limits<int64_t>::min(); },
        }, source);
    }

    std::expected<int64_t, std::error_code> IntValue::max() const
    {
        return std::visit<std::expected<int64_t, std::error_code>>(Overloaded
        {
            [](Integer* integer) { return integer->max(); },
            [](Float* real) { return real->max().transform(saturate); },
            [](auto) { return std::numeric_limits<int64_t>::max(); },
        }, source);
    }

    std::expected<int64_t, std::error_code> IntValue::inc() const
    {
        return std::visit<std::expected<int64_t, std::error_code>>(Overloaded
        {
            [](Integer* integer) { return integer->inc(); },
            [](auto) { return int64_t{1}; },
        }, source);
    }

    std::expected<double, std::error_code> FloatValue::min() const
    {
        return std::visit<std::expected<double, std::error_code>>(Overloaded
        {
            [](Float* real) { return real->min(); },
            [](Integer* integer) { return integer->min(); },
            [](auto) { return std::numeric_limits<double>::lowest(); },
        }, source);
    }

    std::expected<double, std::error_code> FloatValue::max() const
    {
        return std::visit<std::expected<double, std::error_code>>(Overloaded
        {
            [](Float* real) { return real->max(); },
            [](Integer* integer) { return integer->max(); },
            [](auto) { return std::numeric_limits<double>::max(); },
        }, source);
    }

    std::expected<std::optional<double>, std::error_code> FloatValue::inc() const
    {
        return std::visit<std::expected<std::optional<double>, std::error_code>>(Overloaded
        {
            [](Float* real) { return real->inc(); },
            [](Integer* integer) { return integer->inc(); },
            [](auto) { return std::nullopt; },
        }, source);
    }
}
