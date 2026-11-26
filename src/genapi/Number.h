#ifndef KICKCAM_GENAPI_NUMBER_H
#define KICKCAM_GENAPI_NUMBER_H

#include <cstdint>
#include <optional>
#include <string_view>

namespace kickcam::genapi
{
    // HexOrDecimal: "0x" followed by hex digits, or decimal digits with an optional minus sign.
    std::optional<int64_t> to_int(std::string_view text);

    // Read in the C locale, whatever the process locale.
    std::optional<double> to_double(std::string_view text);

    // Clamps to the int64 range; NaN gives 0.
    int64_t saturate(double value);
}

#endif
