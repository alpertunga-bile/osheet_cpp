#include "ffmpeg_utils.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace osheet {

auto
close_avformat_context(AVFormatContext* fmt) -> void
{
  if (nullptr != fmt) {
    avformat_close_input(&fmt);
  }
}

auto
close_avcodec_context(AVCodecContext* ctx) -> void
{
  if (nullptr != ctx) {
    avcodec_free_context(&ctx);
  }
}

auto
close_avframe(AVFrame* frame) -> void
{
  if (nullptr != frame) {
    av_frame_free(&frame);
  }
}

auto
close_avpacket(AVPacket* pckt) -> void
{
  if (nullptr != pckt) {
    av_packet_free(&pckt);
  }
}

auto
close_sws_context(SwsContext* ctx) -> void
{
  if (nullptr != ctx) {
    sws_freeContext(ctx);
  }
}

}
