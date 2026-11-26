#include <gtest/gtest.h>

#include "helpers.h"

using namespace kickcam;
using namespace kickcam::genapi;

namespace
{
    class NodeAccess : public NodeMapTest
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(parse(R"(
                <Integer Name="Implemented"><Value>1</Value></Integer>
                <Integer Name="Available"><Value>1</Value></Integer>
                <Integer Name="Locked"><Value>0</Value></Integer>
                <Integer Name="Width">
                    <pIsImplemented>Implemented</pIsImplemented>
                    <pIsAvailable>Available</pIsAvailable>
                    <pIsLocked>Locked</pIsLocked>
                    <Value>640</Value>
                </Integer>
                <Integer Name="ReadOnly"><ImposedAccessMode>RO</ImposedAccessMode><Value>1</Value></Integer>
                <Integer Name="WriteOnly"><ImposedAccessMode>WO</ImposedAccessMode><Value>1</Value></Integer>
                <IntReg Name="Status"><Address>0</Address><Length>1</Length><AccessMode>RO</AccessMode><pPort>Device</pPort></IntReg>
                <Boolean Name="Enabled"><pValue>Status</pValue></Boolean>
                <Integer Name="Gated"><pIsAvailable>Enabled</pIsAvailable><Value>3</Value></Integer>
            )"));
            width = map->get<Integer>("Width");
        }

        void set(std::string_view name, int64_t value)
        {
            ASSERT_TRUE(map->get<Integer>(name)->set(value));
        }

        Integer* width{nullptr};
    };
}

TEST_F(NodeAccess, ConstantIntegerIsReadWrite)
{
    EXPECT_EQ(width->access(), Access::RW);
    EXPECT_TRUE(width->set(320));
    EXPECT_EQ(width->get(), 320);
}

TEST_F(NodeAccess, NotImplemented)
{
    set("Implemented", 0);
    EXPECT_EQ(width->access(), Access::NI);
    EXPECT_EQ(error_of(width->get()), Errc::not_implemented);
    EXPECT_EQ(error_of(width->set(1)), Errc::not_implemented);
}

TEST_F(NodeAccess, NotAvailable)
{
    set("Available", 0);
    EXPECT_EQ(width->access(), Access::NA);
    EXPECT_EQ(error_of(width->get()), Errc::not_available);
    EXPECT_EQ(error_of(width->set(1)), Errc::not_available);
}

TEST_F(NodeAccess, LockedIsReadOnly)
{
    set("Locked", 1);
    EXPECT_EQ(width->access(), Access::RO);
    EXPECT_EQ(width->get(), 640);
    EXPECT_EQ(error_of(width->set(1)), Errc::locked);
}

TEST_F(NodeAccess, ImposedAccessModeRestricts)
{
    auto read_only = map->get<Integer>("ReadOnly");
    EXPECT_EQ(read_only->access(), Access::RO);
    EXPECT_EQ(error_of(read_only->set(2)), Errc::not_writable);

    auto write_only = map->get<Integer>("WriteOnly");
    EXPECT_EQ(write_only->access(), Access::WO);
    EXPECT_EQ(error_of(write_only->get()), Errc::not_readable);
    EXPECT_TRUE(write_only->set(2));
}

TEST_F(NodeAccess, AvailabilityFollowsARegister)
{
    auto gated = map->get<Integer>("Gated");
    EXPECT_EQ(gated->access(), Access::NA);
    port.store(0, {1});
    EXPECT_EQ(gated->access(), Access::RW);
    EXPECT_EQ(gated->get(), 3);
}

TEST_F(NodeAccess, PortErrorsPropagate)
{
    port.failure = std::make_error_code(std::errc::timed_out);
    auto gated = map->get<Integer>("Gated");
    EXPECT_EQ(error_of(gated->access()), std::errc::timed_out);
    EXPECT_EQ(error_of(gated->get()), std::errc::timed_out);
}
