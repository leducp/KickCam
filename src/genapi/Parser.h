#ifndef KICKCAM_GENAPI_PARSER_H
#define KICKCAM_GENAPI_PARSER_H

#include <concepts>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <tinyxml2.h>

#include "kickcam/genapi/NodeMap.h"
#include "genapi/RegisterNodes.h"
#include "genapi/SwissKnifeNodes.h"

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
        std::expected<void, std::string> create_node(tinyxml2::XMLElement const* element);
        std::expected<void, std::string> create_entries(EnumerationNode& enumeration, tinyxml2::XMLElement const* element);

        // Fails when the element has no Name, or a name already in the map.
        std::expected<std::string, std::string> new_name(tinyxml2::XMLElement const* element) const;

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

        template<typename Interface, typename T>
        std::expected<void, std::string> configure(SwissKnifeNode<Interface, T>& node, tinyxml2::XMLElement const* element);
        template<typename Interface, typename T>
        std::expected<void, std::string> configure(ConverterNode<Interface, T>& node, tinyxml2::XMLElement const* element);

        // pVariable, Constant and Expression; returns false for another child. T is the formula's type.
        template<typename T>
        std::expected<bool, std::string> variable_element(Variables& variables, std::vector<std::string_view>& expressions,
                                                          tinyxml2::XMLElement const* child);
        std::expected<void, std::string> resolve_variable(Variables::Variable& variable, tinyxml2::XMLElement const* child);

        // Parses the Expression elements once every name is known.
        std::expected<void, std::string> finish(Variables& variables, std::span<std::string_view const> expressions,
                                                tinyxml2::XMLElement const* element) const;

        // The error names the element and the tag; a missing text gives "no <tag>".
        std::expected<Formula, std::string> formula(Variables const& variables, tinyxml2::XMLElement const* element,
                                                    std::string_view tag, std::optional<std::string_view> text) const;

        // Runs handler on every child element. Children it does not handle must be common to all nodes.
        template<typename Handler>
        std::expected<void, std::string> each(Conditions& conditions, tinyxml2::XMLElement const* element, Handler handler);

        // Returns false when the child is not common to all nodes.
        std::expected<bool, std::string> common(Conditions& conditions, tinyxml2::XMLElement const* child);

        // Returns false when the child is not a register element.
        std::expected<bool, std::string> register_element(RegisterAccess& reg, tinyxml2::XMLElement const* child);

        // "unknown node" when the name is not in the map, "is not <kind>" when the node has another type.
        std::string not_found(tinyxml2::XMLElement const* child, std::string_view kind) const;
        // Both record the reference for check_cycles.
        std::expected<IntValue, std::string> int_value(tinyxml2::XMLElement const* child);
        std::expected<FloatValue, std::string> float_value(tinyxml2::XMLElement const* child);

        // Names the first loop of references found, in document order.
        std::expected<void, std::string> check_cycles() const;

        Port& port_;
        NodeMap map_;
        std::unordered_map<std::string_view, PortNode*> ports_; // views of each port's own name
        std::vector<std::function<std::expected<void, std::string>()>> pending_;

        // What each node reads to evaluate, in the order nodes were created.
        Node const* current_{nullptr};
        std::vector<Node const*> order_;
        std::unordered_map<Node const*, std::vector<Node const*>> references_;
    };
}

#endif
