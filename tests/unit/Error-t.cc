#include <gtest/gtest.h>

#include "kickcam/Error.h"

using namespace kickcam;

TEST(Error, ConvertsToErrorCode)
{
    std::error_code const code = Errc::locked;
    EXPECT_EQ(code.category().name(), std::string{"kickcam"});
    EXPECT_EQ(code.message(), "feature locked");
    EXPECT_EQ(code, Errc::locked);
    EXPECT_NE(code, Errc::not_writable);
}
