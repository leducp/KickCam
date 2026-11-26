#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

#include "genapi/Number.h"

namespace kickcam::genapi
{
    std::optional<int64_t> to_int(std::string_view text)
    {
        char const* const end = text.data() + text.size();
        std::from_chars_result result{};
        int64_t number = 0;
        if (text.starts_with("0x") or text.starts_with("0X"))
        {
            uint64_t raw = 0;
            result = std::from_chars(text.data() + 2, end, raw, 16);
            number = static_cast<int64_t>(raw);
        }
        else
        {
            result = std::from_chars(text.data(), end, number);
        }
        if (text.empty() or result.ec != std::errc{} or result.ptr != end)
        {
            return std::nullopt;
        }
        return number;
    }

    // std::from_chars for double is missing from the libc++ of older macOS.
    std::optional<double> to_double(std::string_view text)
    {
        std::istringstream stream{std::string{text}};
        stream.imbue(std::locale::classic());
        double number = 0.0;
        stream >> number;
        if (stream.fail() or not stream.eof())
        {
            return std::nullopt;
        }
        return number;
    }

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
}
