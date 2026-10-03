#include "tiles.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include "ffmpeg_utils.hpp"

#include <filesystem>
#include <format>
#include <memory>

namespace osheet {

[[nodiscard]] auto
save_tile_png(const char* path, AVFrame* src) -> bool
{
  const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_PNG);

  if (nullptr == encoder) {
    return false;
  }

  CREATE_CODECCTX_UNIQUE(enc, avcodec_alloc_context3(encoder));

  if (enc == nullptr)
    return false;

  enc->width     = src->width;
  enc->height    = src->height;
  enc->pix_fmt   = AV_PIX_FMT_RGBA;
  enc->time_base = AVRational{ 1, 1 };

  if (avcodec_open2(enc.get(), encoder, nullptr) < 0) {
    return false;
  }

  if (avcodec_send_frame(enc.get(), src) < 0) {
    return false;
  }

  if (avcodec_send_frame(enc.get(), nullptr) < 0) {
    return false;
  }

  CREATE_PACKET_UNIQUE(pkt, av_packet_alloc());

  FILE* out = std::fopen(path, "wb"); // "-y" → overwrite
  if (out == nullptr)
    return false;

  while (avcodec_receive_packet(enc.get(), pkt.get()) >= 0) {
    std::fwrite(pkt->data, 1, static_cast<std::size_t>(pkt->size), out);
    av_packet_unref(pkt.get());
  }

  return std::fclose(out) == 0;
}

[[nodiscard]] auto
scale_tile(AVFrame* tile, AVFrame* rgb, const int wanted_width) -> bool
{
  const int outW = wanted_width;
  const int outH = std::max(2, (tile->height * outW / tile->width) & ~1);

  CREATE_SWSCTX_UNIQUE(
    sws,
    sws_getContext(tile->width,
                   tile->height,
                   static_cast<AVPixelFormat>(tile->format),
                   outW,
                   outH,
                   static_cast<AVPixelFormat>(AV_PIX_FMT_RGBA),
                   SWS_LANCZOS,
                   nullptr,
                   nullptr,
                   nullptr));

  if (nullptr == sws) {
    return false;
  }

  rgb->width  = outW;
  rgb->height = outH;
  rgb->format = AV_PIX_FMT_RGBA;

  if (av_frame_get_buffer(rgb, 0) < 0) {
    return false;
  }

  sws_scale(sws.get(),
            tile->data,
            tile->linesize,
            0,
            tile->height,
            rgb->data,
            rgb->linesize);

  return true;
}

[[nodiscard]] auto
get_wanted_frame(FrameUniqType&    tile,
                 FmtUniqType&      fmt,
                 CodecCtxUniqType& dec,
                 const int         vidx,
                 const int64_t     target) -> bool
{
  CREATE_PACKET_UNIQUE(pkt, av_packet_alloc());

  while (nullptr == tile && av_read_frame(fmt.get(), pkt.get()) >= 0) {
    if (pkt->stream_index != vidx) {
      av_packet_unref(pkt.get());
      continue;
    }

    const int sr = avcodec_send_packet(dec.get(), pkt.get());
    av_packet_unref(pkt.get());

    if (sr < 0 && sr != EAGAIN) {
      return false;
    }

    while (true) {
      CREATE_FRAME_UNIQUE(f, av_frame_alloc());

      const int rr = avcodec_receive_frame(dec.get(), f.get());

      if (rr == AVERROR(EAGAIN)) {
        break;
      } else if (rr < 0) {
        break;
      }

      const int64_t pts = f->pts;

      if (pts != AV_NOPTS_VALUE && pts >= target) {
        tile = std::move(f);

        break;
      }
    }
  }

  return nullptr != tile;
}

[[nodiscard]] auto
extract_save_tile(FmtUniqType&    fmt,
                  const char*     output_path,
                  const AVStream* st,
                  const int       vidx,
                  const float     timestamp,
                  const int       wanted_width) -> bool
{
  const int64_t target =
    av_rescale_q_rnd(static_cast<int64_t>(timestamp * 1000.0),
                     AVRational{ 1, 1000 },
                     st->time_base,
                     AV_ROUND_NEAR_INF);

  if (0 > av_seek_frame(fmt.get(), vidx, target, AVSEEK_FLAG_BACKWARD)) {
    return false;
  }

  CREATE_CODECCTX_UNIQUE(
    dec, avcodec_alloc_context3(avcodec_find_decoder(st->codecpar->codec_id)));

  if (nullptr == dec) {
    return false;
  }

  if (0 > avcodec_parameters_to_context(dec.get(), st->codecpar)) {
    return false;
  }

  if (0 > avcodec_open2(
            dec.get(), avcodec_find_decoder(st->codecpar->codec_id), nullptr)) {
    return false;
  }

  CREATE_FRAME_UNIQUE(tile, {});
  if (!get_wanted_frame(tile, fmt, dec, vidx, target)) {
    return false;
  }

  CREATE_FRAME_UNIQUE(rgb, av_frame_alloc());
  if (!scale_tile(tile.get(), rgb.get(), wanted_width)) {
    return false;
  }

  return save_tile_png(output_path, rgb.get()) >= 0;
}

auto
extract_tiles(std::string video_filepath,
              uint16_t    total_tiles,
              uint16_t    tile_width,
              std::string output_folder) -> bool
{
  if (!std::filesystem::exists(video_filepath)) {
    return false;
  }

  AVFormatContext* temp_fmt = nullptr;

  if (avformat_open_input(&temp_fmt, video_filepath.c_str(), NULL, NULL) < 0) {
    return false;
  }

  std::unique_ptr<AVFormatContext, decltype(&close_avformat_context)> fmt(
    temp_fmt, &close_avformat_context);

  avformat_find_stream_info(fmt.get(), nullptr);

  const int vidx =
    av_find_best_stream(fmt.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

  if (0 > vidx) {
    return false;
  }

  const AVStream* st = fmt->streams[vidx];
  float duration     = static_cast<float>(st->duration) * av_q2d(st->time_base);

  if (!std::filesystem::exists(output_folder)) {
    if (!std::filesystem::create_directories(output_folder)) {
      return false;
    }
  }

  for (int i = 0; i < total_tiles; ++i) {
    const float timestamp = duration * static_cast<float>(i) / total_tiles;
    std::string tile_filepath =
      std::format("{}/tile_{:03d}.png", output_folder, i);

    if (false ==
        extract_save_tile(
          fmt, tile_filepath.c_str(), st, vidx, timestamp, tile_width)) {
      return false;
    }
  }

  return true;
}
}