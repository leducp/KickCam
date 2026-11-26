#include "kickcam/Error.h"
#include "genapi/Nodes.h"

namespace kickcam::genapi
{
    std::expected<int64_t, std::error_code> IntegerNode::min() const
    {
        if (minimum)
        {
            return minimum->get();
        }
        return select(index, indexed, value).and_then([](IntValue const* current) { return current->min(); });
    }

    std::expected<int64_t, std::error_code> IntegerNode::max() const
    {
        if (maximum)
        {
            return maximum->get();
        }
        return select(index, indexed, value).and_then([](IntValue const* current) { return current->max(); });
    }

    std::expected<int64_t, std::error_code> IntegerNode::inc() const
    {
        if (increment)
        {
            return increment->get();
        }
        return select(index, indexed, value).and_then([](IntValue const* current) { return current->inc(); });
    }

    std::expected<Access, std::error_code> IntegerNode::own_access() const
    {
        return select(index, indexed, value).and_then([](IntValue const* current) { return current->access(); });
    }

    std::expected<int64_t, std::error_code> IntegerNode::read() const
    {
        return select(index, indexed, value).and_then([](IntValue const* current) { return current->get(); });
    }

    std::expected<void, std::error_code> IntegerNode::write(int64_t new_value)
    {
        auto current = select(index, indexed, value);
        if (not current)
        {
            return std::unexpected(current.error());
        }
        auto written = (*current)->set(new_value);
        if (not written)
        {
            return written;
        }
        for (IntValue& copy : copies)
        {
            auto copied = copy.set(new_value);
            if (not copied)
            {
                return copied;
            }
        }
        return {};
    }

    std::expected<double, std::error_code> FloatNode::min() const
    {
        if (minimum)
        {
            return minimum->get();
        }
        return select(index, indexed, value).and_then([](FloatValue const* current) { return current->min(); });
    }

    std::expected<double, std::error_code> FloatNode::max() const
    {
        if (maximum)
        {
            return maximum->get();
        }
        return select(index, indexed, value).and_then([](FloatValue const* current) { return current->max(); });
    }

    std::expected<std::optional<double>, std::error_code> FloatNode::inc() const
    {
        if (increment)
        {
            return increment->get();
        }
        return select(index, indexed, value).and_then([](FloatValue const* current) { return current->inc(); });
    }

    std::expected<Access, std::error_code> FloatNode::own_access() const
    {
        return select(index, indexed, value).and_then([](FloatValue const* current) { return current->access(); });
    }

    std::expected<double, std::error_code> FloatNode::read() const
    {
        return select(index, indexed, value).and_then([](FloatValue const* current) { return current->get(); });
    }

    std::expected<void, std::error_code> FloatNode::write(double new_value)
    {
        return select(index, indexed, value).and_then([new_value](FloatValue* current) { return current->set(new_value); });
    }

    std::expected<bool, std::error_code> BooleanNode::get() const
    {
        auto readable = check_readable();
        if (not readable)
        {
            return std::unexpected(readable.error());
        }
        auto raw = value.get();
        if (not raw)
        {
            return std::unexpected(raw.error());
        }
        if (*raw == on)
        {
            return true;
        }
        if (*raw == off)
        {
            return false;
        }
        return make_unexpected(Errc::invalid_value);
    }

    std::expected<void, std::error_code> BooleanNode::set(bool new_value)
    {
        auto writable = check_writable();
        if (not writable)
        {
            return writable;
        }
        if (new_value)
        {
            return value.set(on);
        }
        return value.set(off);
    }

    std::expected<Access, std::error_code> BooleanNode::own_access() const
    {
        return value.access();
    }

    std::expected<void, std::error_code> CommandNode::execute()
    {
        auto writable = check_writable();
        if (not writable)
        {
            return writable;
        }
        auto command = command_value.get();
        if (not command)
        {
            return std::unexpected(command.error());
        }
        return value.set(*command);
    }

    std::expected<bool, std::error_code> CommandNode::is_done() const
    {
        auto mode = access();
        if (not mode)
        {
            return std::unexpected(mode.error());
        }
        if (*mode == Access::WO)
        {
            return true;
        }
        auto readable = check_readable();
        if (not readable)
        {
            return std::unexpected(readable.error());
        }
        auto current = value.get();
        if (not current)
        {
            return std::unexpected(current.error());
        }
        auto command = command_value.get();
        if (not command)
        {
            return std::unexpected(command.error());
        }
        return *current != *command;
    }

    std::expected<Access, std::error_code> CommandNode::own_access() const
    {
        return value.access();
    }

    int64_t EnumEntryNode::value() const
    {
        return entry_value;
    }

    std::expected<std::string_view, std::error_code> EnumerationNode::get() const
    {
        auto raw = get_int();
        if (not raw)
        {
            return std::unexpected(raw.error());
        }
        for (auto const& entry : entry_nodes)
        {
            if (entry->value() == *raw)
            {
                return entry->name();
            }
        }
        return make_unexpected(Errc::invalid_value);
    }

    std::expected<void, std::error_code> EnumerationNode::set(std::string_view entry_name)
    {
        for (auto const& entry : entry_nodes)
        {
            if (entry->name() == entry_name)
            {
                return set_int(entry->value());
            }
        }
        return make_unexpected(Errc::invalid_value);
    }

    std::expected<int64_t, std::error_code> EnumerationNode::get_int() const
    {
        auto readable = check_readable();
        if (not readable)
        {
            return std::unexpected(readable.error());
        }
        return value.get();
    }

    std::expected<void, std::error_code> EnumerationNode::set_int(int64_t new_value)
    {
        auto writable = check_writable();
        if (not writable)
        {
            return writable;
        }
        for (auto const& entry : entry_nodes)
        {
            if (entry->value() == new_value)
            {
                auto usable = check_usable(entry->access());
                if (not usable)
                {
                    return usable;
                }
                return value.set(new_value);
            }
        }
        return make_unexpected(Errc::invalid_value);
    }

    std::vector<EnumEntry const*> EnumerationNode::entries() const
    {
        std::vector<EnumEntry const*> list;
        for (auto const& entry : entry_nodes)
        {
            list.push_back(entry.get());
        }
        return list;
    }

    std::expected<Access, std::error_code> EnumerationNode::own_access() const
    {
        return value.access();
    }

    std::expected<std::string, std::error_code> StringNode::get() const
    {
        auto readable = check_readable();
        if (not readable)
        {
            return std::unexpected(readable.error());
        }
        if (node == nullptr)
        {
            return constant;
        }
        return node->get();
    }

    std::expected<void, std::error_code> StringNode::set(std::string_view new_value)
    {
        auto writable = check_writable();
        if (not writable)
        {
            return writable;
        }
        if (node == nullptr)
        {
            constant = new_value;
            return {};
        }
        return node->set(new_value);
    }

    std::expected<Access, std::error_code> StringNode::own_access() const
    {
        if (node == nullptr)
        {
            return Access::RW;
        }
        return node->access();
    }

    std::span<Node* const> CategoryNode::features() const
    {
        return feature_list;
    }

    std::expected<Access, std::error_code> CategoryNode::own_access() const
    {
        return Access::RO;
    }

    std::expected<void, std::error_code> PortNode::read(uint64_t address, std::span<uint8_t> data) const
    {
        if (chunk)
        {
            return make_unexpected(Errc::not_available);
        }
        return port->read(address, data);
    }

    std::expected<void, std::error_code> PortNode::write(uint64_t address, std::span<uint8_t const> data) const
    {
        if (chunk)
        {
            return make_unexpected(Errc::not_available);
        }
        return port->write(address, data);
    }
}
