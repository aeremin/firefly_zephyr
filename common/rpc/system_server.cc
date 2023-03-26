#include "system_server.h"

#include <cstddef>

#include "pw_hdlc/encoded_size.h"
#include "pw_hdlc/rpc_channel.h"
#include "pw_hdlc/rpc_packets.h"
#include "pw_log/log.h"
#include "pw_rpc/server.h"
#include "pw_stream/sys_io_stream.h"


namespace common::rpc::system_server {
namespace {

// Hard-coded to 1055 bytes, which is enough to fit 512-byte payloads when using
// HDLC framing.
constexpr size_t kMaxTransmissionUnit = 1055;

static_assert(kMaxTransmissionUnit ==
              pw::hdlc::MaxEncodedFrameSize(pw::rpc::cfg::kEncodingBufferSizeBytes));

// Used to write HDLC data to pw::sys_io.
pw::stream::SysIoWriter writer;

// Set up the output channel for the pw_rpc server to use.
pw::hdlc::FixedMtuChannelOutput<kMaxTransmissionUnit> hdlc_channel_output(
    writer, pw::hdlc::kDefaultRpcAddress, "HDLC channel");
pw::rpc::Channel channels[] = {pw::rpc::Channel::Create<1>(&hdlc_channel_output)};
pw::rpc::Server server(channels);

}  // namespace

pw::rpc::Server& Server() { return server; }

pw::Status Start() {
  constexpr size_t kDecoderBufferSize =
      pw::hdlc::Decoder::RequiredBufferSizeForFrameSize(kMaxTransmissionUnit);
  // Declare a buffer for decoding incoming HDLC frames.
  std::array<std::byte, kDecoderBufferSize> input_buffer;
  pw::hdlc::Decoder decoder(input_buffer);

  while (true) {
    std::byte byte;
    pw::Status ret_val = pw::sys_io::ReadByte(&byte);
    if (!ret_val.ok()) {
      return ret_val;
    }
    if (auto result = decoder.Process(byte); result.ok()) {
      pw::hdlc::Frame& frame = result.value();
      if (frame.address() == pw::hdlc::kDefaultRpcAddress) {
        server.ProcessPacket(frame.data());
      }
    }
  }
}

}  // namespace common::rpc::system_server
