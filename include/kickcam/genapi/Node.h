#ifndef KICKCAM_GENAPI_NODE_H
#define KICKCAM_GENAPI_NODE_H

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace kickcam::genapi
{
    enum class Access
    {
        NI, // not implemented
        NA, // not available
        WO,
        RO,
        RW,
    };

    class Node
    {
    public:
        virtual ~Node() = default;

        virtual std::string const& name() const = 0;

        // Combines the node's own access with pIsImplemented, pIsAvailable, ImposedAccessMode and pIsLocked.
        virtual std::expected<Access, std::error_code> access() const = 0;
    };

    class Integer : public Node
    {
    public:
        virtual std::expected<int64_t, std::error_code> get() const = 0;

        // Fails with Errc::out_of_range when the value is outside min and max, or off the inc grid.
        virtual std::expected<void, std::error_code> set(int64_t value) = 0;

        virtual std::expected<int64_t, std::error_code> min() const = 0;
        virtual std::expected<int64_t, std::error_code> max() const = 0;
        virtual std::expected<int64_t, std::error_code> inc() const = 0;
    };

    class Float : public Node
    {
    public:
        virtual std::expected<double, std::error_code> get() const = 0;

        // Fails with Errc::out_of_range when the value is outside min and max. The device rounds or rejects a value off inc.
        virtual std::expected<void, std::error_code> set(double value) = 0;

        virtual std::expected<double, std::error_code> min() const = 0;
        virtual std::expected<double, std::error_code> max() const = 0;

        // Empty when the feature has no increment.
        virtual std::expected<std::optional<double>, std::error_code> inc() const = 0;
    };

    class Boolean : public Node
    {
    public:
        virtual std::expected<bool, std::error_code> get() const = 0;
        virtual std::expected<void, std::error_code> set(bool value) = 0;
    };

    class Command : public Node
    {
    public:
        virtual std::expected<void, std::error_code> execute() = 0;

        // A command whose register cannot be read is always done.
        virtual std::expected<bool, std::error_code> is_done() const = 0;
    };

    class EnumEntry : public Node
    {
    public:
        virtual int64_t value() const = 0;
    };

    class Enumeration : public Node
    {
    public:
        // Name of the current entry.
        virtual std::expected<std::string_view, std::error_code> get() const = 0;
        virtual std::expected<void, std::error_code> set(std::string_view entry) = 0;

        virtual std::expected<int64_t, std::error_code> get_int() const = 0;
        virtual std::expected<void, std::error_code> set_int(int64_t value) = 0;

        // Entries belong to their enumeration and are not in the node map: their names repeat across enumerations.
        virtual std::vector<EnumEntry const*> entries() const = 0;
    };

    class String : public Node
    {
    public:
        virtual std::expected<std::string, std::error_code> get() const = 0;

        // Fails with Errc::out_of_range when the value does not fit the register.
        virtual std::expected<void, std::error_code> set(std::string_view value) = 0;
    };

    class Register : public Node
    {
    public:
        virtual std::expected<std::vector<uint8_t>, std::error_code> get() const = 0;

        // Fails with Errc::out_of_range when the size differs from the register length.
        virtual std::expected<void, std::error_code> set(std::span<uint8_t const> data) = 0;
    };

    class Category : public Node
    {
    public:
        virtual std::span<Node* const> features() const = 0;
    };
}

#endif
