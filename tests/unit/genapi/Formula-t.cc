#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <limits>
#include <numbers>

#include "genapi/Formula.h"
#include "helpers.h"

using namespace kickcam;
using namespace kickcam::genapi;

namespace
{
    constexpr std::array<std::string_view, 4> NAMES{"A", "B", "C", "W.Max"};

    std::optional<std::size_t> resolve(std::string_view name)
    {
        auto it = std::ranges::find(NAMES, name);
        if (it == NAMES.end())
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(it - NAMES.begin());
    }

    // Evaluates text with A, B, C and W.Max set to values.
    template<typename T>
    std::expected<T, std::error_code> run(std::string_view text, std::array<T, 4> values = {})
    {
        auto formula = Formula::parse(text, resolve);
        if (not formula)
        {
            ADD_FAILURE() << text << ": " << formula.error();
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        return formula->evaluate<T>([&](std::size_t index) -> std::expected<T, std::error_code>
        {
            return values[index];
        });
    }

    std::string parse_error(std::string_view text)
    {
        auto formula = Formula::parse(text, resolve);
        if (formula)
        {
            return "parsed";
        }
        return formula.error();
    }
}

TEST(Formula, FollowsPrecedenceAndGrouping)
{
    EXPECT_EQ(run<int64_t>("1 + 2 * 3"), 7);
    EXPECT_EQ(run<int64_t>("(1 + 2) * 3"), 9);
    EXPECT_EQ(run<int64_t>("10 - 4 - 3"), 3);
    EXPECT_EQ(run<int64_t>("2 ** 3 ** 2"), 512);
    EXPECT_EQ(run<int64_t>("-2 ** 2"), 4);
    EXPECT_EQ(run<int64_t>("1 + 1 = 2"), 1);
    EXPECT_EQ(run<int64_t>("1 | 2 & 3"), 3);
    EXPECT_EQ(run<int64_t>("1 << 2 + 1"), 8);
    EXPECT_EQ(run<int64_t>("1 < 2 = 1"), 1);
    EXPECT_EQ(run<int64_t>("0 || 1 && 0"), 0);
    EXPECT_EQ(run<int64_t>("0 ? 1 : 1 ? 2 : 3"), 2);
    EXPECT_EQ(run<int64_t>("1 ? 0 ? 4 : 5 : 6"), 5);
}

TEST(Formula, ComputesEveryOperator)
{
    EXPECT_EQ(run<int64_t>("7 % 3"), 1);
    EXPECT_EQ(run<int64_t>("256 >> 4"), 16);
    EXPECT_EQ(run<int64_t>("0xF0 | 0x0F"), 255);
    EXPECT_EQ(run<int64_t>("0xFF & 0x0F"), 15);
    EXPECT_EQ(run<int64_t>("6 ^ 3"), 5);
    EXPECT_EQ(run<int64_t>("~0"), -1);
    EXPECT_EQ(run<int64_t>("+3 - -2"), 5);
    EXPECT_EQ(run<int64_t>("3 <> 3"), 0);
    EXPECT_EQ(run<int64_t>("3 <= 3"), 1);
    EXPECT_EQ(run<int64_t>("3 >= 4"), 0);
    EXPECT_EQ(run<int64_t>("3 > 2"), 1);
    EXPECT_EQ(run<int64_t>("1 && 2"), 1);
}

TEST(Formula, DividesByMode)
{
    EXPECT_EQ(run<int64_t>("7 / 2"), 3);
    EXPECT_EQ(run<double>("7 / 2"), 3.5);
    EXPECT_EQ(run<int64_t>("1.9 + 1"), 2);
    EXPECT_EQ(run<double>("1.5e2"), 150.0);
    EXPECT_EQ(run<double>("5 % 3.5"), 1.5);
    EXPECT_EQ(run<double>("(0xFF00 >> 8) / 2"), 127.5);
}

TEST(Formula, FailsOnDivisionByZero)
{
    EXPECT_EQ(error_of(run<int64_t>("1 / 0")), Errc::division_by_zero);
    EXPECT_EQ(error_of(run<int64_t>("1 % 0")), Errc::division_by_zero);
    EXPECT_EQ(error_of(run<int64_t>("0 ** -1")), Errc::division_by_zero);
    EXPECT_EQ(error_of(run<double>("1 / 0")), Errc::division_by_zero);
    EXPECT_EQ(error_of(run<double>("1 % 0")), Errc::division_by_zero);
}

TEST(Formula, WrapsIntegersInsteadOfOverflowing)
{
    EXPECT_EQ(run<int64_t>("0x7FFFFFFFFFFFFFFF + 1"), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(run<int64_t>("(-0x7FFFFFFFFFFFFFFF - 1) / -1"), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(run<int64_t>("(-0x7FFFFFFFFFFFFFFF - 1) % -1"), 0);
    EXPECT_EQ(run<int64_t>("ABS(-0x7FFFFFFFFFFFFFFF - 1)"), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(run<int64_t>("1 << 64"), 0);
    EXPECT_EQ(run<int64_t>("-8 >> 70"), -1);
    EXPECT_EQ(run<int64_t>("2 ** 10"), 1024);
    EXPECT_EQ(run<int64_t>("2 ** -1"), 0);
    EXPECT_EQ(run<int64_t>("(-1) ** -3"), -1);
}

TEST(Formula, ComputesFunctions)
{
    EXPECT_EQ(run<double>("SQRT(16)"), 4.0);
    EXPECT_EQ(run<double>("ABS(-3.5)"), 3.5);
    EXPECT_EQ(run<double>("TRUNC(-2.7)"), -2.0);
    EXPECT_EQ(run<double>("FLOOR(-2.5)"), -3.0);
    EXPECT_EQ(run<double>("CEIL(2.1)"), 3.0);
    EXPECT_EQ(run<double>("ROUND(2.5)"), 3.0);
    EXPECT_DOUBLE_EQ(run<double>("ROUND(3.14159, 2)").value_or(0.0), 3.14);
    EXPECT_EQ(run<double>("ROUND(1234, -2)"), 1200.0);
    EXPECT_EQ(run<double>("SGN(-5)"), -1.0);
    EXPECT_EQ(run<double>("NEG(4)"), -4.0);
    EXPECT_DOUBLE_EQ(run<double>("LN(E)").value_or(0.0), 1.0);
    EXPECT_EQ(run<double>("LG(100)"), 2.0);
    EXPECT_EQ(run<double>("EXP(0)"), 1.0);
    EXPECT_EQ(run<double>("COS(0) + SIN(0) + TAN(0)"), 1.0);
    EXPECT_DOUBLE_EQ(run<double>("ASIN(1) + ACOS(1)").value_or(0.0), std::numbers::pi / 2);
    EXPECT_DOUBLE_EQ(run<double>("4 * ATAN(1)").value_or(0.0), std::numbers::pi);
    EXPECT_EQ(run<double>("sqrt(4)"), 2.0);
}

TEST(Formula, ComputesFunctionsOnIntegers)
{
    EXPECT_EQ(run<int64_t>("SGN(-5)"), -1);
    EXPECT_EQ(run<int64_t>("ABS(-5)"), 5);
    EXPECT_EQ(run<int64_t>("TRUNC(7)"), 7);
    EXPECT_EQ(run<int64_t>("SQRT(17)"), 4);
    EXPECT_EQ(run<int64_t>("ROUND(1250, -2)"), 1300);
    EXPECT_EQ(run<int64_t>("ROUND(-1250, -2)"), -1300);
    EXPECT_EQ(run<int64_t>("ROUND(1249, -2)"), 1200);
    EXPECT_EQ(run<int64_t>("ROUND(9007199254740993, 0)"), 9007199254740993);
    EXPECT_EQ(run<int64_t>("PI"), 3);
}

TEST(Formula, ReadsVariables)
{
    EXPECT_EQ(run<int64_t>("A * 2 + W.Max", {5, 0, 0, 100}), 110);
    EXPECT_EQ(run<double>("(A - B) / C", {1.0, 0.5, 0.25, 0.0}), 2.0);
}

TEST(Formula, ReadsOnlyTheBranchesItTakes)
{
    std::vector<std::size_t> reads;
    auto variable = [&](std::size_t index) -> std::expected<int64_t, std::error_code>
    {
        reads.push_back(index);
        if (index == 2)
        {
            return make_unexpected(Errc::not_available);
        }
        return 1;
    };

    auto choose = Formula::parse("A ? B : C", resolve);
    ASSERT_TRUE(choose) << choose.error();
    EXPECT_EQ(choose->evaluate<int64_t>(variable), 1);
    EXPECT_EQ(reads, (std::vector<std::size_t>{0, 1}));

    reads.clear();
    auto either = Formula::parse("A || C", resolve);
    ASSERT_TRUE(either) << either.error();
    EXPECT_EQ(either->evaluate<int64_t>(variable), 1);
    EXPECT_EQ(reads, (std::vector<std::size_t>{0}));

    auto failing = Formula::parse("C + 1", resolve);
    ASSERT_TRUE(failing) << failing.error();
    EXPECT_EQ(error_of(failing->evaluate<int64_t>(variable)), Errc::not_available);
}

TEST(Formula, VariablesHideTheConstants)
{
    auto formula = Formula::parse("PI + E", [](std::string_view name) -> std::optional<std::size_t>
    {
        if (name == "PI")
        {
            return 0;
        }
        return std::nullopt;
    });
    ASSERT_TRUE(formula) << formula.error();
    auto value = formula->evaluate<double>([](std::size_t) -> std::expected<double, std::error_code> { return 10.0; });
    EXPECT_DOUBLE_EQ(value.value_or(0.0), 10.0 + std::numbers::e);
}

TEST(Formula, NamesParseErrors)
{
    EXPECT_EQ(parse_error(""), "unexpected end");
    EXPECT_EQ(parse_error("1 +"), "unexpected end");
    EXPECT_EQ(parse_error("(1"), "expected ')' at the end");
    EXPECT_EQ(parse_error("1 2"), "unexpected '2' at offset 2");
    EXPECT_EQ(parse_error("1 $ 2"), "unexpected '$' at offset 2");
    EXPECT_EQ(parse_error("1 ? 2"), "expected ':' at the end");
    EXPECT_EQ(parse_error("FOO(1)"), "unknown function 'FOO'");
    EXPECT_EQ(parse_error("SQRT(1, 2)"), "expected ')' at offset 6");
    EXPECT_EQ(parse_error("D + 1"), "unknown name 'D'");
    EXPECT_EQ(parse_error("0x"), "invalid number '0x' at offset 0");
}
