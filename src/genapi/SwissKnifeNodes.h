#ifndef KICKCAM_GENAPI_SWISS_KNIFE_NODES_H
#define KICKCAM_GENAPI_SWISS_KNIFE_NODES_H

#include <type_traits>
#include <utility>

#include "genapi/NodeBase.h"
#include "genapi/Variables.h"

namespace kickcam::genapi
{
    // SwissKnife (Float, double) and IntSwissKnife (Integer, int64_t): a read-only formula.
    template<typename Interface, typename T>
    class SwissKnifeNode final : public NumericBase<Interface, T>
    {
    public:
        using NumericBase<Interface, T>::NumericBase;

        std::expected<T, std::error_code> min() const override;
        std::expected<T, std::error_code> max() const override;
        decltype(std::declval<Interface const&>().inc()) inc() const override;

        Variables variables;
        Formula formula;

    private:
        std::expected<Access, std::error_code> own_access() const override;
        std::expected<T, std::error_code> read() const override;
        std::expected<void, std::error_code> write(T value) override;
    };

    // Converter (Float, double) and IntConverter (Integer, int64_t): pValue read through FormulaFrom, written through FormulaTo.
    template<typename Interface, typename T>
    class ConverterNode final : public NumericBase<Interface, T>
    {
    public:
        using NumericBase<Interface, T>::NumericBase;

        // FormulaFrom of the pValue bounds, ordered; the full range for a Varying slope.
        std::expected<T, std::error_code> min() const override;
        std::expected<T, std::error_code> max() const override;
        decltype(std::declval<Interface const&>().inc()) inc() const override;

        Variables variables{.converter = true};
        Formula to;
        Formula from;
        std::conditional_t<std::is_integral_v<T>, IntValue, FloatValue> value;
        bool varying{false};

    private:
        std::expected<std::pair<T, T>, std::error_code> bounds() const;
        std::expected<Access, std::error_code> own_access() const override;
        std::expected<T, std::error_code> read() const override;
        std::expected<void, std::error_code> write(T value) override;
    };

    extern template class SwissKnifeNode<Float, double>;
    extern template class SwissKnifeNode<Integer, int64_t>;
    extern template class ConverterNode<Float, double>;
    extern template class ConverterNode<Integer, int64_t>;
}

#endif
