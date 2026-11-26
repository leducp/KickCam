#ifndef KICKCAM_GENAPI_FORMULA_H
#define KICKCAM_GENAPI_FORMULA_H

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace kickcam::genapi
{
    // A SwissKnife or Converter formula. Evaluation reads only the variables of the branches it takes.
    class Formula
    {
    public:
        // resolve gives the index of a name the formula may use, or nothing for an unknown name.
        // The error names the problem and where it is in the text.
        static std::expected<Formula, std::string> parse(std::string_view text,
                                                         std::function<std::optional<std::size_t>(std::string_view)> const& resolve);

        // int64_t for IntSwissKnife and IntConverter: arithmetic wraps on overflow. double for SwissKnife and Converter.
        template<typename T>
        std::expected<T, std::error_code> evaluate(std::function<std::expected<T, std::error_code>(std::size_t)> const& variable) const;

    private:
        class Builder;

        enum class Op : uint8_t
        {
            // No operand
            Number, Variable,

            // One operand
            Negate, BitNot, Sgn, Neg, Abs, Sqrt, Trunc, Floor, Ceil, Round, Exp, Ln, Lg, Sin, Cos, Tan, Asin, Acos, Atan,

            // Two operands
            Add, Subtract, Multiply, Divide, Remainder, Power, ShiftLeft, ShiftRight, BitAnd, BitOr, BitXor,
            Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual, RoundTo,

            // Operands evaluated only when needed
            And, Or, Choose,
        };

        struct Term
        {
            Op op;
            uint32_t a{0};             // operands: index of an earlier term
            uint32_t b{0};
            uint32_t c{0};
            std::size_t variable{0};
            int64_t integer{0};        // Number, in each mode
            double real{0.0};
        };

        template<typename T>
        std::expected<T, std::error_code> eval(uint32_t index, std::function<std::expected<T, std::error_code>(std::size_t)> const& variable) const;

        template<typename T>
        static T unary(Op op, T x);

        template<typename T>
        static std::expected<T, std::error_code> binary(Op op, T a, T b);

        std::vector<Term> terms_; // the last term is the root
    };
}

#endif
