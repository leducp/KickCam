#ifndef KICKCAM_GENAPI_VARIABLES_H
#define KICKCAM_GENAPI_VARIABLES_H

#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "kickcam/genapi/Node.h"
#include "genapi/Formula.h"

namespace kickcam::genapi
{
    // The names a SwissKnife or Converter formula uses: its pVariable, Constant and Expression elements.
    struct Variables
    {
        enum class Property
        {
            Value,
            Min,
            Max,
            Inc,
        };

        // A sub-formula, evaluated where its name is used.
        struct Expression
        {
            std::size_t index;
        };

        struct Variable
        {
            std::string name;
            std::variant<int64_t, double, Integer*, Float*, Enumeration*, Boolean*, EnumEntry const*, Expression> source{int64_t{0}};
            Property property{Property::Value};
        };

        // The index of a name; TO and FROM of a converter give ARGUMENT.
        std::optional<std::size_t> find(std::string_view name) const;

        // ARGUMENT evaluates to argument, the value a converter converts.
        template<typename T>
        std::expected<T, std::error_code> evaluate(Formula const& formula, T argument) const;

        static constexpr std::size_t ARGUMENT = std::numeric_limits<std::size_t>::max();

        std::vector<Variable> variables;
        std::vector<Formula> expressions;
        bool converter{false};
    };
}

#endif
