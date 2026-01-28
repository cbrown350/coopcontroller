#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Main function for test executable
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
