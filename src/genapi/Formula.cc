#include <algorithm>
#include <array>
#include <cmath>
#include <locale>
#include <numbers>
#include <type_traits>
#include <utility>

#include "kickcam/Error.h"
#include "genapi/Formula.h"
#include "genapi/Number.h"

namespace kickcam::genapi
{
    namespace
    {
        // The grammar is ASCII; <cctype> would depend on the locale.
        constexpr std::string_view DIGITS = "0123456789";
        constexpr std::string_view HEX_DIGITS = "0123456789abcdefABCDEF";
        constexpr std::string_view LETTERS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
        constexpr std::string_view NAME = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789.";
        constexpr std::string_view SPACES = " \t\r\n";

        bool same_letters(std::string_view a, std::string_view b)
        {
            return std::ranges::equal(a, b, [](char x, char y)
            {
                return std::toupper(x, std::locale::classic()) == std::toupper(y, std::locale::classic());
            });
        }

        int64_t negate(int64_t x)
        {
            return static_cast<int64_t>(uint64_t{0} - static_cast<uint64_t>(x));
        }

        // Rounds half away from zero to a multiple of 10^-digits; integers have no decimals to round.
        int64_t round_to(int64_t value, int64_t digits)
        {
            if (digits >= 0)
            {
                return value;
            }
            if (digits < -18)
            {
                return 0;
            }
            int64_t scale = 1;
            for (int64_t i = 0; i > digits; --i)
            {
                scale *= 10;
            }
            int64_t quotient = value / scale;
            if (2 * std::abs(value % scale) >= scale)
            {
                if (value < 0)
                {
                    --quotient;
                }
                else
                {
                    ++quotient;
                }
            }
            return static_cast<int64_t>(static_cast<uint64_t>(quotient) * static_cast<uint64_t>(scale));
        }

        std::expected<int64_t, std::error_code> power(int64_t base, int64_t exponent)
        {
            if (exponent < 0)
            {
                if (base == 0)
                {
                    return make_unexpected(Errc::division_by_zero);
                }
                if (base == 1)
                {
                    return 1;
                }
                if (base == -1)
                {
                    if (exponent % 2 == 0)
                    {
                        return 1;
                    }
                    return -1;
                }
                return 0;
            }
            uint64_t result = 1;
            uint64_t factor = static_cast<uint64_t>(base);
            for (; exponent > 0; exponent >>= 1)
            {
                if (exponent & 1)
                {
                    result *= factor;
                }
                factor *= factor;
            }
            return static_cast<int64_t>(result);
        }
    }

    class Formula::Builder
    {
    public:
        Builder(std::string_view text, std::function<std::optional<std::size_t>(std::string_view)> const& resolve)
            : text_{text}
            , resolve_{resolve}
        {
        }

        std::expected<Formula, std::string> build()
        {
            auto root = ternary();
            if (not root)
            {
                return std::unexpected(root.error());
            }
            skip(SPACES);
            if (pos_ < text_.size())
            {
                return std::unexpected(unexpected());
            }
            return std::move(formula_);
        }

    private:
        struct BinaryOperator
        {
            std::string_view text;
            Op op;
            int precedence;
            bool right_to_left;
        };

        // Two-character operators come first, so that "<=" is not read as "<".
        static constexpr std::array<BinaryOperator, 19> BINARY
        {{
            {"||", Op::Or, 1, false},
            {"&&", Op::And, 2, false},
            {"<>", Op::NotEqual, 6, false},
            {"<=", Op::LessEqual, 7, false},
            {">=", Op::GreaterEqual, 7, false},
            {"<<", Op::ShiftLeft, 8, false},
            {">>", Op::ShiftRight, 8, false},
            {"**", Op::Power, 11, true},
            {"|", Op::BitOr, 3, false},
            {"^", Op::BitXor, 4, false},
            {"&", Op::BitAnd, 5, false},
            {"=", Op::Equal, 6, false},
            {"<", Op::Less, 7, false},
            {">", Op::Greater, 7, false},
            {"+", Op::Add, 9, false},
            {"-", Op::Subtract, 9, false},
            {"*", Op::Multiply, 10, false},
            {"/", Op::Divide, 10, false},
            {"%", Op::Remainder, 10, false},
        }};

        struct Function
        {
            std::string_view name;
            Op op;
        };

        static constexpr std::array<Function, 17> FUNCTIONS
        {{
            {"SGN", Op::Sgn}, {"NEG", Op::Neg}, {"ABS", Op::Abs}, {"SQRT", Op::Sqrt}, {"TRUNC", Op::Trunc},
            {"FLOOR", Op::Floor}, {"CEIL", Op::Ceil}, {"ROUND", Op::Round}, {"EXP", Op::Exp}, {"LN", Op::Ln},
            {"LG", Op::Lg}, {"SIN", Op::Sin}, {"COS", Op::Cos}, {"TAN", Op::Tan}, {"ASIN", Op::Asin},
            {"ACOS", Op::Acos}, {"ATAN", Op::Atan},
        }};

        uint32_t add(Term term)
        {
            formula_.terms_.push_back(term);
            return static_cast<uint32_t>(formula_.terms_.size() - 1);
        }

        void skip(std::string_view set)
        {
            while (pos_ < text_.size() and set.contains(text_[pos_]))
            {
                ++pos_;
            }
        }

        std::string unexpected() const
        {
            if (pos_ >= text_.size())
            {
                return "unexpected end";
            }
            return "unexpected '" + std::string{text_[pos_]} + "' at offset " + std::to_string(pos_);
        }

        std::expected<void, std::string> expect(char c)
        {
            skip(SPACES);
            if (pos_ < text_.size() and text_[pos_] == c)
            {
                ++pos_;
                return {};
            }
            if (pos_ >= text_.size())
            {
                return std::unexpected("expected '" + std::string{c} + "' at the end");
            }
            return std::unexpected("expected '" + std::string{c} + "' at offset " + std::to_string(pos_));
        }

        // cond ? a : b, the lowest precedence, grouped from the right.
        std::expected<uint32_t, std::string> ternary()
        {
            auto condition = binary(0);
            if (not condition)
            {
                return condition;
            }
            skip(SPACES);
            if (pos_ >= text_.size() or text_[pos_] != '?')
            {
                return condition;
            }
            ++pos_;
            auto chosen = ternary();
            if (not chosen)
            {
                return chosen;
            }
            auto colon = expect(':');
            if (not colon)
            {
                return std::unexpected(colon.error());
            }
            auto other = ternary();
            if (not other)
            {
                return other;
            }
            return add({.op = Op::Choose, .a = *condition, .b = *chosen, .c = *other});
        }

        // Precedence climbing: reads operators of at least min_precedence.
        std::expected<uint32_t, std::string> binary(int min_precedence)
        {
            auto lhs = unary();
            while (lhs)
            {
                skip(SPACES);
                auto found = std::ranges::find_if(BINARY, [this](BinaryOperator const& candidate)
                {
                    return text_.substr(pos_).starts_with(candidate.text);
                });
                if (found == BINARY.end() or found->precedence < min_precedence)
                {
                    break;
                }
                pos_ += found->text.size();
                int next = found->precedence + 1;
                if (found->right_to_left)
                {
                    next = found->precedence;
                }
                auto rhs = binary(next);
                if (not rhs)
                {
                    return rhs;
                }
                lhs = add({.op = found->op, .a = *lhs, .b = *rhs});
            }
            return lhs;
        }

        // Unary operators bind tighter than every binary one: -2**2 is 4.
        std::expected<uint32_t, std::string> unary()
        {
            skip(SPACES);
            if (pos_ < text_.size() and text_[pos_] == '+')
            {
                ++pos_;
                return unary();
            }
            if (pos_ < text_.size() and (text_[pos_] == '-' or text_[pos_] == '~'))
            {
                Op op = Op::Negate;
                if (text_[pos_] == '~')
                {
                    op = Op::BitNot;
                }
                ++pos_;
                auto operand = unary();
                if (not operand)
                {
                    return operand;
                }
                return add({.op = op, .a = *operand});
            }
            return primary();
        }

        std::expected<uint32_t, std::string> primary()
        {
            skip(SPACES);
            if (pos_ >= text_.size())
            {
                return std::unexpected(unexpected());
            }
            char const c = text_[pos_];
            if (c == '(')
            {
                ++pos_;
                auto inner = ternary();
                if (not inner)
                {
                    return inner;
                }
                auto closed = expect(')');
                if (not closed)
                {
                    return std::unexpected(closed.error());
                }
                return inner;
            }
            if (DIGITS.contains(c))
            {
                return number();
            }
            if (LETTERS.contains(c))
            {
                return name();
            }
            return std::unexpected(unexpected());
        }

        // Decimal or 0x hexadecimal integers; a fraction or an exponent makes a float.
        std::expected<uint32_t, std::string> number()
        {
            std::size_t const start = pos_;
            bool is_real = false;
            if (text_.substr(pos_).starts_with("0x") or text_.substr(pos_).starts_with("0X"))
            {
                pos_ += 2;
                skip(HEX_DIGITS);
            }
            else
            {
                skip(DIGITS);
                if (pos_ < text_.size() and text_[pos_] == '.')
                {
                    is_real = true;
                    ++pos_;
                    skip(DIGITS);
                }
                if (pos_ < text_.size() and (text_[pos_] == 'e' or text_[pos_] == 'E'))
                {
                    std::size_t exponent = pos_ + 1;
                    if (exponent < text_.size() and (text_[exponent] == '+' or text_[exponent] == '-'))
                    {
                        ++exponent;
                    }
                    if (exponent < text_.size() and DIGITS.contains(text_[exponent]))
                    {
                        is_real = true;
                        pos_ = exponent;
                        skip(DIGITS);
                    }
                }
            }

            std::string_view const literal = text_.substr(start, pos_ - start);
            auto invalid = [&]
            {
                return std::unexpected("invalid number '" + std::string{literal} + "' at offset " + std::to_string(start));
            };
            Term term{.op = Op::Number};
            if (is_real)
            {
                auto value = to_double(literal);
                if (not value)
                {
                    return invalid();
                }
                term.real = *value;
                term.integer = saturate(*value);
                return add(term);
            }
            auto value = to_int(literal);
            if (not value)
            {
                return invalid();
            }
            term.integer = *value;
            term.real = static_cast<double>(*value);
            return add(term);
        }

        // A variable, PI or E, or a function call. A variable named PI or E hides the constant.
        std::expected<uint32_t, std::string> name()
        {
            std::size_t const start = pos_;
            skip(NAME);
            std::string_view const identifier = text_.substr(start, pos_ - start);
            skip(SPACES);
            if (pos_ < text_.size() and text_[pos_] == '(')
            {
                return call(identifier);
            }
            auto index = resolve_(identifier);
            if (index)
            {
                return add({.op = Op::Variable, .variable = *index});
            }
            if (identifier == "PI")
            {
                return add({.op = Op::Number, .integer = 3, .real = std::numbers::pi});
            }
            if (identifier == "E")
            {
                return add({.op = Op::Number, .integer = 2, .real = std::numbers::e});
            }
            return std::unexpected("unknown name '" + std::string{identifier} + "'");
        }

        // Function names ignore case. ROUND takes an optional number of decimals.
        std::expected<uint32_t, std::string> call(std::string_view identifier)
        {
            auto function = std::ranges::find_if(FUNCTIONS, [&](Function const& candidate)
            {
                return same_letters(candidate.name, identifier);
            });
            if (function == FUNCTIONS.end())
            {
                return std::unexpected("unknown function '" + std::string{identifier} + "'");
            }
            ++pos_;
            auto argument = ternary();
            if (not argument)
            {
                return argument;
            }
            skip(SPACES);
            if (function->op == Op::Round and pos_ < text_.size() and text_[pos_] == ',')
            {
                ++pos_;
                auto decimals = ternary();
                if (not decimals)
                {
                    return decimals;
                }
                auto closed = expect(')');
                if (not closed)
                {
                    return std::unexpected(closed.error());
                }
                return add({.op = Op::RoundTo, .a = *argument, .b = *decimals});
            }
            auto closed = expect(')');
            if (not closed)
            {
                return std::unexpected(closed.error());
            }
            return add({.op = function->op, .a = *argument});
        }

        std::string_view text_;
        std::size_t pos_{0};
        std::function<std::optional<std::size_t>(std::string_view)> const& resolve_;
        Formula formula_;
    };

    std::expected<Formula, std::string> Formula::parse(std::string_view text,
                                                       std::function<std::optional<std::size_t>(std::string_view)> const& resolve)
    {
        return Builder{text, resolve}.build();
    }

    template<typename T>
    std::expected<T, std::error_code> Formula::evaluate(std::function<std::expected<T, std::error_code>(std::size_t)> const& variable) const
    {
        return eval<T>(static_cast<uint32_t>(terms_.size() - 1), variable);
    }

    template<typename T>
    std::expected<T, std::error_code> Formula::eval(uint32_t index, std::function<std::expected<T, std::error_code>(std::size_t)> const& variable) const
    {
        Term const& term = terms_[index];
        if (term.op == Op::Number)
        {
            if constexpr (std::is_integral_v<T>)
            {
                return term.integer;
            }
            else
            {
                return term.real;
            }
        }
        if (term.op == Op::Variable)
        {
            return variable(term.variable);
        }

        auto first = eval<T>(term.a, variable);
        if (not first)
        {
            return first;
        }
        if (term.op == Op::Choose)
        {
            if (*first != 0)
            {
                return eval<T>(term.b, variable);
            }
            return eval<T>(term.c, variable);
        }
        bool const logic = term.op == Op::And or term.op == Op::Or;
        if (logic and ((*first == 0) == (term.op == Op::And)))
        {
            return static_cast<T>(term.op == Op::Or);
        }
        if (term.op < Op::Add)
        {
            return unary<T>(term.op, *first);
        }
        auto second = eval<T>(term.b, variable);
        if (not second)
        {
            return second;
        }
        if (logic)
        {
            return static_cast<T>(*second != 0);
        }
        return binary<T>(term.op, *first, *second);
    }

    template<typename T>
    T Formula::unary(Op op, T x)
    {
        if constexpr (std::is_integral_v<T>)
        {
            switch (op)
            {
                case Op::Negate:
                case Op::Neg:    { return negate(x); }
                case Op::BitNot: { return ~x; }
                case Op::Sgn:    { return static_cast<int64_t>(x > 0) - static_cast<int64_t>(x < 0); }
                case Op::Abs:
                {
                    if (x < 0)
                    {
                        return negate(x);
                    }
                    return x;
                }
                case Op::Trunc:
                case Op::Floor:
                case Op::Ceil:
                case Op::Round:  { return x; }
                default:         { return saturate(unary<double>(op, static_cast<double>(x))); }
            }
        }
        else
        {
            switch (op)
            {
                case Op::Negate:
                case Op::Neg:    { return -x; }
                case Op::BitNot: { return static_cast<double>(~saturate(x)); }
                case Op::Sgn:    { return static_cast<double>((x > 0) - (x < 0)); }
                case Op::Abs:    { return std::fabs(x); }
                case Op::Sqrt:   { return std::sqrt(x); }
                case Op::Trunc:  { return std::trunc(x); }
                case Op::Floor:  { return std::floor(x); }
                case Op::Ceil:   { return std::ceil(x); }
                case Op::Round:  { return std::round(x); }
                case Op::Exp:    { return std::exp(x); }
                case Op::Ln:     { return std::log(x); }
                case Op::Lg:     { return std::log10(x); }
                case Op::Sin:    { return std::sin(x); }
                case Op::Cos:    { return std::cos(x); }
                case Op::Tan:    { return std::tan(x); }
                case Op::Asin:   { return std::asin(x); }
                case Op::Acos:   { return std::acos(x); }
                case Op::Atan:   { return std::atan(x); }
                default:         { return x; }
            }
        }
    }

    template<typename T>
    std::expected<T, std::error_code> Formula::binary(Op op, T a, T b)
    {
        switch (op)
        {
            case Op::Equal:        { return static_cast<T>(a == b); }
            case Op::NotEqual:     { return static_cast<T>(a != b); }
            case Op::Less:         { return static_cast<T>(a < b); }
            case Op::LessEqual:    { return static_cast<T>(a <= b); }
            case Op::Greater:      { return static_cast<T>(a > b); }
            case Op::GreaterEqual: { return static_cast<T>(a >= b); }
            case Op::Divide:
            case Op::Remainder:
            {
                if (b == 0)
                {
                    return make_unexpected(Errc::division_by_zero);
                }
                break;
            }
            default:
            {
                break;
            }
        }

        if constexpr (std::is_integral_v<T>)
        {
            uint64_t const ua = static_cast<uint64_t>(a);
            uint64_t const ub = static_cast<uint64_t>(b);
            switch (op)
            {
                case Op::Add:       { return static_cast<int64_t>(ua + ub); }
                case Op::Subtract:  { return static_cast<int64_t>(ua - ub); }
                case Op::Multiply:  { return static_cast<int64_t>(ua * ub); }
                case Op::Power:     { return power(a, b); }
                case Op::RoundTo:   { return round_to(a, b); }
                case Op::BitAnd:    { return a & b; }
                case Op::BitOr:     { return a | b; }
                case Op::BitXor:    { return a ^ b; }
                // INT64_MIN / -1 overflows: negating wraps instead.
                case Op::Divide:
                {
                    if (b == -1)
                    {
                        return negate(a);
                    }
                    return a / b;
                }
                case Op::Remainder:
                {
                    if (b == -1)
                    {
                        return 0;
                    }
                    return a % b;
                }
                // A shift count outside 0..63 shifts every bit out.
                case Op::ShiftLeft:
                {
                    if (b < 0 or b > 63)
                    {
                        return 0;
                    }
                    return static_cast<int64_t>(ua << b);
                }
                case Op::ShiftRight:
                {
                    if (b < 0 or b > 63)
                    {
                        return a >> 63;
                    }
                    return a >> b;
                }
                default:
                {
                    std::unreachable();
                }
            }
        }
        else
        {
            switch (op)
            {
                case Op::Add:       { return a + b; }
                case Op::Subtract:  { return a - b; }
                case Op::Multiply:  { return a * b; }
                case Op::Divide:    { return a / b; }
                case Op::Remainder: { return std::fmod(a, b); }
                case Op::Power:
                {
                    if (a == 0.0 and b < 0.0)
                    {
                        return make_unexpected(Errc::division_by_zero);
                    }
                    return std::pow(a, b);
                }
                case Op::RoundTo:
                {
                    double const scale = std::pow(10.0, std::trunc(b));
                    return std::round(a * scale) / scale;
                }
                default:
                {
                    // Shifts and bit operations work on the integer values.
                    return binary<int64_t>(op, saturate(a), saturate(b)).transform([](int64_t value) { return static_cast<double>(value); });
                }
            }
        }
    }

    template std::expected<int64_t, std::error_code> Formula::evaluate<int64_t>(
        std::function<std::expected<int64_t, std::error_code>(std::size_t)> const&) const;
    template std::expected<double, std::error_code> Formula::evaluate<double>(
        std::function<std::expected<double, std::error_code>(std::size_t)> const&) const;
}
