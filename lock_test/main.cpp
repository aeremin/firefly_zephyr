#include <zephyr/kernel.h>
#include <zephyr/random/rand32.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>

#include "gtest/gtest.h"
#include "printk_event_handler.h"
#include "rpc/system_server.h"
#include "test.pwpb.h"
#include "test.rpc.pwpb.h"
#include "pw_log/log.h"
#include "pw_assert/check.h"
#include "pw_log/proto/log.raw_rpc.pb.h"

using namespace common::rpc;


TEST(BasicTest, Sum) {
  ASSERT_EQ(2 + 2, 4);
}

TEST(ProtoTest, Equality) {
  static_assert(pw::protobuf::IsTriviallyComparable<pwpb::Customer::Message>());
  pwpb::Customer::Message a, b;
  a.age = 5;
  a.status = pwpb::Customer::Status::ACTIVE;

  b.age = 5;
  b.status = pwpb::Customer::Status::ACTIVE;

  ASSERT_EQ(a, b);
}

class EchoService final
    : public common::rpc::pw_rpc::pwpb::EchoService::Service<EchoService> {
 public:
  pw::Status Echo(const pwpb::Customer::Message& request, pwpb::Customer::Message& response) {
    response = request;
    response.age += 3;
    return pw::OkStatus();
  }
};

class LogService final : public pw::log::pw_rpc::raw::Logs::Service<LogService> {
 public:
  void Listen(pw::ConstByteSpan, pw::rpc::RawServerWriter&) {}
};

static EchoService echo_service;
static LogService log_service;

int main() {
  PrintkEventHandler handler;
  pw::unit_test::RegisterEventHandler(&handler);

  int num_failures = RUN_ALL_TESTS();
  if (!num_failures) {
    printk("All tests passed!\n");
  }

  system_server::Server().RegisterService(echo_service);
  system_server::Server().RegisterService(log_service);

  PW_LOG_INFO("Starting pw_rpc server");
  PW_CHECK_OK(system_server::Start());

  while (true) k_sleep(K_MSEC(1000));
}
