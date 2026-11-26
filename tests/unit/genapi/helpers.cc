#include <algorithm>

#include "helpers.h"

namespace kickcam::genapi
{
    std::expected<void, std::error_code> MemoryPort::read(uint64_t address, std::span<uint8_t> data)
    {
        ++reads;
        if (failure)
        {
            return std::unexpected(failure);
        }
        if (address + data.size() > memory.size())
        {
            return std::unexpected(std::make_error_code(std::errc::bad_address));
        }
        std::ranges::copy_n(memory.begin() + static_cast<std::ptrdiff_t>(address), static_cast<std::ptrdiff_t>(data.size()), data.begin());
        return {};
    }

    std::expected<void, std::error_code> MemoryPort::write(uint64_t address, std::span<uint8_t const> data)
    {
        if (failure)
        {
            return std::unexpected(failure);
        }
        if (address + data.size() > memory.size())
        {
            return std::unexpected(std::make_error_code(std::errc::bad_address));
        }
        std::ranges::copy(data, memory.begin() + static_cast<std::ptrdiff_t>(address));
        return {};
    }

    void MemoryPort::store(uint64_t address, std::initializer_list<uint8_t> bytes)
    {
        std::ranges::copy(bytes, memory.begin() + static_cast<std::ptrdiff_t>(address));
    }

    std::vector<uint8_t> MemoryPort::load(uint64_t address, std::size_t size) const
    {
        auto first = memory.begin() + static_cast<std::ptrdiff_t>(address);
        return {first, first + static_cast<std::ptrdiff_t>(size)};
    }

    std::string description(std::string_view nodes)
    {
        std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
<RegisterDescription ModelName="Test" VendorName="KickCam" StandardNameSpace="None"
    SchemaMajorVersion="1" SchemaMinorVersion="1" SchemaSubMinorVersion="0"
    MajorVersion="1" MinorVersion="0" SubMinorVersion="0"
    ProductGuid="00000000-0000-0000-0000-000000000000" VersionGuid="00000000-0000-0000-0000-000000000000"
    xmlns="http://www.genicam.org/GenApi/Version_1_1">
    <Port Name="Device"/>
)";
        xml += nodes;
        xml += "\n</RegisterDescription>\n";
        return xml;
    }

    testing::AssertionResult NodeMapTest::parse(std::string_view nodes)
    {
        auto parsed = NodeMap::parse(description(nodes), port);
        if (not parsed)
        {
            return testing::AssertionFailure() << parsed.error();
        }
        map.emplace(std::move(*parsed));
        return testing::AssertionSuccess();
    }
}
