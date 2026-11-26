#ifndef KICKCAM_TESTS_UNIT_GENAPI_HELPERS_H
#define KICKCAM_TESTS_UNIT_GENAPI_HELPERS_H

#include <gtest/gtest.h>

#include <initializer_list>
#include <optional>
#include <string>

#include "kickcam/Error.h"
#include "kickcam/genapi/NodeMap.h"

namespace kickcam::genapi
{
    // Register space in memory. Every access fails with failure when it is set.
    class MemoryPort : public Port
    {
    public:
        std::expected<void, std::error_code> read(uint64_t address, std::span<uint8_t> data) override;
        std::expected<void, std::error_code> write(uint64_t address, std::span<uint8_t const> data) override;

        void store(uint64_t address, std::initializer_list<uint8_t> bytes);
        std::vector<uint8_t> load(uint64_t address, std::size_t size) const;

        std::vector<uint8_t> memory = std::vector<uint8_t>(256);
        std::error_code failure;
        int reads{0};
    };

    // A register description with a port named Device, followed by the given nodes.
    std::string description(std::string_view nodes);

    class NodeMapTest : public testing::Test
    {
    protected:
        // Fails with the parse error, for ASSERT_TRUE in the test body.
        testing::AssertionResult parse(std::string_view nodes);

        MemoryPort port;
        std::optional<NodeMap> map;
    };

    // Empty when the result holds a value.
    template<typename T>
    std::error_code error_of(std::expected<T, std::error_code> const& result)
    {
        if (result)
        {
            return {};
        }
        return result.error();
    }
}

#endif
