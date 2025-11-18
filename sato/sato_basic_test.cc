// Copyright 2024 David Allison
// All Rights Reserved
// See LICENSE file for licensing information.

#include "absl/strings/str_format.h"
#include "sato/testdata/TestMessage.pb.h"
#include "sato/testdata/TestMessage.sato.h"
#include "toolbelt/hexdump.h"
#include <gtest/gtest.h>
#include <sstream>

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

  msg.set_e(foo::bar::FOO);

  msg.set_u1a(0x01020304);

  std::string serialized;
  msg.SerializeToString(&serialized);
  ASSERT_EQ(1234, msg.x());
  ASSERT_EQ(0, msg.y());
  ASSERT_EQ("hello world", msg.s());
  toolbelt::Hexdump(serialized.data(), serialized.size(), stderr);

  foo::bar::sato::TestMessage t;
  sato::ProtoBuffer buffer(serialized);
  sato::ROSBuffer ros_buffer;
  absl::Status status = t.ProtoToROS(buffer, ros_buffer);
  std::cerr << "status: " << status << std::endl;
  ASSERT_TRUE(status.ok());

  toolbelt::Hexdump(ros_buffer.data(), ros_buffer.size(), stderr);

  sato::ProtoBuffer buffer2;
  sato::ROSBuffer ros_buffer2(ros_buffer.data(), ros_buffer.size());
  std::cerr << "ros_buffer2: " << ros_buffer2.AsString() << std::endl;
  toolbelt::Hexdump(ros_buffer2.data(), ros_buffer.size(), stderr);

  foo::bar::sato::TestMessage t2;
  status = t2.ROSToProto(ros_buffer2, buffer2);
  std::cerr << "status: " << status << std::endl;
  ASSERT_TRUE(status.ok());

  toolbelt::Hexdump(buffer2.data(), buffer2.size(), stderr);
  ASSERT_EQ(buffer.size(), buffer2.size());
  ASSERT_EQ(0, memcmp(buffer.data(), buffer2.data(), buffer2.size()));
}

TEST(SatoBasicTest, Multiplexer) {
  foo::bar::TestMessage msg;
  msg.set_x(1234);
  msg.set_y(5678);
  msg.set_s("hello world");

  std::string serialized;
  msg.SerializeToString(&serialized);

  toolbelt::Hexdump(serialized.data(), serialized.size(), stderr);
  // Parse from protobuf using the multiplexer
  sato::ProtoBuffer buffer(serialized);
  foo::bar::sato::TestMessage t;

  absl::Status status = sato::MultiplexerParseProto("foo.bar.TestMessage", t, buffer);
  sato::ROSBuffer ros_buffer;

  status = sato::MultiplexerWriteROS("foo.bar.TestMessage", t, ros_buffer);
  std::cerr << "status: " << status << std::endl;
  ASSERT_TRUE(status.ok());

  // Convert back to protobuf using the multiplexer
  sato::ProtoBuffer buffer2;
  status = sato::MultiplexerWriteProto("foo.bar.TestMessage", t, buffer2);
  std::cerr << "status: " << status << std::endl;
  ASSERT_TRUE(status.ok());

  toolbelt::Hexdump(buffer2.data(), buffer2.size(), stderr);
  ASSERT_EQ(buffer.size(), buffer2.size());
  ASSERT_EQ(0, memcmp(buffer.data(), buffer2.data(), buffer2.size()));
}