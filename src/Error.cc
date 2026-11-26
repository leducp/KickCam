#include "kickcam/Error.h"

#include <string>

namespace kickcam
{
    namespace
    {
        class Category : public std::error_category
        {
        public:
            char const* name() const noexcept override
            {
                return "kickcam";
            }

            std::string message(int value) const override
            {
                switch (static_cast<Errc>(value))
                {
                    case Errc::not_implemented:  { return "feature not implemented"; }
                    case Errc::not_available:    { return "feature not available"; }
                    case Errc::locked:           { return "feature locked"; }
                    case Errc::not_readable:     { return "feature not readable"; }
                    case Errc::not_writable:     { return "feature not writable"; }
                    case Errc::out_of_range:     { return "value out of range"; }
                    case Errc::invalid_value:    { return "invalid value"; }
                    case Errc::division_by_zero: { return "division by zero in a formula"; }
                }
                return "unknown kickcam error";
            }
        };
    }

    std::error_category const& error_category()
    {
        static Category const category;
        return category;
    }

    std::error_code make_error_code(Errc e)
    {
        return {static_cast<int>(e), error_category()};
    }

    std::unexpected<std::error_code> make_unexpected(Errc e)
    {
        return std::unexpected(make_error_code(e));
    }
}
