#include <algorithm>
#include <array>
#include <bit>
#include <limits>

#include "kickcam/Error.h"
#include "genapi/RegisterNodes.h"

namespace kickcam::genapi
{
    namespace
    {
        uint64_t mask(uint32_t bits)
        {
            if (bits >= 64)
            {
                return ~uint64_t{0};
            }
            return (uint64_t{1} << bits) - 1;
        }

        int64_t sign_extend(uint64_t value, uint32_t bits)
        {
            if (bits >= 64)
            {
                return static_cast<int64_t>(value);
            }
            uint32_t const shift = 64 - bits;
            return static_cast<int64_t>(value << shift) >> shift;
        }

        int64_t lowest(uint32_t bits, bool is_signed)
        {
            if (not is_signed)
            {
                return 0;
            }
            return sign_extend(uint64_t{1} << (bits - 1), bits);
        }

        // An unsigned 64-bit register reports the int64 maximum; a value above it reads as a negative int64, bits unchanged.
        int64_t highest(uint32_t bits, bool is_signed)
        {
            if (is_signed)
            {
                return static_cast<int64_t>(mask(bits - 1));
            }
            if (bits >= 64)
            {
                return std::numeric_limits<int64_t>::max();
            }
            return static_cast<int64_t>(mask(bits));
        }

        std::expected<uint64_t, std::error_code> read_at(RegisterAccess const& reg, uint64_t address)
        {
            std::array<uint8_t, 8> bytes{};
            // The parser keeps the length at 8 bytes or less; the bound lets GCC see it.
            std::span<uint8_t> const data{bytes.data(), std::min<std::size_t>(reg.bits() / 8, bytes.size())};
            auto done = reg.port->read(address, data);
            if (not done)
            {
                return std::unexpected(done.error());
            }
            if (not reg.big_endian)
            {
                std::ranges::reverse(data);
            }
            uint64_t value = 0;
            for (uint8_t byte : data)
            {
                value = (value << 8) | byte;
            }
            return value;
        }

        std::expected<void, std::error_code> write_at(RegisterAccess const& reg, uint64_t address, uint64_t value)
        {
            std::array<uint8_t, 8> bytes{};
            // The parser keeps the length at 8 bytes or less; the bound lets GCC see it.
            std::span<uint8_t> const data{bytes.data(), std::min<std::size_t>(reg.bits() / 8, bytes.size())};
            for (uint8_t& byte : data)
            {
                byte = static_cast<uint8_t>(value);
                value >>= 8;
            }
            if (reg.big_endian)
            {
                std::ranges::reverse(data);
            }
            return reg.port->write(address, data);
        }
    }

    std::expected<uint64_t, std::error_code> RegisterAccess::address() const
    {
        uint64_t sum = 0;
        for (IntValue const& offset : offsets)
        {
            auto value = offset.get();
            if (not value)
            {
                return std::unexpected(value.error());
            }
            sum += static_cast<uint64_t>(*value);
        }
        for (Index const& term : indexes)
        {
            auto index = term.index.get();
            if (not index)
            {
                return std::unexpected(index.error());
            }
            auto step = length.get();
            if (term.offset)
            {
                step = term.offset->get();
            }
            if (not step)
            {
                return std::unexpected(step.error());
            }
            sum += static_cast<uint64_t>(*index) * static_cast<uint64_t>(*step);
        }
        return sum;
    }

    std::expected<std::size_t, std::error_code> RegisterAccess::size() const
    {
        auto value = length.get();
        if (not value)
        {
            return std::unexpected(value.error());
        }
        if (*value < 0)
        {
            return make_unexpected(Errc::out_of_range);
        }
        return static_cast<std::size_t>(*value);
    }

    std::expected<std::vector<uint8_t>, std::error_code> RegisterAccess::read_bytes() const
    {
        auto count = size();
        if (not count)
        {
            return std::unexpected(count.error());
        }
        auto where = address();
        if (not where)
        {
            return std::unexpected(where.error());
        }
        std::vector<uint8_t> data(*count);
        auto done = port->read(*where, data);
        if (not done)
        {
            return std::unexpected(done.error());
        }
        return data;
    }

    std::expected<void, std::error_code> RegisterAccess::write_bytes(std::span<uint8_t const> data) const
    {
        auto where = address();
        if (not where)
        {
            return std::unexpected(where.error());
        }
        return port->write(*where, data);
    }

    uint32_t RegisterAccess::bits() const
    {
        return static_cast<uint32_t>(std::get<int64_t>(length.source) * 8);
    }

    std::expected<uint64_t, std::error_code> RegisterAccess::read_uint() const
    {
        auto where = address();
        if (not where)
        {
            return std::unexpected(where.error());
        }
        return read_at(*this, *where);
    }

    std::expected<void, std::error_code> RegisterAccess::write_uint(uint64_t value) const
    {
        auto where = address();
        if (not where)
        {
            return std::unexpected(where.error());
        }
        return write_at(*this, *where, value);
    }

    std::expected<void, std::error_code> RegisterAccess::update_uint(uint64_t mask, uint64_t value) const
    {
        auto where = address();
        if (not where)
        {
            return std::unexpected(where.error());
        }
        auto raw = read_at(*this, *where);
        if (not raw)
        {
            return std::unexpected(raw.error());
        }
        return write_at(*this, *where, (*raw & ~mask) | (value & mask));
    }

    std::expected<int64_t, std::error_code> IntRegNode::min() const
    {
        return lowest(reg.bits(), is_signed);
    }

    std::expected<int64_t, std::error_code> IntRegNode::max() const
    {
        return highest(reg.bits(), is_signed);
    }

    std::expected<int64_t, std::error_code> IntRegNode::inc() const
    {
        return 1;
    }

    std::expected<Access, std::error_code> IntRegNode::own_access() const
    {
        return reg.mode;
    }

    std::expected<int64_t, std::error_code> IntRegNode::read() const
    {
        auto raw = reg.read_uint();
        if (not raw)
        {
            return std::unexpected(raw.error());
        }
        if (not is_signed)
        {
            return static_cast<int64_t>(*raw);
        }
        return sign_extend(*raw, reg.bits());
    }

    std::expected<void, std::error_code> IntRegNode::write(int64_t value)
    {
        return reg.write_uint(static_cast<uint64_t>(value));
    }

    std::expected<int64_t, std::error_code> MaskedIntRegNode::min() const
    {
        return lowest(msb - lsb + 1, is_signed);
    }

    std::expected<int64_t, std::error_code> MaskedIntRegNode::max() const
    {
        return highest(msb - lsb + 1, is_signed);
    }

    std::expected<int64_t, std::error_code> MaskedIntRegNode::inc() const
    {
        return 1;
    }

    std::expected<Access, std::error_code> MaskedIntRegNode::own_access() const
    {
        return reg.mode;
    }

    std::expected<int64_t, std::error_code> MaskedIntRegNode::read() const
    {
        auto raw = reg.read_uint();
        if (not raw)
        {
            return std::unexpected(raw.error());
        }
        uint32_t const width = msb - lsb + 1;
        uint64_t const field = (*raw >> lsb) & mask(width);
        if (is_signed)
        {
            return sign_extend(field, width);
        }
        return static_cast<int64_t>(field);
    }

    std::expected<void, std::error_code> MaskedIntRegNode::write(int64_t value)
    {
        return reg.update_uint(mask(msb - lsb + 1) << lsb, static_cast<uint64_t>(value) << lsb);
    }

    std::expected<double, std::error_code> FloatRegNode::min() const
    {
        if (reg.bits() == 32)
        {
            return std::numeric_limits<float>::lowest();
        }
        return std::numeric_limits<double>::lowest();
    }

    std::expected<double, std::error_code> FloatRegNode::max() const
    {
        if (reg.bits() == 32)
        {
            return std::numeric_limits<float>::max();
        }
        return std::numeric_limits<double>::max();
    }

    std::expected<std::optional<double>, std::error_code> FloatRegNode::inc() const
    {
        return std::nullopt;
    }

    std::expected<Access, std::error_code> FloatRegNode::own_access() const
    {
        return reg.mode;
    }

    std::expected<double, std::error_code> FloatRegNode::read() const
    {
        auto raw = reg.read_uint();
        if (not raw)
        {
            return std::unexpected(raw.error());
        }
        if (reg.bits() == 32)
        {
            return static_cast<double>(std::bit_cast<float>(static_cast<uint32_t>(*raw)));
        }
        return std::bit_cast<double>(*raw);
    }

    std::expected<void, std::error_code> FloatRegNode::write(double value)
    {
        if (reg.bits() == 32)
        {
            return reg.write_uint(std::bit_cast<uint32_t>(static_cast<float>(value)));
        }
        return reg.write_uint(std::bit_cast<uint64_t>(value));
    }

    std::expected<std::string, std::error_code> StringRegNode::get() const
    {
        auto readable = check_readable();
        if (not readable)
        {
            return std::unexpected(readable.error());
        }
        return reg.read_bytes().transform([](std::vector<uint8_t> const& bytes)
        {
            return std::string(bytes.begin(), std::ranges::find(bytes, 0));
        });
    }

    std::expected<void, std::error_code> StringRegNode::set(std::string_view value)
    {
        auto writable = check_writable();
        if (not writable)
        {
            return writable;
        }
        auto size = reg.size();
        if (not size)
        {
            return std::unexpected(size.error());
        }
        if (value.size() > *size)
        {
            return make_unexpected(Errc::out_of_range);
        }
        std::vector<uint8_t> bytes(*size, 0);
        std::ranges::copy(value, bytes.begin());
        return reg.write_bytes(bytes);
    }

    std::expected<Access, std::error_code> StringRegNode::own_access() const
    {
        return reg.mode;
    }

    std::expected<std::vector<uint8_t>, std::error_code> RegisterNode::get() const
    {
        auto readable = check_readable();
        if (not readable)
        {
            return std::unexpected(readable.error());
        }
        return reg.read_bytes();
    }

    std::expected<void, std::error_code> RegisterNode::set(std::span<uint8_t const> data)
    {
        auto writable = check_writable();
        if (not writable)
        {
            return writable;
        }
        auto size = reg.size();
        if (not size)
        {
            return std::unexpected(size.error());
        }
        if (data.size() != *size)
        {
            return make_unexpected(Errc::out_of_range);
        }
        return reg.write_bytes(data);
    }

    std::expected<Access, std::error_code> RegisterNode::own_access() const
    {
        return reg.mode;
    }
}
