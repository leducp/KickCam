#include <gtest/gtest.h>

#include <limits>

#include "helpers.h"

using namespace kickcam;
using namespace kickcam::genapi;

class SwissKnife : public NodeMapTest
{
};

TEST_F(SwissKnife, ComputesFromVariablesConstantsAndExpressions)
{
    ASSERT_TRUE(parse(R"(<SwissKnife Name="Gain">
                             <pVariable Name="RAW">GainRaw</pVariable>
                             <Constant Name="SCALE">0.5</Constant>
                             <Expression Name="HALF">RAW * SCALE</Expression>
                             <Formula>HALF + 1</Formula>
                         </SwissKnife>
                         <IntReg Name="GainRaw"><Address>0</Address><Length>1</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>)"));
    port.store(0, {7});
    auto gain = map->get<Float>("Gain");
    EXPECT_EQ(gain->get(), 4.5);
    EXPECT_EQ(gain->access(), Access::RO);
    EXPECT_EQ(error_of(gain->set(1.0)), Errc::not_writable);
}

TEST_F(SwissKnife, IntSwissKnifeComputesInIntegers)
{
    ASSERT_TRUE(parse(R"(<IntSwissKnife Name="Payload">
                             <pVariable Name="W">Width</pVariable>
                             <pVariable Name="H">Height</pVariable>
                             <Constant Name="BYTES">0x2</Constant>
                             <Formula>W * H * BYTES / 3</Formula>
                         </IntSwissKnife>
                         <Integer Name="Width"><Value>10</Value></Integer>
                         <Integer Name="Height"><Value>5</Value></Integer>)"));
    EXPECT_EQ(map->get<Integer>("Payload")->get(), 33);
}

TEST_F(SwissKnife, ReadsBoundsAndEntriesThroughVariableNames)
{
    ASSERT_TRUE(parse(R"(<IntSwissKnife Name="Steps">
                             <pVariable Name="W.Max">Width</pVariable>
                             <pVariable Name="W.Min">Width</pVariable>
                             <pVariable Name="W.Inc">Width</pVariable>
                             <Formula>(W.Max - W.Min) / W.Inc</Formula>
                         </IntSwissKnife>
                         <Integer Name="Width"><Value>64</Value><Min>16</Min><Max>1024</Max><Inc>8</Inc></Integer>
                         <IntSwissKnife Name="IsMono">
                             <pVariable Name="FORMAT">PixelFormat</pVariable>
                             <pVariable Name="FORMAT.Entry.Mono8">PixelFormat</pVariable>
                             <Formula>FORMAT = FORMAT.Entry.Mono8</Formula>
                         </IntSwissKnife>
                         <Enumeration Name="PixelFormat">
                             <EnumEntry Name="Mono8"><Value>0x01080001</Value></EnumEntry>
                             <EnumEntry Name="Mono12"><Value>0x01100005</Value></EnumEntry>
                             <Value>0x01080001</Value>
                         </Enumeration>)"));
    EXPECT_EQ(map->get<Integer>("Steps")->get(), 126);
    EXPECT_EQ(map->get<Integer>("IsMono")->get(), 1);
    ASSERT_TRUE(map->get<Enumeration>("PixelFormat")->set("Mono12"));
    EXPECT_EQ(map->get<Integer>("IsMono")->get(), 0);
}

TEST_F(SwissKnife, AddsToARegisterAddress)
{
    ASSERT_TRUE(parse(R"(<IntReg Name="Value">
                             <Address>0x10</Address>
                             <IntSwissKnife Name="Offset"><pVariable Name="I">Index</pVariable><Formula>I * 4</Formula></IntSwissKnife>
                             <Length>1</Length>
                             <pPort>Device</pPort>
                         </IntReg>
                         <Integer Name="Index"><Value>2</Value></Integer>)"));
    port.store(0x18, {9});
    EXPECT_EQ(map->get<Integer>("Value")->get(), 9);
    EXPECT_EQ(map->get<Integer>("Offset")->get(), 8);
}

TEST_F(SwissKnife, ConverterMapsBothWays)
{
    ASSERT_TRUE(parse(R"(<Converter Name="ExposureTime">
                             <pVariable Name="LINE">LineTime</pVariable>
                             <FormulaTo>FROM / LINE</FormulaTo>
                             <FormulaFrom>TO * LINE</FormulaFrom>
                             <pValue>ExposureLines</pValue>
                         </Converter>
                         <Float Name="LineTime"><Value>12.5</Value></Float>
                         <IntReg Name="ExposureLines"><Address>0</Address><Length>2</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>)"));
    auto exposure = map->get<Float>("ExposureTime");
    EXPECT_TRUE(exposure->set(250.0));
    EXPECT_EQ(port.load(0, 2), (std::vector<uint8_t>{20, 0}));
    EXPECT_EQ(exposure->get(), 250.0);
    EXPECT_EQ(exposure->min(), 0.0);
    EXPECT_EQ(exposure->max(), 65535 * 12.5);
    EXPECT_EQ(exposure->access(), Access::RW);
}

TEST_F(SwissKnife, ConverterBoundsFollowItsSlope)
{
    ASSERT_TRUE(parse(R"(<IntConverter Name="Level">
                             <FormulaTo>100 - FROM</FormulaTo>
                             <FormulaFrom>100 - TO</FormulaFrom>
                             <pValue>LevelReg</pValue>
                         </IntConverter>
                         <IntConverter Name="Wavy">
                             <FormulaTo>FROM</FormulaTo>
                             <FormulaFrom>TO</FormulaFrom>
                             <pValue>LevelReg</pValue>
                             <Slope>Varying</Slope>
                         </IntConverter>
                         <IntReg Name="LevelReg"><Address>0</Address><Length>1</Length><AccessMode>RW</AccessMode><pPort>Device</pPort></IntReg>)"));
    auto level = map->get<Integer>("Level");
    EXPECT_EQ(level->min(), -155);
    EXPECT_EQ(level->max(), 100);
    EXPECT_TRUE(level->set(30));
    EXPECT_EQ(port.memory[0], 70);
    EXPECT_EQ(level->get(), 30);

    auto wavy = map->get<Integer>("Wavy");
    EXPECT_EQ(wavy->min(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(wavy->max(), std::numeric_limits<int64_t>::max());
}
