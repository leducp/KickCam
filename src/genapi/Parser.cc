#include <algorithm>
#include <array>

#include <type_traits>

#include "genapi/Number.h"
#include "genapi/Parser.h"

namespace kickcam::genapi
{
    namespace
    {
        // Elements that change nothing without caching, polling, events or a user interface.
        constexpr std::array<std::string_view, 27> IGNORED
        {
            "Extension", "ToolTip", "Description", "DisplayName", "Visibility", "DocuURL", "IsDeprecated",
            "EventID", "pBlockPolling", "pError", "pAlias", "pCastAlias", "pInvalidator", "pSelected",
            "Streamable", "Unit", "Representation", "DisplayNotation", "DisplayPrecision", "Cachable",
            "PollingTime", "ValidValueSet", "IsSelfClearing", "NumericValue", "Symbolic", "CacheChunkData", "IsLinear",
        };

        // A StructReg has a Comment instead of a Name.
        std::string_view name_of(tinyxml2::XMLElement const* element)
        {
            char const* name = element->Attribute("Name");
            if (name == nullptr)
            {
                name = element->Attribute("Comment");
            }
            if (name == nullptr)
            {
                return {};
            }
            return name;
        }

        // "Integer 'Width': <problem>"
        std::string fail(tinyxml2::XMLElement const* element, std::string_view problem)
        {
            std::string message{element->Name()};
            message += " '";
            message += name_of(element);
            message += "': ";
            message += problem;
            return message;
        }

        std::string_view raw(tinyxml2::XMLElement const* child)
        {
            char const* value = child->GetText();
            if (value == nullptr)
            {
                return {};
            }
            return value;
        }

        std::string_view text(tinyxml2::XMLElement const* child)
        {
            std::string_view value = raw(child);
            std::size_t const first = value.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos)
            {
                return {};
            }
            std::size_t const last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }

        std::string invalid(tinyxml2::XMLElement const* child, std::string_view what)
        {
            std::string problem = "invalid ";
            problem += what;
            problem += " '";
            problem += text(child);
            problem += "' in <";
            problem += child->Name();
            problem += ">";
            return fail(child->Parent()->ToElement(), problem);
        }

        std::expected<int64_t, std::string> integer(tinyxml2::XMLElement const* child)
        {
            auto number = to_int(text(child));
            if (not number)
            {
                return std::unexpected(invalid(child, "integer"));
            }
            return *number;
        }

        // The Index attribute of ValueIndexed and pValueIndexed.
        std::expected<int64_t, std::string> index_key(tinyxml2::XMLElement const* child)
        {
            char const* index = child->Attribute("Index");
            if (index == nullptr)
            {
                return std::unexpected(fail(child->Parent()->ToElement(), "no Index in <" + std::string{child->Name()} + ">"));
            }
            auto key = to_int(index);
            if (not key)
            {
                return std::unexpected(fail(child->Parent()->ToElement(), "invalid Index '" + std::string{index} + "' in <" + child->Name() + ">"));
            }
            return *key;
        }

        std::expected<double, std::string> real(tinyxml2::XMLElement const* child)
        {
            auto number = to_double(text(child));
            if (not number)
            {
                return std::unexpected(invalid(child, "float"));
            }
            return *number;
        }

        std::expected<bool, std::string> boolean(tinyxml2::XMLElement const* child)
        {
            std::string_view const value = text(child);
            if (value == "true" or value == "1")
            {
                return true;
            }
            if (value == "false" or value == "0")
            {
                return false;
            }
            return std::unexpected(invalid(child, "boolean"));
        }

        // Returns true when the text is the first choice, false when it is the second.
        std::expected<bool, std::string> choice(tinyxml2::XMLElement const* child, std::string_view first, std::string_view second)
        {
            std::string_view const value = text(child);
            if (value == first)
            {
                return true;
            }
            if (value == second)
            {
                return false;
            }
            return std::unexpected(invalid(child, "value"));
        }

        std::expected<Access, std::string> access(tinyxml2::XMLElement const* child)
        {
            std::string_view const value = text(child);
            if (value == "RO")
            {
                return Access::RO;
            }
            if (value == "WO")
            {
                return Access::WO;
            }
            if (value == "RW")
            {
                return Access::RW;
            }
            return std::unexpected(invalid(child, "access mode"));
        }

        template<typename T, typename U>
        std::expected<bool, std::string> store(T& out, std::expected<U, std::string> value)
        {
            if (not value)
            {
                return std::unexpected(value.error());
            }
            out = *value;
            return true;
        }

        std::expected<void, std::string> check_register(RegisterAccess const& reg, tinyxml2::XMLElement const* element)
        {
            if (reg.port == nullptr)
            {
                return std::unexpected(fail(element, "no <pPort>"));
            }
            auto const* length = std::get_if<int64_t>(&reg.length.source);
            if (length != nullptr and *length <= 0)
            {
                return std::unexpected(fail(element, "no <Length>"));
            }
            return {};
        }

        // Integer and float registers hold 1 to 8 bytes; their handlers reject pLength.
        std::expected<void, std::string> check_int_register(RegisterAccess const& reg, tinyxml2::XMLElement const* element)
        {
            auto valid = check_register(reg, element);
            if (not valid)
            {
                return valid;
            }
            if (reg.bits() > 64)
            {
                return std::unexpected(fail(element, "length above 8 bytes"));
            }
            return {};
        }
    }

    Parser::Parser(Port& port)
        : port_{port}
    {
    }

    std::expected<NodeMap, std::string> Parser::parse(std::string_view xml)
    {
        tinyxml2::XMLDocument document;
        if (document.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS)
        {
            return std::unexpected("XML: " + std::string{document.ErrorStr()});
        }
        tinyxml2::XMLElement const* const root = document.RootElement();
        if (root == nullptr)
        {
            return std::unexpected("XML: no root element");
        }
        if (std::string_view{root->Name()} != "RegisterDescription")
        {
            return std::unexpected("XML: root element is <" + std::string{root->Name()} + ">, not <RegisterDescription>");
        }
        auto created = create(root);
        if (not created)
        {
            return std::unexpected(created.error());
        }
        for (auto const& step : pending_)
        {
            auto configured = step();
            if (not configured)
            {
                return std::unexpected(configured.error());
            }
        }
        auto acyclic = check_cycles();
        if (not acyclic)
        {
            return std::unexpected(acyclic.error());
        }
        return std::move(map_);
    }

    template<typename T>
    std::unique_ptr<T> Parser::make(std::string name, tinyxml2::XMLElement const* element)
    {
        auto node = std::make_unique<T>(std::move(name));
        order_.push_back(node.get());
        pending_.push_back([this, typed = node.get(), element]
        {
            current_ = typed;
            return configure(*typed, element);
        });
        return node;
    }

    std::expected<void, std::string> Parser::create(tinyxml2::XMLElement const* parent)
    {
        for (auto element = parent->FirstChildElement(); element != nullptr; element = element->NextSiblingElement())
        {
            std::string_view const tag = element->Name();
            if (tag == "Group")
            {
                auto created = create(element);
                if (not created)
                {
                    return created;
                }
                continue;
            }

            auto created = create_node(element);
            if (not created)
            {
                return created;
            }

            // An IntSwissKnife inside a register adds to its address.
            for (auto child = element->FirstChildElement("IntSwissKnife"); child != nullptr; child = child->NextSiblingElement("IntSwissKnife"))
            {
                auto knife = new_name(child);
                if (not knife)
                {
                    return std::unexpected(knife.error());
                }
                map_.add(make<SwissKnifeNode<Integer, int64_t>>(std::move(*knife), child));
            }
        }
        return {};
    }

    std::expected<void, std::string> Parser::create_node(tinyxml2::XMLElement const* element)
    {
        std::string_view const tag = element->Name();
        // A StructReg is no node: each of its StructEntry elements is a MaskedIntReg.
        if (tag == "StructReg")
        {
            for (auto entry = element->FirstChildElement("StructEntry"); entry != nullptr; entry = entry->NextSiblingElement("StructEntry"))
            {
                auto name = new_name(entry);
                if (not name)
                {
                    return std::unexpected(name.error());
                }
                map_.add(make<MaskedIntRegNode>(std::move(*name), entry));
            }
            return {};
        }

        auto checked = new_name(element);
        if (not checked)
        {
            return std::unexpected(checked.error());
        }
        std::string name = std::move(*checked);

        if (tag == "Node")              { map_.add(make<NodeBase<Node>>(std::move(name), element)); }
        else if (tag == "Category")     { map_.add(make<CategoryNode>(std::move(name), element)); }
        else if (tag == "Integer")      { map_.add(make<IntegerNode>(std::move(name), element)); }
        else if (tag == "IntReg")       { map_.add(make<IntRegNode>(std::move(name), element)); }
        else if (tag == "MaskedIntReg") { map_.add(make<MaskedIntRegNode>(std::move(name), element)); }
        else if (tag == "Float")        { map_.add(make<FloatNode>(std::move(name), element)); }
        else if (tag == "FloatReg")     { map_.add(make<FloatRegNode>(std::move(name), element)); }
        else if (tag == "Boolean")      { map_.add(make<BooleanNode>(std::move(name), element)); }
        else if (tag == "Command")      { map_.add(make<CommandNode>(std::move(name), element)); }
        else if (tag == "String")       { map_.add(make<StringNode>(std::move(name), element)); }
        else if (tag == "StringReg")    { map_.add(make<StringRegNode>(std::move(name), element)); }
        else if (tag == "Register")     { map_.add(make<RegisterNode>(std::move(name), element)); }
        else if (tag == "SwissKnife")    { map_.add(make<SwissKnifeNode<Float, double>>(std::move(name), element)); }
        else if (tag == "IntSwissKnife") { map_.add(make<SwissKnifeNode<Integer, int64_t>>(std::move(name), element)); }
        else if (tag == "Converter")     { map_.add(make<ConverterNode<Float, double>>(std::move(name), element)); }
        else if (tag == "IntConverter")  { map_.add(make<ConverterNode<Integer, int64_t>>(std::move(name), element)); }
        else if (tag == "Port")
        {
            PortNode& port = map_.add(make<PortNode>(std::move(name), element));
            ports_.emplace(port.name(), &port);
        }
        else if (tag == "Enumeration")
        {
            EnumerationNode& enumeration = map_.add(make<EnumerationNode>(std::move(name), element));
            auto created = create_entries(enumeration, element);
            if (not created)
            {
                return created;
            }
        }
        else
        {
            return std::unexpected(fail(element, "unsupported node type"));
        }
        return {};
    }

    std::expected<std::string, std::string> Parser::new_name(tinyxml2::XMLElement const* element) const
    {
        std::string name{name_of(element)};
        if (name.empty())
        {
            return std::unexpected(fail(element, "no Name attribute"));
        }
        if (map_.get(name) != nullptr)
        {
            return std::unexpected(fail(element, "duplicate name"));
        }
        return name;
    }

    std::expected<void, std::string> Parser::create_entries(EnumerationNode& enumeration, tinyxml2::XMLElement const* element)
    {
        for (auto child = element->FirstChildElement("EnumEntry"); child != nullptr; child = child->NextSiblingElement("EnumEntry"))
        {
            std::string name{name_of(child)};
            if (name.empty())
            {
                return std::unexpected(fail(child, "no Name attribute"));
            }
            enumeration.entry_nodes.push_back(make<EnumEntryNode>(std::move(name), child));
        }
        return {};
    }

    template<typename Handler>
    std::expected<void, std::string> Parser::each(Conditions& conditions, tinyxml2::XMLElement const* element, Handler handler)
    {
        for (auto child = element->FirstChildElement(); child != nullptr; child = child->NextSiblingElement())
        {
            std::string_view const tag = child->Name();
            std::expected<bool, std::string> handled = handler(child, tag);
            if (handled and not *handled)
            {
                handled = common(conditions, child);
            }
            if (not handled)
            {
                return std::unexpected(handled.error());
            }
            if (not *handled)
            {
                return std::unexpected(fail(element, "unsupported element <" + std::string{tag} + ">"));
            }
        }
        return {};
    }

    std::expected<bool, std::string> Parser::common(Conditions& conditions, tinyxml2::XMLElement const* child)
    {
        std::string_view const tag = child->Name();
        if (tag == "pIsImplemented")    { return store(conditions.is_implemented, int_value(child)); }
        if (tag == "pIsAvailable")      { return store(conditions.is_available, int_value(child)); }
        if (tag == "pIsLocked")         { return store(conditions.is_locked, int_value(child)); }
        if (tag == "ImposedAccessMode") { return store(conditions.imposed, access(child)); }
        return std::ranges::find(IGNORED, tag) != IGNORED.end();
    }

    std::expected<bool, std::string> Parser::register_element(RegisterAccess& reg, tinyxml2::XMLElement const* child)
    {
        std::string_view const tag = child->Name();
        if (tag == "Address")       { return store(reg.offsets.emplace_back().source, integer(child)); }
        if (tag == "IntSwissKnife")
        {
            Integer* knife = map_.get<Integer>(name_of(child));
            references_[current_].push_back(knife);
            reg.offsets.emplace_back(knife);
            return true;
        }
        if (tag == "pIndex")
        {
            RegisterAccess::Index& term = reg.indexes.emplace_back();
            auto index = store(term.index, int_value(child));
            if (not index)
            {
                return index;
            }
            char const* offset = child->Attribute("Offset");
            if (offset != nullptr)
            {
                auto value = to_int(offset);
                if (not value)
                {
                    return std::unexpected(fail(child->Parent()->ToElement(), "invalid Offset '" + std::string{offset} + "' in <pIndex>"));
                }
                term.offset = IntValue{*value};
            }
            char const* p_offset = child->Attribute("pOffset");
            if (p_offset != nullptr)
            {
                Integer* node = map_.get<Integer>(p_offset);
                if (node == nullptr)
                {
                    return std::unexpected(fail(child->Parent()->ToElement(), "unknown integer '" + std::string{p_offset} + "' in pOffset"));
                }
                references_[current_].push_back(node);
                term.offset = IntValue{node};
            }
            return true;
        }
        if (tag == "pAddress")   { return store(reg.offsets.emplace_back(), int_value(child)); }
        if (tag == "Length")     { return store(reg.length.source, integer(child)); }
        if (tag == "pLength")    { return store(reg.length, int_value(child)); }
        if (tag == "AccessMode") { return store(reg.mode, access(child)); }
        if (tag == "Endianess")  { return store(reg.big_endian, choice(child, "BigEndian", "LittleEndian")); }
        if (tag == "pPort")
        {
            auto port = ports_.find(text(child));
            if (port == ports_.end())
            {
                return std::unexpected(not_found(child, "a port"));
            }
            reg.port = port->second;
            return true;
        }
        return false;
    }

    std::string Parser::not_found(tinyxml2::XMLElement const* child, std::string_view kind) const
    {
        std::string const name{text(child)};
        std::string const tag{child->Name()};
        if (map_.get(name) == nullptr)
        {
            return fail(child->Parent()->ToElement(), "unknown node '" + name + "' in <" + tag + ">");
        }
        return fail(child->Parent()->ToElement(), "<" + tag + "> '" + name + "' is not " + std::string{kind});
    }

    std::expected<IntValue, std::string> Parser::int_value(tinyxml2::XMLElement const* child)
    {
        std::string_view const name = text(child);
        Integer* integer = map_.get<Integer>(name);
        if (integer != nullptr)
        {
            references_[current_].push_back(integer);
            return IntValue{integer};
        }
        Enumeration* enumeration = map_.get<Enumeration>(name);
        if (enumeration != nullptr)
        {
            references_[current_].push_back(enumeration);
            return IntValue{enumeration};
        }
        Boolean* boolean = map_.get<Boolean>(name);
        if (boolean != nullptr)
        {
            references_[current_].push_back(boolean);
            return IntValue{boolean};
        }
        Float* real = map_.get<Float>(name);
        if (real != nullptr)
        {
            references_[current_].push_back(real);
            return IntValue{real};
        }
        return std::unexpected(not_found(child, "an integer, float, enumeration or boolean"));
    }

    std::expected<FloatValue, std::string> Parser::float_value(tinyxml2::XMLElement const* child)
    {
        std::string_view const name = text(child);
        Float* real = map_.get<Float>(name);
        if (real != nullptr)
        {
            references_[current_].push_back(real);
            return FloatValue{real};
        }
        Integer* integer = map_.get<Integer>(name);
        if (integer != nullptr)
        {
            references_[current_].push_back(integer);
            return FloatValue{integer};
        }
        Enumeration* enumeration = map_.get<Enumeration>(name);
        if (enumeration != nullptr)
        {
            references_[current_].push_back(enumeration);
            return FloatValue{enumeration};
        }
        return std::unexpected(not_found(child, "a float, integer or enumeration"));
    }

    std::expected<void, std::string> Parser::configure(NodeBase<Node>& node, tinyxml2::XMLElement const* element)
    {
        return each(node.conditions, element, [](tinyxml2::XMLElement const*, std::string_view) -> std::expected<bool, std::string>
        {
            return false;
        });
    }

    std::expected<void, std::string> Parser::configure(CategoryNode& node, tinyxml2::XMLElement const* element)
    {
        return each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag != "pFeature")
            {
                return false;
            }
            // Vendors list features their XML does not define: skip them.
            Node* feature = map_.get(text(child));
            if (feature != nullptr)
            {
                node.feature_list.push_back(feature);
            }
            return true;
        });
    }

    std::expected<void, std::string> Parser::configure(PortNode& node, tinyxml2::XMLElement const* element)
    {
        node.port = &port_;
        return each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "ChunkID" or tag == "pChunkID")
            {
                node.chunk = true;
                return true;
            }
            if (tag == "SwapEndianess")
            {
                auto swap = choice(child, "Yes", "No");
                if (swap and *swap)
                {
                    return std::unexpected(fail(element, "unsupported <SwapEndianess> Yes"));
                }
                return swap.transform([](bool) { return true; });
            }
            return false;
        });
    }

    std::expected<void, std::string> Parser::configure(IntegerNode& node, tinyxml2::XMLElement const* element)
    {
        return each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "Value" or tag == "ValueDefault")   { return store(node.value.source, integer(child)); }
            if (tag == "pValue" or tag == "pValueDefault") { return store(node.value, int_value(child)); }
            if (tag == "pIndex")     { return store(node.index.emplace(), int_value(child)); }
            if (tag == "pValueCopy") { return store(node.copies.emplace_back(), int_value(child)); }
            if (tag == "ValueIndexed" or tag == "pValueIndexed")
            {
                auto key = index_key(child);
                if (not key)
                {
                    return std::unexpected(key.error());
                }
                IntValue& entry = node.indexed.emplace_back(*key, IntValue{}).second;
                if (tag == "ValueIndexed")
                {
                    return store(entry.source, integer(child));
                }
                return store(entry, int_value(child));
            }
            if (tag == "Min")    { return store(node.minimum.emplace().source, integer(child)); }
            if (tag == "pMin")   { return store(node.minimum.emplace(), int_value(child)); }
            if (tag == "Max")    { return store(node.maximum.emplace().source, integer(child)); }
            if (tag == "pMax")   { return store(node.maximum.emplace(), int_value(child)); }
            if (tag == "Inc")    { return store(node.increment.emplace().source, integer(child)); }
            if (tag == "pInc")   { return store(node.increment.emplace(), int_value(child)); }
            return false;
        });
    }

    std::expected<void, std::string> Parser::configure(FloatNode& node, tinyxml2::XMLElement const* element)
    {
        return each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "Value" or tag == "ValueDefault")   { return store(node.value.source, real(child)); }
            if (tag == "pValue" or tag == "pValueDefault") { return store(node.value, float_value(child)); }
            if (tag == "pIndex") { return store(node.index.emplace(), int_value(child)); }
            if (tag == "ValueIndexed" or tag == "pValueIndexed")
            {
                auto key = index_key(child);
                if (not key)
                {
                    return std::unexpected(key.error());
                }
                FloatValue& entry = node.indexed.emplace_back(*key, FloatValue{}).second;
                if (tag == "ValueIndexed")
                {
                    return store(entry.source, real(child));
                }
                return store(entry, float_value(child));
            }
            if (tag == "Min")    { return store(node.minimum.emplace().source, real(child)); }
            if (tag == "pMin")   { return store(node.minimum.emplace(), float_value(child)); }
            if (tag == "Max")    { return store(node.maximum.emplace().source, real(child)); }
            if (tag == "pMax")   { return store(node.maximum.emplace(), float_value(child)); }
            if (tag == "Inc")    { return store(node.increment.emplace().source, real(child)); }
            if (tag == "pInc")   { return store(node.increment.emplace(), float_value(child)); }
            return false;
        });
    }

    std::expected<void, std::string> Parser::configure(BooleanNode& node, tinyxml2::XMLElement const* element)
    {
        std::optional<bool> constant;
        auto configured = each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "Value")    { return store(constant, boolean(child)); }
            if (tag == "pValue")   { return store(node.value, int_value(child)); }
            if (tag == "OnValue")  { return store(node.on, integer(child)); }
            if (tag == "OffValue") { return store(node.off, integer(child)); }
            return false;
        });
        if (constant)
        {
            node.value.source = node.off;
            if (*constant)
            {
                node.value.source = node.on;
            }
        }
        return configured;
    }

    std::expected<void, std::string> Parser::configure(CommandNode& node, tinyxml2::XMLElement const* element)
    {
        return each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "Value")         { return store(node.value.source, integer(child)); }
            if (tag == "pValue")        { return store(node.value, int_value(child)); }
            if (tag == "CommandValue")  { return store(node.command_value.source, integer(child)); }
            if (tag == "pCommandValue") { return store(node.command_value, int_value(child)); }
            return false;
        });
    }

    std::expected<void, std::string> Parser::configure(EnumerationNode& node, tinyxml2::XMLElement const* element)
    {
        return each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "EnumEntry") { return true; }
            if (tag == "Value")     { return store(node.value.source, integer(child)); }
            if (tag == "pValue")    { return store(node.value, int_value(child)); }
            return false;
        });
    }

    std::expected<void, std::string> Parser::configure(EnumEntryNode& node, tinyxml2::XMLElement const* element)
    {
        return each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "Value") { return store(node.entry_value, integer(child)); }
            return false;
        });
    }

    std::expected<void, std::string> Parser::configure(StringNode& node, tinyxml2::XMLElement const* element)
    {
        return each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "Value")
            {
                node.constant = raw(child);
                return true;
            }
            if (tag == "pValue")
            {
                node.node = map_.get<String>(text(child));
                if (node.node == nullptr)
                {
                    return std::unexpected(not_found(child, "a string"));
                }
                references_[current_].push_back(node.node);
                return true;
            }
            return false;
        });
    }

    std::expected<void, std::string> Parser::configure(IntRegNode& node, tinyxml2::XMLElement const* element)
    {
        auto configured = each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "Sign")    { return store(node.is_signed, choice(child, "Signed", "Unsigned")); }
            if (tag == "pLength") { return false; }
            return register_element(node.reg, child);
        });
        if (not configured)
        {
            return configured;
        }
        return check_int_register(node.reg, element);
    }

    std::expected<void, std::string> Parser::configure(MaskedIntRegNode& node, tinyxml2::XMLElement const* element)
    {
        std::optional<int64_t> lsb;
        std::optional<int64_t> msb;
        auto handler = [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "StructEntry") { return true; }
            if (tag == "Sign")    { return store(node.is_signed, choice(child, "Signed", "Unsigned")); }
            if (tag == "LSB")     { return store(lsb, integer(child)); }
            if (tag == "MSB")     { return store(msb, integer(child)); }
            if (tag == "pLength") { return false; }
            if (tag == "Bit")
            {
                auto stored = store(lsb, integer(child));
                msb = lsb;
                return stored;
            }
            return register_element(node.reg, child);
        };
        // A StructEntry reads the elements its StructReg shares with the other entries first.
        if (std::string_view{element->Name()} == "StructEntry")
        {
            auto shared = each(node.conditions, element->Parent()->ToElement(), handler);
            if (not shared)
            {
                return shared;
            }
        }
        auto configured = each(node.conditions, element, handler);
        if (not configured)
        {
            return configured;
        }
        auto valid = check_int_register(node.reg, element);
        if (not valid)
        {
            return valid;
        }
        if (not lsb or not msb)
        {
            return std::unexpected(fail(element, "no <Bit>, or no <LSB> and <MSB>"));
        }

        int64_t const width = node.reg.bits();
        if (*lsb < 0 or *lsb >= width or *msb < 0 or *msb >= width)
        {
            return std::unexpected(fail(element, "invalid bit range"));
        }
        // On a big-endian register, bit 0 is the most significant bit.
        int64_t low = *lsb;
        int64_t high = *msb;
        if (node.reg.big_endian)
        {
            low = width - 1 - *lsb;
            high = width - 1 - *msb;
        }
        if (low > high)
        {
            return std::unexpected(fail(element, "invalid bit range"));
        }
        node.lsb = static_cast<uint32_t>(low);
        node.msb = static_cast<uint32_t>(high);
        return {};
    }

    std::expected<void, std::string> Parser::configure(FloatRegNode& node, tinyxml2::XMLElement const* element)
    {
        auto configured = each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "pLength") { return false; }
            return register_element(node.reg, child);
        });
        if (not configured)
        {
            return configured;
        }
        auto valid = check_int_register(node.reg, element);
        if (not valid)
        {
            return valid;
        }
        if (node.reg.bits() != 32 and node.reg.bits() != 64)
        {
            return std::unexpected(fail(element, "length is not 4 or 8 bytes"));
        }
        return {};
    }

    template<typename T>
    requires std::same_as<T, StringRegNode> or std::same_as<T, RegisterNode>
    std::expected<void, std::string> Parser::configure(T& node, tinyxml2::XMLElement const* element)
    {
        auto configured = each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view) -> std::expected<bool, std::string>
        {
            return register_element(node.reg, child);
        });
        if (not configured)
        {
            return configured;
        }
        return check_register(node.reg, element);
    }

    template<typename T>
    std::expected<bool, std::string> Parser::variable_element(Variables& variables, std::vector<std::string_view>& expressions,
                                                              tinyxml2::XMLElement const* child)
    {
        std::string_view const tag = child->Name();
        Variables::Variable variable{.name = std::string{name_of(child)}};
        if (tag == "Expression")
        {
            variable.source = Variables::Expression{expressions.size()};
            expressions.push_back(raw(child));
        }
        else if (tag == "Constant")
        {
            if constexpr (std::is_integral_v<T>)
            {
                auto stored = store(variable.source, integer(child));
                if (not stored)
                {
                    return stored;
                }
            }
            else
            {
                auto stored = store(variable.source, real(child));
                if (not stored)
                {
                    return stored;
                }
            }
        }
        else if (tag == "pVariable")
        {
            auto resolved = resolve_variable(variable, child);
            if (not resolved)
            {
                return std::unexpected(resolved.error());
            }
        }
        else
        {
            return false;
        }
        variables.variables.push_back(std::move(variable));
        return true;
    }

    // The Name may end with .Value, .Min, .Max, .Inc, or .Entry.<entry> to read an enumeration entry's value.
    std::expected<void, std::string> Parser::resolve_variable(Variables::Variable& variable, tinyxml2::XMLElement const* child)
    {
        std::string_view suffix;
        std::size_t const dot = variable.name.find('.');
        if (dot != std::string::npos)
        {
            suffix = std::string_view{variable.name}.substr(dot + 1);
        }

        if (suffix.starts_with("Entry."))
        {
            Enumeration* enumeration = map_.get<Enumeration>(text(child));
            if (enumeration == nullptr)
            {
                return std::unexpected(not_found(child, "an enumeration"));
            }
            std::string_view const entry_name = suffix.substr(6);
            auto entries = enumeration->entries();
            auto entry = std::ranges::find(entries, entry_name, &EnumEntry::name);
            if (entry == entries.end())
            {
                return std::unexpected(fail(child->Parent()->ToElement(), "unknown entry '" + std::string{entry_name} + "' in <pVariable>"));
            }
            variable.source = *entry;
            return {};
        }

        auto target = int_value(child);
        if (not target)
        {
            return std::unexpected(target.error());
        }
        std::visit([&](auto source) { variable.source = source; }, target->source);
        if (suffix == "Min")      { variable.property = Variables::Property::Min; }
        else if (suffix == "Max") { variable.property = Variables::Property::Max; }
        else if (suffix == "Inc") { variable.property = Variables::Property::Inc; }
        else if (not suffix.empty() and suffix != "Value")
        {
            return std::unexpected(fail(child->Parent()->ToElement(), "unsupported <pVariable> '" + variable.name + "'"));
        }
        bool const numeric = std::holds_alternative<Integer*>(variable.source) or std::holds_alternative<Float*>(variable.source);
        if (variable.property != Variables::Property::Value and not numeric)
        {
            return std::unexpected(fail(child->Parent()->ToElement(), "<pVariable> '" + variable.name + "' needs an integer or a float"));
        }
        return {};
    }

    std::expected<void, std::string> Parser::finish(Variables& variables, std::span<std::string_view const> expressions,
                                                    tinyxml2::XMLElement const* element) const
    {
        for (std::string_view text : expressions)
        {
            auto parsed = formula(variables, element, "Expression", text);
            if (not parsed)
            {
                return std::unexpected(parsed.error());
            }
            variables.expressions.push_back(std::move(*parsed));
        }
        return {};
    }

    std::expected<Formula, std::string> Parser::formula(Variables const& variables, tinyxml2::XMLElement const* element,
                                                        std::string_view tag, std::optional<std::string_view> text) const
    {
        if (not text)
        {
            return std::unexpected(fail(element, "no <" + std::string{tag} + ">"));
        }
        auto parsed = Formula::parse(*text, [&](std::string_view name) { return variables.find(name); });
        if (not parsed)
        {
            return std::unexpected(fail(element, "<" + std::string{tag} + ">: " + parsed.error()));
        }
        return parsed;
    }

    template<typename Interface, typename T>
    std::expected<void, std::string> Parser::configure(SwissKnifeNode<Interface, T>& node, tinyxml2::XMLElement const* element)
    {
        std::vector<std::string_view> expressions;
        std::optional<std::string_view> text;
        auto configured = each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "Formula")
            {
                text = raw(child);
                return true;
            }
            return variable_element<T>(node.variables, expressions, child);
        });
        if (not configured)
        {
            return configured;
        }
        auto finished = finish(node.variables, expressions, element);
        if (not finished)
        {
            return finished;
        }
        return store(node.formula, formula(node.variables, element, "Formula", text)).transform([](bool) {});
    }

    template<typename Interface, typename T>
    std::expected<void, std::string> Parser::configure(ConverterNode<Interface, T>& node, tinyxml2::XMLElement const* element)
    {
        std::vector<std::string_view> expressions;
        std::optional<std::string_view> to;
        std::optional<std::string_view> from;
        auto configured = each(node.conditions, element, [&](tinyxml2::XMLElement const* child, std::string_view tag) -> std::expected<bool, std::string>
        {
            if (tag == "FormulaTo")
            {
                to = raw(child);
                return true;
            }
            if (tag == "FormulaFrom")
            {
                from = raw(child);
                return true;
            }
            if (tag == "Slope")
            {
                node.varying = text(child) == "Varying";
                return true;
            }
            if (tag == "pValue")
            {
                if constexpr (std::is_integral_v<T>)
                {
                    return store(node.value, int_value(child));
                }
                else
                {
                    return store(node.value, float_value(child));
                }
            }
            return variable_element<T>(node.variables, expressions, child);
        });
        if (not configured)
        {
            return configured;
        }
        auto finished = finish(node.variables, expressions, element);
        if (not finished)
        {
            return finished;
        }
        auto stored = store(node.to, formula(node.variables, element, "FormulaTo", to));
        if (not stored)
        {
            return std::unexpected(stored.error());
        }
        return store(node.from, formula(node.variables, element, "FormulaFrom", from)).transform([](bool) {});
    }

    std::expected<void, std::string> Parser::check_cycles() const
    {
        enum class Mark
        {
            Visiting,
            Done,
        };
        std::unordered_map<Node const*, Mark> marks;
        std::vector<Node const*> path;
        std::function<std::optional<std::string>(Node const*)> visit = [&](Node const* node) -> std::optional<std::string>
        {
            auto mark = marks.find(node);
            if (mark != marks.end())
            {
                if (mark->second == Mark::Done)
                {
                    return std::nullopt;
                }
                std::string cycle;
                for (auto it = std::ranges::find(path, node); it != path.end(); ++it)
                {
                    cycle += (*it)->name() + " -> ";
                }
                return cycle + node->name();
            }
            marks.emplace(node, Mark::Visiting);
            path.push_back(node);
            auto references = references_.find(node);
            if (references != references_.end())
            {
                for (Node const* target : references->second)
                {
                    auto cycle = visit(target);
                    if (cycle)
                    {
                        return cycle;
                    }
                }
            }
            path.pop_back();
            marks[node] = Mark::Done;
            return std::nullopt;
        };
        for (Node const* node : order_)
        {
            auto cycle = visit(node);
            if (cycle)
            {
                return std::unexpected("cycle in references: " + *cycle);
            }
        }
        return {};
    }
}
