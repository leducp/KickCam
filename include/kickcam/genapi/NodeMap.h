#ifndef KICKCAM_GENAPI_NODE_MAP_H
#define KICKCAM_GENAPI_NODE_MAP_H

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "kickcam/genapi/Node.h"
#include "kickcam/genapi/Port.h"

namespace kickcam::genapi
{
    class NodeMap
    {
    public:
        // The port must outlive the node map. The error names the node and the construct that failed.
        static std::expected<NodeMap, std::string> parse(std::string_view xml, Port& port);

        // The name must be new to the map. The node is found by every interface its type derives from.
        template<typename T>
        T& add(std::unique_ptr<T> node)
        {
            T& added = *node;
            std::apply([&added](auto&... index) { (insert(index, added), ...); }, indexes_);
            nodes_.push_back(std::move(node));
            return added;
        }

        // Returns nullptr when no T has this name.
        template<typename T = Node>
        T* get(std::string_view name) const
        {
            auto const& index = std::get<Index<T>>(indexes_);
            auto it = index.find(name);
            if (it == index.end())
            {
                return nullptr;
            }
            return it->second;
        }

    private:
        // Keys are views of each node's own name.
        template<typename T>
        using Index = std::unordered_map<std::string_view, T*>;

        template<typename Interface, typename T>
        static void insert(Index<Interface>& index, T& node)
        {
            if constexpr (std::is_base_of_v<Interface, T>)
            {
                index.emplace(node.name(), &node);
            }
        }

        std::vector<std::unique_ptr<Node>> nodes_;
        std::tuple<Index<Node>, Index<Integer>, Index<Float>, Index<Boolean>, Index<Command>,
                   Index<Enumeration>, Index<String>, Index<Register>, Index<Category>> indexes_;
    };
}

#endif
