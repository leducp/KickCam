#ifndef KICKCAM_GENAPI_REGISTER_NODES_H
#define KICKCAM_GENAPI_REGISTER_NODES_H

#include "genapi/Nodes.h"

namespace kickcam::genapi
{
    struct RegisterAccess
    {
        // Sum of the Address and pAddress elements.
        std::expected<uint64_t, std::error_code> address() const;

        // String and raw registers, whose pLength may change. A negative length fails with Errc::out_of_range.
        std::expected<std::size_t, std::error_code> size() const;
        std::expected<std::vector<uint8_t>, std::error_code> read_bytes() const;
        std::expected<void, std::error_code> write_bytes(std::span<uint8_t const> data) const;

        // Integer and float registers: a constant length of 1 to 8 bytes, checked by the parser.
        uint32_t bits() const;
        std::expected<uint64_t, std::error_code> read_uint() const;
        std::expected<void, std::error_code> write_uint(uint64_t value) const;

        // Replaces the bits under mask and keeps the others, reading and writing the same address.
        std::expected<void, std::error_code> update_uint(uint64_t mask, uint64_t value) const;

        std::vector<IntValue> offsets;
        IntValue length;
        Access mode{Access::RO};
        PortNode* port{nullptr};
        bool big_endian{false};
    };

    class IntRegNode final : public NumericBase<Integer, int64_t>
    {
    public:
        using NumericBase::NumericBase;

        std::expected<int64_t, std::error_code> min() const override;
        std::expected<int64_t, std::error_code> max() const override;
        std::expected<int64_t, std::error_code> inc() const override;

        RegisterAccess reg;
        bool is_signed{false};

    private:
        std::expected<Access, std::error_code> own_access() const override;
        std::expected<int64_t, std::error_code> read() const override;
        std::expected<void, std::error_code> write(int64_t value) override;
    };

    class MaskedIntRegNode final : public NumericBase<Integer, int64_t>
    {
    public:
        using NumericBase::NumericBase;

        std::expected<int64_t, std::error_code> min() const override;
        std::expected<int64_t, std::error_code> max() const override;
        std::expected<int64_t, std::error_code> inc() const override;

        RegisterAccess reg;
        bool is_signed{false};
        uint32_t lsb{0}; // bit 0 is the least significant bit of the register, whatever its byte order
        uint32_t msb{0};

    private:
        std::expected<Access, std::error_code> own_access() const override;
        std::expected<int64_t, std::error_code> read() const override;
        std::expected<void, std::error_code> write(int64_t value) override;
    };

    class FloatRegNode final : public NumericBase<Float, double>
    {
    public:
        using NumericBase::NumericBase;

        std::expected<double, std::error_code> min() const override;
        std::expected<double, std::error_code> max() const override;
        std::expected<std::optional<double>, std::error_code> inc() const override;

        RegisterAccess reg;

    private:
        std::expected<Access, std::error_code> own_access() const override;
        std::expected<double, std::error_code> read() const override;
        std::expected<void, std::error_code> write(double value) override;
    };

    class StringRegNode final : public NodeBase<String>
    {
    public:
        using NodeBase::NodeBase;

        std::expected<std::string, std::error_code> get() const override;
        std::expected<void, std::error_code> set(std::string_view value) override;

        RegisterAccess reg;

    private:
        std::expected<Access, std::error_code> own_access() const override;
    };

    class RegisterNode final : public NodeBase<Register>
    {
    public:
        using NodeBase::NodeBase;

        std::expected<std::vector<uint8_t>, std::error_code> get() const override;
        std::expected<void, std::error_code> set(std::span<uint8_t const> data) override;

        RegisterAccess reg;

    private:
        std::expected<Access, std::error_code> own_access() const override;
    };
}

#endif
