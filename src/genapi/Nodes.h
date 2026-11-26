#ifndef KICKCAM_GENAPI_NODES_H
#define KICKCAM_GENAPI_NODES_H

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "kickcam/genapi/Port.h"
#include "genapi/NodeBase.h"
#include "genapi/Value.h"

namespace kickcam::genapi
{
    class IntegerNode final : public NumericBase<Integer, int64_t>
    {
    public:
        using NumericBase::NumericBase;

        std::expected<int64_t, std::error_code> min() const override;
        std::expected<int64_t, std::error_code> max() const override;
        std::expected<int64_t, std::error_code> inc() const override;

        IntValue value; // the default too, when pIndex selects no ValueIndexed
        std::optional<IntValue> index;
        std::vector<std::pair<int64_t, IntValue>> indexed;
        std::vector<IntValue> copies;
        std::optional<IntValue> minimum;
        std::optional<IntValue> maximum;
        std::optional<IntValue> increment;

    private:
        std::expected<Access, std::error_code> own_access() const override;
        std::expected<int64_t, std::error_code> read() const override;
        std::expected<void, std::error_code> write(int64_t value) override;
    };

    class FloatNode final : public NumericBase<Float, double>
    {
    public:
        using NumericBase::NumericBase;

        std::expected<double, std::error_code> min() const override;
        std::expected<double, std::error_code> max() const override;
        std::expected<std::optional<double>, std::error_code> inc() const override;

        FloatValue value; // the default too, when pIndex selects no ValueIndexed
        std::optional<IntValue> index;
        std::vector<std::pair<int64_t, FloatValue>> indexed;
        std::optional<FloatValue> minimum;
        std::optional<FloatValue> maximum;
        std::optional<FloatValue> increment;

    private:
        std::expected<Access, std::error_code> own_access() const override;
        std::expected<double, std::error_code> read() const override;
        std::expected<void, std::error_code> write(double value) override;
    };

    class BooleanNode final : public NodeBase<Boolean>
    {
    public:
        using NodeBase::NodeBase;

        std::expected<bool, std::error_code> get() const override;
        std::expected<void, std::error_code> set(bool value) override;

        IntValue value;
        int64_t on{1};
        int64_t off{0};

    private:
        std::expected<Access, std::error_code> own_access() const override;
    };

    class CommandNode final : public NodeBase<Command>
    {
    public:
        using NodeBase::NodeBase;

        std::expected<void, std::error_code> execute() override;
        std::expected<bool, std::error_code> is_done() const override;

        IntValue value;
        IntValue command_value;

    private:
        std::expected<Access, std::error_code> own_access() const override;
    };

    class EnumEntryNode final : public NodeBase<EnumEntry>
    {
    public:
        using NodeBase::NodeBase;

        int64_t value() const override;

        int64_t entry_value{0};
    };

    class EnumerationNode final : public NodeBase<Enumeration>
    {
    public:
        using NodeBase::NodeBase;

        std::expected<std::string_view, std::error_code> get() const override;
        std::expected<void, std::error_code> set(std::string_view entry) override;
        std::expected<int64_t, std::error_code> get_int() const override;
        std::expected<void, std::error_code> set_int(int64_t value) override;
        std::vector<EnumEntry const*> entries() const override;

        IntValue value;
        std::vector<std::unique_ptr<EnumEntryNode>> entry_nodes;

    private:
        std::expected<Access, std::error_code> own_access() const override;
    };

    class StringNode final : public NodeBase<String>
    {
    public:
        using NodeBase::NodeBase;

        std::expected<std::string, std::error_code> get() const override;
        std::expected<void, std::error_code> set(std::string_view value) override;

        std::string constant;
        String* node{nullptr};

    private:
        std::expected<Access, std::error_code> own_access() const override;
    };

    class CategoryNode final : public NodeBase<Category>
    {
    public:
        using NodeBase::NodeBase;

        std::span<Node* const> features() const override;

        std::vector<Node*> feature_list;

    private:
        std::expected<Access, std::error_code> own_access() const override;
    };

    class PortNode final : public NodeBase<Node>
    {
    public:
        using NodeBase::NodeBase;

        // Every access to a chunk port fails with Errc::not_available.
        std::expected<void, std::error_code> read(uint64_t address, std::span<uint8_t> data) const;
        std::expected<void, std::error_code> write(uint64_t address, std::span<uint8_t const> data) const;

        Port* port{nullptr};
        bool chunk{false};
    };
}

#endif
