#include <type_traits>

#include "kickcam/Error.h"
#include "genapi/NodeBase.h"

namespace kickcam::genapi
{
    Access combine(Access a, Access b)
    {
        if (a == b or b == Access::RW)
        {
            return a;
        }
        if (a == Access::RW)
        {
            return b;
        }
        if (a == Access::NI or b == Access::NI)
        {
            return Access::NI;
        }
        return Access::NA;
    }

    std::expected<Access, std::error_code> Conditions::presence() const
    {
        auto implemented = is_implemented.get();
        if (not implemented)
        {
            return std::unexpected(implemented.error());
        }
        if (*implemented == 0)
        {
            return Access::NI;
        }
        auto available = is_available.get();
        if (not available)
        {
            return std::unexpected(available.error());
        }
        if (*available == 0)
        {
            return Access::NA;
        }
        return Access::RW;
    }

    std::expected<Access, std::error_code> Conditions::lock(std::expected<Access, std::error_code> unlocked) const
    {
        if (not unlocked or *unlocked == Access::NI or *unlocked == Access::NA)
        {
            return unlocked;
        }
        auto locked = is_locked.get();
        if (not locked)
        {
            return std::unexpected(locked.error());
        }
        if (*locked != 0)
        {
            return combine(*unlocked, Access::RO);
        }
        return unlocked;
    }

    std::expected<void, std::error_code> check_usable(std::expected<Access, std::error_code> mode)
    {
        if (not mode)
        {
            return std::unexpected(mode.error());
        }
        if (*mode == Access::NI)
        {
            return make_unexpected(Errc::not_implemented);
        }
        if (*mode == Access::NA)
        {
            return make_unexpected(Errc::not_available);
        }
        return {};
    }

    std::expected<void, std::error_code> check_readable(std::expected<Access, std::error_code> mode)
    {
        auto usable = check_usable(mode);
        if (not usable)
        {
            return usable;
        }
        if (*mode == Access::WO)
        {
            return make_unexpected(Errc::not_readable);
        }
        return {};
    }

    std::expected<void, std::error_code> Conditions::check_writable(std::expected<Access, std::error_code> unlocked) const
    {
        auto usable = check_usable(unlocked);
        if (not usable)
        {
            return usable;
        }
        if (*unlocked == Access::RO)
        {
            return make_unexpected(Errc::not_writable);
        }
        auto locked = is_locked.get();
        if (not locked)
        {
            return std::unexpected(locked.error());
        }
        if (*locked != 0)
        {
            return make_unexpected(Errc::locked);
        }
        return {};
    }

    template<typename Interface, typename T>
    std::expected<T, std::error_code> NumericBase<Interface, T>::get() const
    {
        auto readable = this->check_readable();
        if (not readable)
        {
            return std::unexpected(readable.error());
        }
        return read();
    }

    template<typename Interface, typename T>
    std::expected<void, std::error_code> NumericBase<Interface, T>::set(T value)
    {
        auto writable = this->check_writable();
        if (not writable)
        {
            return writable;
        }
        auto lowest = this->min();
        if (not lowest)
        {
            return std::unexpected(lowest.error());
        }
        auto highest = this->max();
        if (not highest)
        {
            return std::unexpected(highest.error());
        }
        if (value < *lowest or value > *highest)
        {
            return make_unexpected(Errc::out_of_range);
        }
        if constexpr (std::is_integral_v<T>)
        {
            auto step = this->inc();
            if (not step)
            {
                return std::unexpected(step.error());
            }
            // Unsigned difference: value - min overflows int64 when min is the int64 minimum.
            uint64_t const offset = static_cast<uint64_t>(value) - static_cast<uint64_t>(*lowest);
            if (*step > 1 and offset % static_cast<uint64_t>(*step) != 0)
            {
                return make_unexpected(Errc::out_of_range);
            }
        }
        return write(value);
    }

    template class NumericBase<Integer, int64_t>;
    template class NumericBase<Float, double>;
}
