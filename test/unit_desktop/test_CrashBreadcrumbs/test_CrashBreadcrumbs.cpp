#include <gtest/gtest.h>
#include "CrashBreadcrumbs.h"

// Desktop tests verify the no-op stub API when COOP_CRASH_DIAGNOSTICS is not
// defined (the default for unit-test-desktop and release firmware builds).
// The active ESP32 instrumentation is covered by on-board testing.

// Non-capturing counter for the report test (function pointers can't capture).
static int s_reportLineCount = 0;
static void testReportCallback(const char *) { s_reportLineCount++; }

TEST(CrashBreadcrumbsTest, DisabledBuild_RecordIsNoOp) {
    // Should not crash, should not allocate, should not log.
    crashBreadcrumbRecord(CRUMB_UPDATE_REQUESTED, 1, 2);
    SUCCEED();
}

TEST(CrashBreadcrumbsTest, DisabledBuild_RequestDeltaIsNoOp) {
    crashBreadcrumbRequestDelta(1, 0, 0);
    crashBreadcrumbRequestDelta(-1, 0, 0);
    SUCCEED();
}

TEST(CrashBreadcrumbsTest, DisabledBuild_ReportProducesNoOutput) {
    s_reportLineCount = 0;
    crashBreadcrumbReport(testReportCallback);
    EXPECT_EQ(s_reportLineCount, 0);
}

TEST(CrashBreadcrumbsTest, DisabledBuild_PressureRequestIsNoOp) {
    crashBreadcrumbPressureRequest(8192, 5000);
    auto status = crashBreadcrumbPressureStatus();
    EXPECT_FALSE(status.active);
}

TEST(CrashBreadcrumbsTest, DisabledBuild_PressureUpdateIsNoOp) {
    crashBreadcrumbPressureUpdate();
    SUCCEED();
}
