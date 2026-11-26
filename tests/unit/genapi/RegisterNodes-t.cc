#include <gtest/gtest.h>

#include <limits>

#include "helpers.h"

using namespace kickcam;
using namespace kickcam::genapi;

class RegisterNodes : public NodeMapTest
{
};

TEST_F(RegisterNodes, IntRegReadsAndWritesBothByteOrders)
{
    ASSERT_TRUE(parse(R"(<IntReg Name="Little"><Address>0x10</Address><Length>4</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>
             <IntReg Name="Big"><Address>0x20</Address><Length>4</Length><AccessMode>RW</AccessMode><pPort>Device</pPort><Endianess>BigEndian</Endianess></IntReg>)"));
    port.store(0x10, {0x78, 0x56, 0x34, 0x12});
    port.store(0x20, {0x12, 0x34, 0x56, 0x78});
    EXPECT_EQ(map->get<Integer>("Little")->get(), 0x12345678);
    EXPECT_EQ(map->get<Integer>("Big")->get(), 0x12345678);

    EXPECT_TRUE(map->get<Integer>("Little")->set(0x0A0B0C0D));
    EXPECT_EQ(port.load(0x10, 4), (std::vector<uint8_t>{0x0D, 0x0C, 0x0B, 0x0A}));
    EXPECT_TRUE(map->get<Integer>("Big")->set(0x0A0B0C0D));
    EXPECT_EQ(port.load(0x20, 4), (std::vector<uint8_t>{0x0A, 0x0B, 0x0C, 0x0D}));
}

TEST_F(RegisterNodes, IntRegBoundsFollowLengthAndSign)
{
    ASSERT_TRUE(parse(R"(<IntReg Name="U8"><Address>0</Address><Length>1</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>
             <IntReg Name="S16"><Address>2</Address><Length>2</Length><AccessMode>RW</AccessMode><pPort>Device</pPort><Sign>Signed</Sign></IntReg>
             <IntReg Name="U64"><Address>8</Address><Length>8</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>
             <IntReg Name="S64"><Address>16</Address><Length>8</Length><AccessMode>RW</AccessMode><pPort>Device</pPort><Sign>Signed</Sign></IntReg>)"));
    EXPECT_EQ(map->get<Integer>("U8")->min(), 0);
    EXPECT_EQ(map->get<Integer>("U8")->max(), 255);
    EXPECT_EQ(map->get<Integer>("S16")->min(), -32768);
    EXPECT_EQ(map->get<Integer>("S16")->max(), 32767);
    EXPECT_EQ(map->get<Integer>("U64")->max(), std::numeric_limits<int64_t>::max());
    EXPECT_EQ(map->get<Integer>("S64")->min(), std::numeric_limits<int64_t>::min());

    port.store(8, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
    EXPECT_EQ(map->get<Integer>("U64")->get(), -1);

    EXPECT_EQ(error_of(map->get<Integer>("U8")->set(256)), Errc::out_of_range);
    EXPECT_TRUE(map->get<Integer>("S16")->set(-2));
    EXPECT_EQ(port.load(2, 2), (std::vector<uint8_t>{0xFE, 0xFF}));
    EXPECT_EQ(map->get<Integer>("S16")->get(), -2);
}

TEST_F(RegisterNodes, AddressSumsConstantsAndNodes)
{
    ASSERT_TRUE(parse(R"(<IntReg Name="Value"><Address>0x10</Address><Address>0x4</Address><pAddress>Base</pAddress><Length>1</Length><pPort>Device</pPort></IntReg>
             <Integer Name="Base"><Value>0x20</Value></Integer>)"));
    port.store(0x34, {7});
    EXPECT_EQ(map->get<Integer>("Value")->get(), 7);
}

TEST_F(RegisterNodes, AccessModeLimitsTheRegister)
{
    ASSERT_TRUE(parse(R"(<IntReg Name="ReadOnly"><Address>0</Address><Length>1</Length><pPort>Device</pPort></IntReg>
             <IntReg Name="WriteOnly"><Address>1</Address><Length>1</Length><AccessMode>WO</AccessMode><pPort>Device</pPort></IntReg>)"));
    EXPECT_EQ(map->get("ReadOnly")->access(), Access::RO);
    EXPECT_EQ(error_of(map->get<Integer>("ReadOnly")->set(1)), Errc::not_writable);
    EXPECT_EQ(error_of(map->get<Integer>("WriteOnly")->get()), Errc::not_readable);
    EXPECT_TRUE(map->get<Integer>("WriteOnly")->set(1));
}

TEST_F(RegisterNodes, ChunkPortsAreNotAvailable)
{
    ASSERT_TRUE(parse(R"(<Port Name="Chunk"><ChunkID>4000</ChunkID></Port>
             <IntReg Name="Timestamp"><Address>0</Address><Length>8</Length><pPort>Chunk</pPort></IntReg>)"));
    EXPECT_EQ(error_of(map->get<Integer>("Timestamp")->get()), Errc::not_available);
}

TEST_F(RegisterNodes, PortErrorsPropagate)
{
    ASSERT_TRUE(parse(R"(<IntReg Name="Far"><Address>0x1000</Address><Length>4</Length><pPort>Device</pPort></IntReg>)"));
    EXPECT_EQ(error_of(map->get<Integer>("Far")->get()), std::errc::bad_address);
}

TEST_F(RegisterNodes, MaskedIntRegKeepsTheOtherBits)
{
    ASSERT_TRUE(parse(R"(<MaskedIntReg Name="Field"><Address>0</Address><Length>2</Length><AccessMode>RW</AccessMode><pPort>Device</pPort><LSB>4</LSB><MSB>11</MSB></MaskedIntReg>
             <MaskedIntReg Name="Flag"><Address>0</Address><Length>2</Length><AccessMode>RW</AccessMode><pPort>Device</pPort><Bit>15</Bit></MaskedIntReg>
             <MaskedIntReg Name="Signed"><Address>4</Address><Length>1</Length><AccessMode>RW</AccessMode><pPort>Device</pPort><LSB>0</LSB><MSB>3</MSB><Sign>Signed</Sign></MaskedIntReg>)"));
    port.store(0, {0x5F, 0x8A});
    auto field = map->get<Integer>("Field");
    EXPECT_EQ(field->get(), 0xA5);
    EXPECT_EQ(field->max(), 255);
    EXPECT_EQ(map->get<Integer>("Flag")->get(), 1);

    EXPECT_TRUE(field->set(0x3C));
    EXPECT_EQ(port.load(0, 2), (std::vector<uint8_t>{0xCF, 0x83}));

    port.store(4, {0xFE});
    EXPECT_EQ(map->get<Integer>("Signed")->get(), -2);
    EXPECT_EQ(map->get<Integer>("Signed")->min(), -8);
}

TEST_F(RegisterNodes, MaskedIntRegReadsItsAddressOnce)
{
    ASSERT_TRUE(parse(R"(<MaskedIntReg Name="Flag"><pAddress>Base</pAddress><Length>1</Length><AccessMode>RW</AccessMode><pPort>Device</pPort><Bit>0</Bit></MaskedIntReg>
                         <IntReg Name="Base"><Address>0</Address><Length>1</Length><pPort>Device</pPort></IntReg>)"));
    port.store(0, {0x10});
    port.store(0x10, {0xF0});
    port.reads = 0;
    EXPECT_TRUE(map->get<Integer>("Flag")->set(1));
    EXPECT_EQ(port.memory[0x10], 0xF1);
    EXPECT_EQ(port.reads, 2); // the address node, then the register
}

TEST_F(RegisterNodes, MaskedIntRegCountsBigEndianBitsFromTheTop)
{
    ASSERT_TRUE(parse(R"(<MaskedIntReg Name="LowByte"><Address>0</Address><Length>4</Length><AccessMode>RW</AccessMode><pPort>Device</pPort>
                 <LSB>31</LSB><MSB>24</MSB><Endianess>BigEndian</Endianess></MaskedIntReg>
             <MaskedIntReg Name="TopBit"><Address>0</Address><Length>4</Length><AccessMode>RW</AccessMode><pPort>Device</pPort>
                 <Bit>0</Bit><Endianess>BigEndian</Endianess></MaskedIntReg>)"));
    port.store(0, {0x80, 0x00, 0x00, 0x42});
    EXPECT_EQ(map->get<Integer>("LowByte")->get(), 0x42);
    EXPECT_EQ(map->get<Integer>("TopBit")->get(), 1);
}

TEST_F(RegisterNodes, FloatRegReadsBothSizesAndByteOrders)
{
    ASSERT_TRUE(parse(R"(<FloatReg Name="Single"><Address>0</Address><Length>4</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></FloatReg>
             <FloatReg Name="Double"><Address>8</Address><Length>8</Length><AccessMode>RW</AccessMode><pPort>Device</pPort><Endianess>BigEndian</Endianess></FloatReg>)"));
    port.store(0, {0x00, 0x00, 0xC0, 0x3F});
    port.store(8, {0x40, 0x09, 0x21, 0xFB, 0x54, 0x44, 0x2D, 0x18});
    EXPECT_EQ(map->get<Float>("Single")->get(), 1.5);
    EXPECT_EQ(map->get<Float>("Single")->inc(), std::nullopt);
    EXPECT_EQ(map->get<Float>("Double")->get(), 3.141592653589793);

    EXPECT_EQ(map->get<Float>("Single")->max(), std::numeric_limits<float>::max());
    EXPECT_EQ(error_of(map->get<Float>("Single")->set(1e39)), Errc::out_of_range);
    EXPECT_EQ(port.load(0, 4), (std::vector<uint8_t>{0x00, 0x00, 0xC0, 0x3F}));

    EXPECT_TRUE(map->get<Float>("Single")->set(-2.0));
    EXPECT_EQ(port.load(0, 4), (std::vector<uint8_t>{0x00, 0x00, 0x00, 0xC0}));
    EXPECT_TRUE(map->get<Float>("Double")->set(1.0));
    EXPECT_EQ(port.load(8, 8), (std::vector<uint8_t>{0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST_F(RegisterNodes, StringRegStopsAtNulAndPads)
{
    ASSERT_TRUE(parse(R"(<StringReg Name="Model"><Address>0</Address><Length>8</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></StringReg>
             <StringReg Name="Serial"><Address>16</Address><pLength>SerialLength</pLength><pPort>Device</pPort></StringReg>
             <Integer Name="SerialLength"><Value>4</Value></Integer>)"));
    port.store(0, {'M', 'V', 0, 'x', 'x'});
    auto model = map->get<String>("Model");
    EXPECT_EQ(model->get(), "MV");
    EXPECT_TRUE(model->set("CB060"));
    EXPECT_EQ(port.load(0, 8), (std::vector<uint8_t>{'C', 'B', '0', '6', '0', 0, 0, 0}));
    EXPECT_EQ(error_of(model->set("123456789")), Errc::out_of_range);

    port.store(16, {'A', 'B', 'C', 'D', 'E'});
    EXPECT_EQ(map->get<String>("Serial")->get(), "ABCD");
}

TEST_F(RegisterNodes, NegativeDynamicLengthIsOutOfRange)
{
    ASSERT_TRUE(parse(R"(<StringReg Name="Model"><Address>0</Address><pLength>Size</pLength><AccessMode>RW</AccessMode><pPort>Device</pPort></StringReg>
                         <Integer Name="Size"><Value>-1</Value></Integer>)"));
    EXPECT_EQ(error_of(map->get<String>("Model")->get()), Errc::out_of_range);
    EXPECT_EQ(error_of(map->get<String>("Model")->set("A")), Errc::out_of_range);
}

TEST_F(RegisterNodes, RegisterMovesRawBytes)
{
    ASSERT_TRUE(parse(R"(<Register Name="Lut"><Address>0x40</Address><Length>3</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></Register>)"));
    auto lut = map->get<Register>("Lut");
    std::vector<uint8_t> const data{1, 2, 3};
    EXPECT_TRUE(lut->set(data));
    EXPECT_EQ(lut->get(), data);
    EXPECT_EQ(error_of(lut->set(std::vector<uint8_t>{1, 2})), Errc::out_of_range);
}

TEST_F(RegisterNodes, IndexMovesTheAddress)
{
    ASSERT_TRUE(parse(R"(<IntReg Name="Gain"><Address>0x10</Address><pIndex Offset="4">Channel</pIndex><Length>4</Length><pPort>Device</pPort></IntReg>
                         <IntReg Name="Lut"><Address>0x40</Address><pIndex>Channel</pIndex><Length>2</Length><pPort>Device</pPort></IntReg>
                         <IntReg Name="Black"><Address>0x60</Address><pIndex pOffset="Stride">Channel</pIndex><Length>1</Length><pPort>Device</pPort></IntReg>
                         <Integer Name="Channel"><Value>2</Value></Integer>
                         <Integer Name="Stride"><Value>8</Value></Integer>)"));
    port.store(0x18, {7});
    port.store(0x44, {8});
    port.store(0x70, {9});
    EXPECT_EQ(map->get<Integer>("Gain")->get(), 7);
    EXPECT_EQ(map->get<Integer>("Lut")->get(), 8);
    EXPECT_EQ(map->get<Integer>("Black")->get(), 9);
}

TEST_F(RegisterNodes, StructEntriesShareTheirRegister)
{
    ASSERT_TRUE(parse(R"(<StructReg Comment="Flags">
                             <Address>0x20</Address><Length>2</Length><AccessMode>RW</AccessMode><pPort>Device</pPort><Endianess>LittleEndian</Endianess>
                             <StructEntry Name="Enable"><Bit>0</Bit></StructEntry>
                             <StructEntry Name="Mode"><LSB>4</LSB><MSB>7</MSB></StructEntry>
                             <StructEntry Name="Status"><AccessMode>RO</AccessMode><LSB>8</LSB><MSB>15</MSB></StructEntry>
                         </StructReg>)"));
    port.store(0x20, {0x31, 0xAB});
    EXPECT_EQ(map->get<Integer>("Enable")->get(), 1);
    EXPECT_EQ(map->get<Integer>("Mode")->get(), 3);
    EXPECT_EQ(map->get<Integer>("Status")->get(), 0xAB);
    EXPECT_TRUE(map->get<Integer>("Mode")->set(5));
    EXPECT_EQ(port.load(0x20, 2), (std::vector<uint8_t>{0x51, 0xAB}));
    EXPECT_EQ(error_of(map->get<Integer>("Status")->set(1)), Errc::not_writable);
}
