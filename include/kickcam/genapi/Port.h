#ifndef KICKCAM_GENAPI_PORT_H
#define KICKCAM_GENAPI_PORT_H

#include <cstdint>
#include <expected>
#include <span>
#include <system_error>

namespace kickcam::genapi
{
    // Register space of a device, as the GenICam XML addresses it.
    class Port
    {
    public:
        virtual ~Port() = default;

        virtual std::expected<void, std::error_code> read(uint64_t address, std::span<uint8_t> data) = 0;
        virtual std::expected<void, std::error_code> write(uint64_t address, std::span<uint8_t const> data) = 0;
    };
}

#endif
