#include <gtest/gtest.h>

#include <string>

#include "kickcam/version.h"

TEST(Version, StringMatchesNumbers)
{
    std::string const expected = std::to_string(KICKCAM_VERSION_MAJOR) + "."
                               + std::to_string(KICKCAM_VERSION_MINOR) + "."
                               + std::to_string(KICKCAM_VERSION_PATCH);
    EXPECT_EQ(KICKCAM_VERSION_STRING, expected);
}
