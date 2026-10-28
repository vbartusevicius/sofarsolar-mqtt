#include <gtest/gtest.h>
#include <string>
#include "util/AppLog.h"

// AppLog.cpp itself is compiled once, via test_integration.cpp.

static std::string dump(const AppLog& log) {
    std::string s;
    for (unsigned int i = 0; i < log.end(); i++) s += log.at(i);
    return s;
}

static int count(const std::string& s, const std::string& needle) {
    int n = 0;
    for (size_t p = s.find(needle); p != std::string::npos; p = s.find(needle, p + 1)) n++;
    return n;
}

TEST(AppLog, DistinctEntriesAreAllKept) {
    AppLog lg;
    lg.add("A", "one");
    lg.add("A", "two");
    lg.add("B", "one");
    std::string s = dump(lg);
    EXPECT_EQ(count(s, "[A] one"), 1);
    EXPECT_EQ(count(s, "[A] two"), 1);
    EXPECT_EQ(count(s, "[B] one"), 1);
    EXPECT_EQ(count(s, "repeated"), 0);
}

TEST(AppLog, IdenticalRunIsCollapsedIntoOneCount) {
    AppLog lg;
    lg.add("SYS", "boot");
    for (int i = 0; i < 10; i++) lg.add("MB", "RD 0x1110 x1 FAIL timeout got 0B");
    lg.add("MB", "link back");
    std::string s = dump(lg);
    EXPECT_EQ(count(s, "RD 0x1110"), 1);
    EXPECT_EQ(count(s, "last message repeated 9x"), 1);
    EXPECT_EQ(count(s, "link back"), 1);
    EXPECT_EQ(count(s, "[SYS] boot"), 1);   // history before the run survives
}

TEST(AppLog, LongRunStillReportsPeriodically) {
    AppLog lg;
    for (int i = 0; i < 1 + 250; i++) lg.add("MB", "same");
    std::string s = dump(lg);
    EXPECT_EQ(count(s, "[MB] same"), 1);
    EXPECT_EQ(count(s, "repeated 100x"), 2);
}

TEST(AppLog, SameTextUnderDifferentTagIsNotARepeat) {
    AppLog lg;
    lg.add("A", "x");
    lg.add("B", "x");
    EXPECT_EQ(count(dump(lg), "repeated"), 0);
}

TEST(AppLog, SerialOnlyAdvancesWhenSomethingIsWritten) {
    AppLog lg;
    lg.add("A", "x");
    unsigned int s0 = lg.serial();
    lg.add("A", "x");
    lg.add("A", "x");
    EXPECT_EQ(lg.serial(), s0);   // the LCD log view does not repaint for nothing
}
