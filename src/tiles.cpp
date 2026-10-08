#include "tiles.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include "ffmpeg_utils.hpp"
#include "indicator_utils.hpp"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <utility>

namespace osheet {

[[nodiscard]] auto
copy_tile_data(std::vector<uint8_t>& tile, FrameUniqType& src) -> bool
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

  if (avcodec_send_frame(enc.get(), src.get()) < 0) {
    return false;
  }

  if (avcodec_send_frame(enc.get(), nullptr) < 0) {
    return false;
  }

  CREATE_PACKET_UNIQUE(pkt, av_packet_alloc());

  while (avcodec_receive_packet(enc.get(), pkt.get()) >= 0) {
    std::vector<uint8_t> pkt_vec(pkt->data,
                                 pkt->data + static_cast<size_t>(pkt->size));
    tile.insert_range(tile.end(), pkt_vec);

    av_packet_unref(pkt.get());
  }

  return true;
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

    if (sr < 0 && sr != AVERROR(EAGAIN)) {
      avcodec_receive_frame(dec.get(), nullptr);

      continue;
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
extract_tile(FmtUniqType&          fmt,
             std::vector<uint8_t>& output_tile,
             const AVStream*       st,
             const int             vidx,
             const float           timestamp,
             const int             wanted_width) -> bool
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

  return copy_tile_data(output_tile, rgb);
}

auto
extract_tiles(std::string video_filepath,
              uint16_t    total_tiles,
              uint16_t    tile_width) -> TileInfos
{
  osheet::ProgressBar pbar;
  pbar.init("Starting extraction", total_tiles);

  if (!std::filesystem::exists(video_filepath)) {
    pbar.set_values(
      "File doesnot exist", 100, osheet::ProgressBar::Status::FAILED_FINISHED);

    return {};
  }

  AVFormatContext* temp_fmt = nullptr;

  if (avformat_open_input(&temp_fmt, video_filepath.c_str(), NULL, NULL) < 0) {
    pbar.set_values("Cannot open the video file",
                    100,
                    osheet::ProgressBar::Status::FAILED_FINISHED);

    return {};
  }

  std::unique_ptr<AVFormatContext, decltype(&close_avformat_context)> fmt(
    temp_fmt, &close_avformat_context);

  avformat_find_stream_info(fmt.get(), nullptr);

  const int vidx =
    av_find_best_stream(fmt.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

  if (0 > vidx) {
    pbar.set_values("Cannot find video stream",
                    100,
                    osheet::ProgressBar::Status::FAILED_FINISHED);

    return {};
  }

  const AVStream* st = fmt->streams[vidx];
  float duration = fmt->duration != AV_NOPTS_VALUE
                     ? static_cast<float>(fmt->duration) / AV_TIME_BASE
                     : static_cast<float>(st->duration) * av_q2d(st->time_base);

  TileInfos infos = {};
  infos.indices.push_back(0);

  for (uint16_t i = 0; i < total_tiles; ++i) {
    std::vector<uint8_t> tile = {};
    const float timestamp     = duration * static_cast<float>(i) / total_tiles;

    if (false == extract_tile(fmt, tile, st, vidx, timestamp, tile_width)) {
      pbar.set_values("Extracting one tile is failed",
                      100,
                      osheet::ProgressBar::Status::FAILED_FINISHED);

      return {};
    }

    pbar.set_values(
      "Extracting tiles", i, osheet::ProgressBar::Status::IN_PROGRESS);

    infos.tiles.insert(infos.tiles.end(), tile.begin(), tile.end());
    infos.indices.push_back(infos.tiles.size());
  }

  pbar.set_values("Extracting tiles is completed",
                  100,
                  osheet::ProgressBar::Status::SUCCESS_FINISHED);

  return infos;
}

}