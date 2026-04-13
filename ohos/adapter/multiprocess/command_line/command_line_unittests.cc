// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "ohos/adapter/multiprocess/command_line/command_line.h"

namespace ohos::adapter::multiprocess {
class CommandLineTest : public ::testing::Test {
 protected:
  void TearDown() override {
    CommandLine::ResetForCurrentProcess();
  }
};

TEST(CommandLineTest, CurrentProcessTest) {
  const std::vector<std::string> args = {
      "program",
      "--foo=",
      "-bAr",
      "-spaetzel=pierogi",
      "-baz",
      "flim",
      "--other-switches=--dog=canine --cat=feline",
      "-spaetzle=Crepe",
      "-=loosevalue",
      "-",
      "FLAN",
      "a",
      "--input-translation=45--output-rotation",
      "--",
      "\"in the time of submarines...\"",
      "unquoted arg-with-space"};
  CommandLine::InitForCurrentProcess(args);

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  EXPECT_NE(nullptr, command_line);
  EXPECT_FALSE(command_line->HasSwitch("cruller"));
  EXPECT_FALSE(command_line->HasSwitch("flim"));
  EXPECT_FALSE(command_line->HasSwitch("program"));
  EXPECT_FALSE(command_line->HasSwitch("dog"));
  EXPECT_FALSE(command_line->HasSwitch("cat"));
  EXPECT_FALSE(command_line->HasSwitch("output-rotation"));
  EXPECT_FALSE(command_line->HasSwitch("--"));

  EXPECT_TRUE(command_line->HasSwitch("foo"));
  EXPECT_FALSE(command_line->HasSwitch("bar"));
  EXPECT_TRUE(command_line->HasSwitch("baz"));
  EXPECT_TRUE(command_line->HasSwitch("spaetzle"));
  EXPECT_TRUE(command_line->HasSwitch("other-switches"));
  EXPECT_TRUE(command_line->HasSwitch("input-translation"));
}

TEST(CommandLineTest, CurrentProcessResetTest) {
  CommandLine::InitForCurrentProcess(std::vector<std::string>());
  EXPECT_NE(nullptr, CommandLine::ForCurrentProcess());
  CommandLine::ResetForCurrentProcess();
  EXPECT_EQ(nullptr, CommandLine::ForCurrentProcess());
}
} // namespace ohos::adapter::multiprocess