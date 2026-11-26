#include <algorithm>
#include <limits>

#include "kickcam/Error.h"
#include "genapi/SwissKnifeNodes.h"

namespace kickcam::genapi
{
    template<typename Interface, typename T>
    std::expected<T, std::error_code> SwissKnifeNode<Interface, T>::min() const
    {
        return std::numeric_limits<T>::lowest();
    }

    template<typename Interface, typename T>
    std::expected<T, std::error_code> SwissKnifeNode<Interface, T>::max() const
    {
        return std::numeric_limits<T>::max();
    }

    template<typename Interface, typename T>
    decltype(std::declval<Interface const&>().inc()) SwissKnifeNode<Interface, T>::inc() const
    {
        if constexpr (std::is_integral_v<T>)
        {
            return int64_t{1};
        }
        else
        {
            return std::nullopt;
        }
    }

    template<typename Interface, typename T>
    std::expected<Access, std::error_code> SwissKnifeNode<Interface, T>::own_access() const
    {
        return Access::RO;
    }

    template<typename Interface, typename T>
    std::expected<T, std::error_code> SwissKnifeNode<Interface, T>::read() const
    {
        return variables.evaluate(formula, T{0});
    }

    template<typename Interface, typename T>
    std::expected<void, std::error_code> SwissKnifeNode<Interface, T>::write(T)
    {
        return make_unexpected(Errc::not_writable);
    }

    template<typename Interface, typename T>
    std::expected<T, std::error_code> ConverterNode<Interface, T>::min() const
    {
        if (varying)
        {
            return std::numeric_limits<T>::lowest();
        }
        return bounds().transform([](std::pair<T, T> range) { return range.first; });
    }

    template<typename Interface, typename T>
    std::expected<T, std::error_code> ConverterNode<Interface, T>::max() const
    {
        if (varying)
        {
            return std::numeric_limits<T>::max();
        }
        return bounds().transform([](std::pair<T, T> range) { return range.second; });
    }

    template<typename Interface, typename T>
    decltype(std::declval<Interface const&>().inc()) ConverterNode<Interface, T>::inc() const
    {
        if constexpr (std::is_integral_v<T>)
        {
            return int64_t{1};
        }
        else
        {
            return std::nullopt;
        }
    }

    template<typename Interface, typename T>
    std::expected<std::pair<T, T>, std::error_code> ConverterNode<Interface, T>::bounds() const
    {
        auto low = value.min();
        if (not low)
        {
            return std::unexpected(low.error());
        }
        auto high = value.max();
        if (not high)
        {
            return std::unexpected(high.error());
        }
        auto a = variables.evaluate(from, *low);
        if (not a)
        {
            return std::unexpected(a.error());
        }
        auto b = variables.evaluate(from, *high);
        if (not b)
        {
            return std::unexpected(b.error());
        }
        return std::pair{std::min(*a, *b), std::max(*a, *b)};
    }

    template<typename Interface, typename T>
    std::expected<Access, std::error_code> ConverterNode<Interface, T>::own_access() const
    {
        return value.access();
    }

    template<typename Interface, typename T>
    std::expected<T, std::error_code> ConverterNode<Interface, T>::read() const
    {
        auto raw = value.get();
        if (not raw)
        {
            return std::unexpected(raw.error());
        }
        return variables.evaluate(from, *raw);
    }

    template<typename Interface, typename T>
    std::expected<void, std::error_code> ConverterNode<Interface, T>::write(T converted)
    {
        auto raw = variables.evaluate(to, converted);
        if (not raw)
        {
            return std::unexpected(raw.error());
        }
        return value.set(*raw);
    }

    template class SwissKnifeNode<Float, double>;
    template class SwissKnifeNode<Integer, int64_t>;
    template class ConverterNode<Float, double>;
    template class ConverterNode<Integer, int64_t>;
}
