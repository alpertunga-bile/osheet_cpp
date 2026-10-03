#ifndef OSHEET_CPP_FFMPEG_UTILS_HPP
#define OSHEET_CPP_FFMPEG_UTILS_HPP

#include <memory>

typedef struct AVFormatContext AVFormatContext;
typedef struct AVCodecContext  AVCodecContext;
typedef struct AVFrame         AVFrame;
typedef struct AVPacket        AVPacket;
typedef struct SwsContext      SwsContext;

namespace osheet {

auto
close_avformat_context(AVFormatContext* fmt) -> void;

auto
close_avcodec_context(AVCodecContext* ctx) -> void;

auto
close_avframe(AVFrame* frame) -> void;

auto
close_avpacket(AVPacket* pckt) -> void;

auto
close_sws_context(SwsContext* ctx) -> void;

using FmtUniqType =
  std::unique_ptr<AVFormatContext, decltype(&close_avformat_context)>;
using CodecCtxUniqType =
  std::unique_ptr<AVCodecContext, decltype(&close_avcodec_context)>;
using FrameUniqType  = std::unique_ptr<AVFrame, decltype(&close_avframe)>;
using PacketUniqType = std::unique_ptr<AVPacket, decltype(&close_avpacket)>;
using SwsCtxUniqType =
  std::unique_ptr<SwsContext, decltype(&close_sws_context)>;

#define CREATE_UNIQUE_BUILDER(__valname__, __value__, __type__, __func__)      \
  std::unique_ptr<__type__, decltype(&__func__)> __valname__(__value__,        \
                                                             &__func__);

#define CREATE_FMT_UNIQUE(__valname__, __value__)                              \
  CREATE_UNIQUE_BUILDER(                                                       \
    __valname__, __value__, AVFormatContext, close_avformat_context)

#define CREATE_CODECCTX_UNIQUE(__valname__, __value__)                         \
  CREATE_UNIQUE_BUILDER(                                                       \
    __valname__, __value__, AVCodecContext, close_avcodec_context)

#define CREATE_FRAME_UNIQUE(__valname__, __value__)                            \
  CREATE_UNIQUE_BUILDER(__valname__, __value__, AVFrame, close_avframe)

#define CREATE_PACKET_UNIQUE(__valname__, __value__)                           \
  CREATE_UNIQUE_BUILDER(__valname__, __value__, AVPacket, close_avpacket)

#define CREATE_SWSCTX_UNIQUE(__valname__, __value__)                           \
  CREATE_UNIQUE_BUILDER(__valname__, __value__, SwsContext, close_sws_context)

}

#endif