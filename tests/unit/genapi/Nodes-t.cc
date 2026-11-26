#include <gtest/gtest.h>

#include <limits>

#include "helpers.h"

using namespace kickcam;
using namespace kickcam::genapi;

class Nodes : public NodeMapTest
{
};

TEST_F(Nodes, IntegerChecksMinMaxAndInc)
{
    ASSERT_TRUE(parse(R"(<Integer Name="Width"><Value>16</Value><Min>16</Min><Max>64</Max><Inc>8</Inc></Integer>)"));
    auto width = map->get<Integer>("Width");
    EXPECT_EQ(width->min(), 16);
    EXPECT_EQ(width->max(), 64);
    EXPECT_EQ(width->inc(), 8);
    EXPECT_TRUE(width->set(40));
    EXPECT_EQ(width->get(), 40);
    EXPECT_EQ(error_of(width->set(8)), Errc::out_of_range);
    EXPECT_EQ(error_of(width->set(72)), Errc::out_of_range);
    EXPECT_EQ(error_of(width->set(42)), Errc::out_of_range);
    EXPECT_EQ(width->get(), 40);
}

TEST_F(Nodes, IntegerIncWithoutMinUsesTheFullRange)
{
    ASSERT_TRUE(parse(R"(<Integer Name="Even"><Value>0</Value><Inc>2</Inc></Integer>)"));
    auto even = map->get<Integer>("Even");
    EXPECT_EQ(even->min(), std::numeric_limits<int64_t>::min());
    EXPECT_TRUE(even->set(4));
    EXPECT_EQ(error_of(even->set(3)), Errc::out_of_range);
}

TEST_F(Nodes, IntegerTakesItsBoundsFromItsValueNode)
{
    ASSERT_TRUE(parse(R"(<Integer Name="Width"><pValue>WidthReg</pValue></Integer>
             <IntReg Name="WidthReg"><Address>0</Address><Length>1</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>
             <Integer Name="Height"><pValue>WidthReg</pValue><pMin>Low</pMin><pMax>High</pMax></Integer>
             <Integer Name="Low"><Value>10</Value></Integer>
             <Integer Name="High"><Value>20</Value></Integer>)"));
    auto width = map->get<Integer>("Width");
    EXPECT_EQ(width->min(), 0);
    EXPECT_EQ(width->max(), 255);
    EXPECT_TRUE(width->set(200));
    EXPECT_EQ(port.memory[0], 200);

    auto height = map->get<Integer>("Height");
    EXPECT_EQ(height->min(), 10);
    EXPECT_EQ(height->max(), 20);
    EXPECT_EQ(error_of(height->set(200)), Errc::out_of_range);
}

TEST_F(Nodes, IntegerReadsAFloatOrAnEnumeration)
{
    ASSERT_TRUE(parse(R"(<Integer Name="Truncated"><pValue>Ratio</pValue></Integer>
             <Float Name="Ratio"><Value>2.75</Value></Float>
             <Integer Name="Raw"><pValue>Mode</pValue></Integer>
             <Enumeration Name="Mode"><EnumEntry Name="Fast"><Value>7</Value></EnumEntry><Value>7</Value></Enumeration>)"));
    EXPECT_EQ(map->get<Integer>("Truncated")->get(), 2);
    EXPECT_EQ(map->get<Integer>("Raw")->get(), 7);
}

TEST_F(Nodes, FloatChecksMinAndMaxButLeavesIncToTheDevice)
{
    ASSERT_TRUE(parse(R"(<Float Name="Exposure"><Value>100.5</Value><Min>10</Min><Max>1000</Max><Inc>0.5</Inc></Float>
             <Float Name="Gain"><Value>1.0</Value><pInc>GainStep</pInc></Float>
             <Float Name="GainStep"><Value>0.1</Value></Float>
             <Float Name="Gamma"><Value>1.0</Value></Float>)"));
    auto exposure = map->get<Float>("Exposure");
    EXPECT_EQ(exposure->inc(), 0.5);
    EXPECT_EQ(map->get<Float>("Gain")->inc(), 0.1);
    EXPECT_EQ(map->get<Float>("Gamma")->inc(), std::nullopt);
    EXPECT_EQ(exposure->get(), 100.5);
    EXPECT_TRUE(exposure->set(250.25));
    EXPECT_EQ(exposure->get(), 250.25);
    EXPECT_EQ(error_of(exposure->set(5.0)), Errc::out_of_range);
    EXPECT_EQ(error_of(exposure->set(1000.5)), Errc::out_of_range);
}

TEST_F(Nodes, FloatReadsAnInteger)
{
    ASSERT_TRUE(parse(R"(<Float Name="Exposure"><pValue>ExposureRaw</pValue></Float>
             <Integer Name="ExposureRaw"><Value>12</Value><Min>1</Min><Max>50</Max></Integer>)"));
    auto exposure = map->get<Float>("Exposure");
    EXPECT_EQ(exposure->get(), 12.0);
    EXPECT_EQ(exposure->min(), 1.0);
    EXPECT_EQ(exposure->max(), 50.0);
    EXPECT_EQ(exposure->inc(), 1.0);
    EXPECT_TRUE(exposure->set(30.9));
    EXPECT_EQ(map->get<Integer>("ExposureRaw")->get(), 30);
}

TEST_F(Nodes, BooleanMapsOnAndOffValues)
{
    ASSERT_TRUE(parse(R"(<Boolean Name="Reverse"><pValue>ReverseReg</pValue><OnValue>3</OnValue><OffValue>1</OffValue></Boolean>
             <IntReg Name="ReverseReg"><Address>0</Address><Length>1</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>
             <Boolean Name="Constant"><Value>true</Value></Boolean>)"));
    auto reverse = map->get<Boolean>("Reverse");
    EXPECT_TRUE(reverse->set(true));
    EXPECT_EQ(port.memory[0], 3);
    EXPECT_EQ(reverse->get(), true);
    EXPECT_TRUE(reverse->set(false));
    EXPECT_EQ(port.memory[0], 1);
    EXPECT_EQ(reverse->get(), false);
    port.store(0, {2});
    EXPECT_EQ(error_of(reverse->get()), Errc::invalid_value);

    EXPECT_EQ(map->get<Boolean>("Constant")->get(), true);
}

TEST_F(Nodes, CommandWritesItsValueAndPollsForCompletion)
{
    ASSERT_TRUE(parse(R"(<Command Name="Start"><pValue>StartReg</pValue><CommandValue>1</CommandValue></Command>
             <IntReg Name="StartReg"><Address>0</Address><Length>4</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>
             <Command Name="Reset"><pValue>ResetReg</pValue><CommandValue>0xA5</CommandValue></Command>
             <IntReg Name="ResetReg"><Address>4</Address><Length>1</Length><AccessMode>WO</AccessMode><pPort>Device</pPort></IntReg>)"));
    auto start = map->get<Command>("Start");
    EXPECT_TRUE(start->execute());
    EXPECT_EQ(port.memory[0], 1);
    EXPECT_EQ(start->is_done(), false);
    port.store(0, {0});
    EXPECT_EQ(start->is_done(), true);

    auto reset = map->get<Command>("Reset");
    EXPECT_TRUE(reset->execute());
    EXPECT_EQ(port.memory[4], 0xA5);
    EXPECT_EQ(reset->is_done(), true);
}

TEST_F(Nodes, EnumerationSetsAndGetsEntriesByName)
{
    ASSERT_TRUE(parse(R"(<Enumeration Name="ExposureAuto">
                 <EnumEntry Name="Off"><Value>0</Value></EnumEntry>
                 <EnumEntry Name="Once"><pIsAvailable>OnceAvailable</pIsAvailable><Value>1</Value></EnumEntry>
                 <EnumEntry Name="Continuous"><Value>2</Value></EnumEntry>
                 <pValue>ExposureAutoReg</pValue>
             </Enumeration>
             <IntReg Name="ExposureAutoReg"><Address>0</Address><Length>4</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>
             <Integer Name="OnceAvailable"><Value>0</Value></Integer>)"));
    auto exposure_auto = map->get<Enumeration>("ExposureAuto");
    ASSERT_EQ(exposure_auto->entries().size(), 3u);
    EXPECT_EQ(exposure_auto->entries()[2]->name(), "Continuous");
    EXPECT_EQ(exposure_auto->entries()[2]->value(), 2);

    EXPECT_EQ(exposure_auto->get(), "Off");
    EXPECT_TRUE(exposure_auto->set("Continuous"));
    EXPECT_EQ(port.memory[0], 2);
    EXPECT_EQ(exposure_auto->get(), "Continuous");
    EXPECT_EQ(exposure_auto->get_int(), 2);

    EXPECT_EQ(error_of(exposure_auto->set("Once")), Errc::not_available);
    EXPECT_EQ(error_of(exposure_auto->set("Sometimes")), Errc::invalid_value);
    EXPECT_EQ(error_of(exposure_auto->set_int(5)), Errc::invalid_value);
    port.store(0, {9});
    EXPECT_EQ(error_of(exposure_auto->get()), Errc::invalid_value);
}

TEST_F(Nodes, StringIsAConstantOrAnotherString)
{
    ASSERT_TRUE(parse(R"(<String Name="Vendor"><Value>KickCam</Value></String>
             <String Name="Model"><pValue>ModelReg</pValue></String>
             <StringReg Name="ModelReg"><Address>0</Address><Length>8</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></StringReg>)"));
    EXPECT_EQ(map->get<String>("Vendor")->get(), "KickCam");
    EXPECT_TRUE(map->get<String>("Model")->set("KC-1"));
    EXPECT_EQ(map->get<String>("ModelReg")->get(), "KC-1");
}

TEST_F(Nodes, CategoryListsItsFeatures)
{
    ASSERT_TRUE(parse(R"(<Category Name="Root"><pFeature>ImageFormat</pFeature><pFeature>NotInThisXml</pFeature></Category>
             <Category Name="ImageFormat"><pFeature>Width</pFeature></Category>
             <Integer Name="Width"><Value>640</Value></Integer>)"));
    auto root = map->get<Category>("Root");
    ASSERT_EQ(root->features().size(), 1u);
    EXPECT_EQ(root->features()[0]->name(), "ImageFormat");
    EXPECT_EQ(root->access(), Access::RO);
}

TEST_F(Nodes, IndexSelectsTheValue)
{
    ASSERT_TRUE(parse(R"(<Integer Name="Gain">
                             <pIndex>Selector</pIndex>
                             <ValueIndexed Index="0">10</ValueIndexed>
                             <pValueIndexed Index="1">GainBlue</pValueIndexed>
                             <ValueDefault>99</ValueDefault>
                         </Integer>
                         <Float Name="Ratio">
                             <pIndex>Selector</pIndex>
                             <ValueIndexed Index="0">1.5</ValueIndexed>
                             <ValueDefault>0.5</ValueDefault>
                         </Float>
                         <Integer Name="Selector"><Value>0</Value></Integer>
                         <Integer Name="GainBlue"><Value>20</Value><Min>5</Min><Max>50</Max></Integer>)"));
    auto gain = map->get<Integer>("Gain");
    auto selector = map->get<Integer>("Selector");
    EXPECT_EQ(gain->get(), 10);
    EXPECT_EQ(map->get<Float>("Ratio")->get(), 1.5);

    ASSERT_TRUE(selector->set(1));
    EXPECT_EQ(gain->get(), 20);
    EXPECT_EQ(gain->min(), 5);
    EXPECT_EQ(gain->max(), 50);
    EXPECT_TRUE(gain->set(30));
    EXPECT_EQ(map->get<Integer>("GainBlue")->get(), 30);

    ASSERT_TRUE(selector->set(7));
    EXPECT_EQ(gain->get(), 99);
    EXPECT_EQ(map->get<Float>("Ratio")->get(), 0.5);
}

TEST_F(Nodes, IntegerWritesItsCopies)
{
    ASSERT_TRUE(parse(R"(<Integer Name="Width"><pValue>WidthReg</pValue><pValueCopy>WidthShadow</pValueCopy></Integer>
                         <IntReg Name="WidthReg"><Address>0</Address><Length>2</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>
                         <IntReg Name="WidthShadow"><Address>2</Address><Length>2</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>)"));
    EXPECT_TRUE(map->get<Integer>("Width")->set(640));
    EXPECT_EQ(port.load(0, 4), (std::vector<uint8_t>{0x80, 0x02, 0x80, 0x02}));
}
