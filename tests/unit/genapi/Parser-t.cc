#include <gtest/gtest.h>

#include "helpers.h"

using namespace kickcam::genapi;

namespace
{
    std::string parse_error(std::string_view nodes)
    {
        MemoryPort port;
        auto map = NodeMap::parse(description(nodes), port);
        if (map)
        {
            return "parsed";
        }
        return map.error();
    }
}

TEST(Parser, RejectsMalformedXml)
{
    MemoryPort port;
    auto map = NodeMap::parse("<RegisterDescription>", port);
    ASSERT_FALSE(map);
    EXPECT_TRUE(map.error().starts_with("XML: "));
}

TEST(Parser, RejectsAnotherRoot)
{
    MemoryPort port;
    auto map = NodeMap::parse("<Camera/>", port);
    ASSERT_FALSE(map);
    EXPECT_EQ(map.error(), "XML: root element is <Camera>, not <RegisterDescription>");
}

TEST(Parser, RejectsADocumentWithoutElements)
{
    MemoryPort port;
    auto map = NodeMap::parse("<?xml version=\"1.0\"?><!-- only a comment -->", port);
    ASSERT_FALSE(map);
    EXPECT_EQ(map.error(), "XML: no root element");
}

TEST(Parser, NamesUnsupportedNodeTypes)
{
    EXPECT_EQ(parse_error(R"(<SwissKnife Name="Gain"><Formula>1</Formula></SwissKnife>)"),
              "SwissKnife 'Gain': unsupported node type");
}

TEST(Parser, NamesUnsupportedElements)
{
    EXPECT_EQ(parse_error(R"(<Integer Name="Width"><pIndex>Index</pIndex><ValueIndexed Index="0">1</ValueIndexed><ValueDefault>0</ValueDefault></Integer>
                             <Integer Name="Index"><Value>0</Value></Integer>)"),
              "Integer 'Width': unsupported element <pIndex>");
}

TEST(Parser, NamesUnknownReferences)
{
    EXPECT_EQ(parse_error(R"(<Integer Name="Width"><pValue>Missing</pValue></Integer>)"),
              "Integer 'Width': unknown node 'Missing' in <pValue>");
}

TEST(Parser, RejectsReferencesOfTheWrongType)
{
    EXPECT_EQ(parse_error(R"(<Integer Name="Width"><pValue>Model</pValue></Integer>
                             <String Name="Model"><Value>KC1</Value></String>)"),
              "Integer 'Width': <pValue> 'Model' is not an integer, float, enumeration or boolean");
    EXPECT_EQ(parse_error(R"(<Float Name="Gain"><pValue>Model</pValue></Float>
                             <String Name="Model"><Value>KC1</Value></String>)"),
              "Float 'Gain': <pValue> 'Model' is not a float, integer or enumeration");
    EXPECT_EQ(parse_error(R"(<String Name="Model"><pValue>Width</pValue></String>
                             <Integer Name="Width"><Value>1</Value></Integer>)"),
              "String 'Model': <pValue> 'Width' is not a string");
    EXPECT_EQ(parse_error(R"(<IntReg Name="Width"><Address>0</Address><Length>4</Length><pPort>Height</pPort></IntReg>
                             <Integer Name="Height"><Value>1</Value></Integer>)"),
              "IntReg 'Width': <pPort> 'Height' is not a port");
}

TEST(Parser, RejectsDuplicateAndMissingNames)
{
    EXPECT_EQ(parse_error(R"(<Integer Name="Width"><Value>1</Value></Integer>
                             <Integer Name="Width"><Value>2</Value></Integer>)"),
              "Integer 'Width': duplicate name");
    EXPECT_EQ(parse_error(R"(<Integer><Value>1</Value></Integer>)"), "Integer '': no Name attribute");
}

TEST(Parser, NamesInvalidNumbers)
{
    EXPECT_EQ(parse_error(R"(<Integer Name="Width"><Value>12a</Value></Integer>)"),
              "Integer 'Width': invalid integer '12a' in <Value>");
    EXPECT_EQ(parse_error(R"(<Float Name="Gain"><Value>1.5.2</Value></Float>)"),
              "Float 'Gain': invalid float '1.5.2' in <Value>");
    EXPECT_EQ(parse_error(R"(<IntReg Name="Width"><Address>0</Address><Length>4</Length><AccessMode>RX</AccessMode><pPort>Device</pPort></IntReg>)"),
              "IntReg 'Width': invalid access mode 'RX' in <AccessMode>");
}

TEST(Parser, ReadsHexAndNegativeIntegers)
{
    MemoryPort port;
    auto map = NodeMap::parse(description(R"(<Integer Name="Hex"><Value>0x1F</Value></Integer>
                                              <Integer Name="Negative"><Value> -5 </Value></Integer>
                                              <Integer Name="Full"><Value>0xFFFFFFFFFFFFFFFF</Value></Integer>)"), port);
    ASSERT_TRUE(map) << map.error();
    EXPECT_EQ(map->get<Integer>("Hex")->get(), 31);
    EXPECT_EQ(map->get<Integer>("Negative")->get(), -5);
    EXPECT_EQ(map->get<Integer>("Full")->get(), -1);
}

TEST(Parser, FlattensGroupsAndResolvesForwardReferences)
{
    MemoryPort port;
    auto map = NodeMap::parse(description(R"(<Group Comment="Image"><Group Comment="Size">
                                                <Integer Name="Width"><pValue>WidthReg</pValue></Integer>
                                              </Group></Group>
                                              <IntReg Name="WidthReg"><Address>0</Address><Length>1</Length><pPort>Device</pPort></IntReg>)"), port);
    ASSERT_TRUE(map) << map.error();
    port.store(0, {42});
    EXPECT_EQ(map->get<Integer>("Width")->get(), 42);
}

TEST(Parser, AcceptsDescriptiveElements)
{
    MemoryPort port;
    auto map = NodeMap::parse(description(R"(<Integer Name="Width" NameSpace="Standard">
                                                <ToolTip>Width of the image</ToolTip>
                                                <Description>Width of the image</Description>
                                                <DisplayName>Width</DisplayName>
                                                <Visibility>Beginner</Visibility>
                                                <pInvalidator>Width</pInvalidator>
                                                <Value>640</Value>
                                                <Unit>px</Unit>
                                                <Representation>Linear</Representation>
                                              </Integer>)"), port);
    ASSERT_TRUE(map) << map.error();
    EXPECT_EQ(map->get<Integer>("Width")->get(), 640);
}

TEST(Parser, ChecksRegisterDefinitions)
{
    EXPECT_EQ(parse_error(R"(<IntReg Name="Width"><Address>0</Address><Length>4</Length></IntReg>)"),
              "IntReg 'Width': no <pPort>");
    EXPECT_EQ(parse_error(R"(<StringReg Name="Model"><Address>0</Address><pPort>Device</pPort></StringReg>)"),
              "StringReg 'Model': no <Length>");
    EXPECT_EQ(parse_error(R"(<IntReg Name="Width"><Address>0</Address><Length>16</Length><pPort>Device</pPort></IntReg>)"),
              "IntReg 'Width': length above 8 bytes");
    EXPECT_EQ(parse_error(R"(<FloatReg Name="Gain"><Address>0</Address><Length>2</Length><pPort>Device</pPort></FloatReg>)"),
              "FloatReg 'Gain': length is not 4 or 8 bytes");
    EXPECT_EQ(parse_error(R"(<MaskedIntReg Name="Flag"><Address>0</Address><Length>4</Length><pPort>Device</pPort></MaskedIntReg>)"),
              "MaskedIntReg 'Flag': no <Bit>, or no <LSB> and <MSB>");
    EXPECT_EQ(parse_error(R"(<MaskedIntReg Name="Flag"><Address>0</Address><Length>1</Length><pPort>Device</pPort><LSB>2</LSB><MSB>9</MSB></MaskedIntReg>)"),
              "MaskedIntReg 'Flag': invalid bit range");
    EXPECT_EQ(parse_error(R"(<MaskedIntReg Name="Flag"><Address>0</Address><Length>4</Length><pPort>Device</pPort>
                             <LSB>9223372036854775807</LSB><MSB>0</MSB><Endianess>BigEndian</Endianess></MaskedIntReg>)"),
              "MaskedIntReg 'Flag': invalid bit range");
    EXPECT_EQ(parse_error(R"(<IntReg Name="Width"><Address>0</Address><pLength>Size</pLength><Length>4</Length><pPort>Device</pPort></IntReg>
                             <Integer Name="Size"><Value>4</Value></Integer>)"),
              "IntReg 'Width': unsupported element <pLength>");
    EXPECT_EQ(parse_error(R"(<FloatReg Name="Gain"><Address>0</Address><Length>4</Length><pLength>Size</pLength><pPort>Device</pPort></FloatReg>
                             <Integer Name="Size"><Value>4</Value></Integer>)"),
              "FloatReg 'Gain': unsupported element <pLength>");
    EXPECT_EQ(parse_error(R"(<IntReg Name="Width"><IntSwissKnife Name="Offset"><Formula>4</Formula></IntSwissKnife><Length>4</Length><pPort>Device</pPort></IntReg>)"),
              "IntReg 'Width': unsupported element <IntSwissKnife>");
}

TEST(Parser, RejectsSwappedPorts)
{
    EXPECT_EQ(parse_error(R"(<Port Name="Swapped"><SwapEndianess>Yes</SwapEndianess></Port>)"),
              "Port 'Swapped': unsupported <SwapEndianess> Yes");
}

TEST(Parser, LooksUpNodesByNameAndType)
{
    MemoryPort port;
    auto map = NodeMap::parse(description(R"(<Integer Name="Width"><Value>640</Value></Integer>)"), port);
    ASSERT_TRUE(map) << map.error();
    EXPECT_NE(map->get("Width"), nullptr);
    EXPECT_NE(map->get<Integer>("Width"), nullptr);
    EXPECT_EQ(map->get<Float>("Width"), nullptr);
    EXPECT_EQ(map->get("Height"), nullptr);
    EXPECT_EQ(map->get("Width")->name(), "Width");
}

TEST(Parser, KeepsEnumEntriesOutOfTheMap)
{
    MemoryPort port;
    auto map = NodeMap::parse(description(R"(<Enumeration Name="ExposureAuto"><EnumEntry Name="Off"><Value>0</Value></EnumEntry><Value>0</Value></Enumeration>
                                              <Enumeration Name="GainAuto"><EnumEntry Name="Off"><Value>0</Value></EnumEntry><Value>0</Value></Enumeration>)"), port);
    ASSERT_TRUE(map) << map.error();
    EXPECT_EQ(map->get("Off"), nullptr);
}
