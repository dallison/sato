// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#include "absl/strings/str_format.h"
#include "sato/testdata/TestMessage.pb.h"
#include "sato/testdata/TestMessage.sato.h"
#include <gtest/gtest.h>
#include <sstream>
#include "toolbelt/hexdump.h"

TEST(SatoBasicTest, Basic) {
  foo::bar::TestMessage msg;
  msg.set_x(1234);
  msg.set_s("hello world");
  msg.add_vi32(1);
  msg.add_vi32(2);
  msg.add_vi32(3);

  msg.add_vstr("one");
  msg.add_vstr("two");
  msg.add_vstr("three");

  auto m = msg.mutable_m();
  m->set_str("Inner message");
  m->set_f(1234567890);

  auto vm = msg.mutable_vm();
  auto inner = vm->Add();
  inner->set_str("Inner1");
  inner->set_f(999);

  inner = vm->Add();
  inner->set_str("Inner2");
  inner->set_f(888);

  std::string serialized;
  msg.SerializeToString(&serialized);
  ASSERT_EQ(1234, msg.x());
  ASSERT_EQ(0, msg.y());
  ASSERT_EQ("hello world", msg.s());
  std::cerr << "serialized: " << serialized;
  toolbelt::Hexdump(serialized.data(), serialized.size());

  foo::bar::sato::TestMessage t;
  sato::ProtoBuffer buffer(serialized);
  sato::ROSBuffer ros_buffer;
  absl::Status status = t.ProtoToROS(buffer, ros_buffer);
  std::cerr << "status: " << status;
  ASSERT_TRUE(status.ok());

  toolbelt::Hexdump(ros_buffer.data(), ros_buffer.size());
}
