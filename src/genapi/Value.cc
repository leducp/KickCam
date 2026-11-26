#include <cmath>
#include <limits>

#include "genapi/Value.h"

namespace kickcam::genapi
{
    int64_t saturate(double value)
    {
        if (std::isnan(value))
        {
            return 0;
        }
        if (value >= 0x1p63)
        {
            return std::numeric_limits<int64_t>::max();
        }
        if (value < -0x1p63)
        {
            return std::numeric_limits<int64_t>::min();
        }
        return static_cast<int64_t>(value);
    }

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
}
