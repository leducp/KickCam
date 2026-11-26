#include <algorithm>
#include <type_traits>
#include <utility>

#include "genapi/Number.h"
#include "genapi/Value.h"
#include "genapi/Variables.h"

namespace kickcam::genapi
{
    namespace
    {
        std::expected<int64_t, std::error_code> read(Integer const& node, Variables::Property property)
        {
            switch (property)
            {
                case Variables::Property::Value: { return node.get(); }
                case Variables::Property::Min:   { return node.min(); }
                case Variables::Property::Max:   { return node.max(); }
                case Variables::Property::Inc:   { return node.inc(); }
            }
            std::unreachable();
        }

        // A float without an increment has an Inc of 0.
        std::expected<double, std::error_code> read(Float const& node, Variables::Property property)
        {
            switch (property)
            {
                case Variables::Property::Value: { return node.get(); }
                case Variables::Property::Min:   { return node.min(); }
                case Variables::Property::Max:   { return node.max(); }
                case Variables::Property::Inc:   { return node.inc().transform([](std::optional<double> step) { return step.value_or(0.0); }); }
            }
            std::unreachable();
        }

        // An integer formula saturates the floats it reads.
        template<typename T>
        std::expected<T, std::error_code> narrow(std::expected<double, std::error_code> value)
        {
            if constexpr (std::is_integral_v<T>)
            {
                return value.transform(saturate);
            }
            else
            {
                return value;
            }
        }
    }

    std::optional<std::size_t> Variables::find(std::string_view name) const
    {
        if (converter and (name == "TO" or name == "FROM"))
        {
            return ARGUMENT;
        }
        auto it = std::ranges::find(variables, name, &Variable::name);
        if (it == variables.end())
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(it - variables.begin());
    }

    template<typename T>
    std::expected<T, std::error_code> Variables::evaluate(Formula const& formula, T argument) const
    {
        std::function<std::expected<T, std::error_code>(std::size_t)> value;
        value = [&](std::size_t index) -> std::expected<T, std::error_code>
        {
            if (index == ARGUMENT)
            {
                return argument;
            }
            Variable const& variable = variables[index];
            return std::visit<std::expected<T, std::error_code>>(Overloaded
            {
                [](int64_t constant) { return static_cast<T>(constant); },
                [](double constant) { return narrow<T>(constant); },
                [&](Integer* node) { return read(*node, variable.property); },
                [&](Float* node) { return narrow<T>(read(*node, variable.property)); },
                [](Enumeration* node) { return node->get_int(); },
                [](Boolean* node) { return node->get(); },
                [](EnumEntry const* entry) { return entry->value(); },
                [&](Expression expression) { return expressions[expression.index].evaluate<T>(value); },
            }, variable.source);
        };
        return formula.evaluate<T>(value);
    }

    template std::expected<int64_t, std::error_code> Variables::evaluate<int64_t>(Formula const&, int64_t) const;
    template std::expected<double, std::error_code> Variables::evaluate<double>(Formula const&, double) const;
}
