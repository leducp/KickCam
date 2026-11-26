#ifndef KICKCAM_ERROR_H
#define KICKCAM_ERROR_H

#include <expected>
#include <system_error>

namespace kickcam
{
    enum class Errc
    {
        not_implemented = 1,
        not_available,
        locked,
        not_readable,
        not_writable,
        out_of_range,   // outside min/max, off the increment grid, or too long for the register
        invalid_value,  // a name or a device value that matches no enumeration entry or boolean state
    };

    std::error_category const& error_category();
    std::error_code make_error_code(Errc e);
    std::unexpected<std::error_code> make_unexpected(Errc e);
}

template<>
struct std::is_error_code_enum<kickcam::Errc> : std::true_type
{
};

#endif
