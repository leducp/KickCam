#ifndef KICKCAM_GENAPI_PARSER_H
#define KICKCAM_GENAPI_PARSER_H

#include <concepts>
#include <functional>
#include <unordered_map>

#include <tinyxml2.h>

#include "kickcam/genapi/NodeMap.h"
#include "genapi/RegisterNodes.h"

namespace kickcam::genapi
{
    class Parser
    {
    public:
        Parser(Port& port);

        std::expected<NodeMap, std::string> parse(std::string_view xml);

    private:
        // Creates every node before any is configured: a reference can name a node defined later.
        std::expected<void, std::string> create(tinyxml2::XMLElement const* parent);
        std::expected<void, std::string> create_entries(EnumerationNode& enumeration, tinyxml2::XMLElement const* element);

        // Creates a node and queues its configuration.
        template<typename T>
        std::unique_ptr<T> make(std::string name, tinyxml2::XMLElement const* element);

        std::expected<void, std::string> configure(NodeBase<Node>& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(CategoryNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(PortNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(IntegerNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(FloatNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(BooleanNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(CommandNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(EnumerationNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(EnumEntryNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(StringNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(IntRegNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(MaskedIntRegNode& node, tinyxml2::XMLElement const* element);
        std::expected<void, std::string> configure(FloatRegNode& node, tinyxml2::XMLElement const* element);

        // StringReg and Register take register elements only.
        template<typename T>
        requires std::same_as<T, StringRegNode> or std::same_as<T, RegisterNode>
        std::expected<void, std::string> configure(T& node, tinyxml2::XMLElement const* element);

        // Runs handler on every child element. Children it does not handle must be common to all nodes.
        template<typename Handler>
        std::expected<void, std::string> each(Conditions& conditions, tinyxml2::XMLElement const* element, Handler handler);

        // Returns false when the child is not common to all nodes.
        std::expected<bool, std::string> common(Conditions& conditions, tinyxml2::XMLElement const* child);

        // Returns false when the child is not a register element.
        std::expected<bool, std::string> register_element(RegisterAccess& reg, tinyxml2::XMLElement const* child);

        // "unknown node" when the name is not in the map, "is not <kind>" when the node has another type.
        std::string not_found(tinyxml2::XMLElement const* child, std::string_view kind) const;
        std::expected<IntValue, std::string> int_value(tinyxml2::XMLElement const* child) const;
        std::expected<FloatValue, std::string> float_value(tinyxml2::XMLElement const* child) const;

        Port& port_;
        NodeMap map_;
        std::unordered_map<std::string_view, PortNode*> ports_; // views of each port's own name
        std::vector<std::function<std::expected<void, std::string>()>> pending_;
    };
}

#endif
