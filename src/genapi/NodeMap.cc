#include "kickcam/genapi/NodeMap.h"
#include "genapi/Parser.h"

namespace kickcam::genapi
{
    std::expected<NodeMap, std::string> NodeMap::parse(std::string_view xml, Port& port)
    {
        return Parser{port}.parse(xml);
    }
}
