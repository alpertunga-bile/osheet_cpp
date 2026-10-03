#include "metadata.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

#include <filesystem>
#include <memory>
#include <print>

#include "indicator_utils.hpp"

namespace osheet {

void
close_avformat_context(AVFormatContext* fmt)
{
  if (nullptr != fmt) {
    avformat_close_input(&fmt);
  }
}

auto
fill_video_metadata(VideoMetadata&           vmt,
                    const AVStream*          st,
                    const AVCodecParameters* pt) -> void
{
  const char*    media_type = av_get_media_type_string(pt->codec_type);
  const AVCodec* dec        = avcodec_find_decoder(pt->codec_id);

  vmt.codec_name = avcodec_get_name(pt->codec_id);
  vmt.profile    = dec ? av_get_profile_name(dec, pt->profile) : "unknown";
  vmt.pix_fmt    = av_get_pix_fmt_name(static_cast<AVPixelFormat>(pt->format));
  vmt.color_space =
    av_color_space_name(static_cast<AVColorSpace>(pt->color_space));
  vmt.width      = pt->width;
  vmt.height     = pt->height;
  vmt.duration   = static_cast<double>(st->duration) * av_q2d(st->time_base);
  vmt.frame_rate = 0 == st->r_frame_rate.den
                     ? 0.0f
                     : static_cast<float>(st->r_frame_rate.num) /
                         static_cast<float>(st->r_frame_rate.den);
}

auto
fill_audio_metadata(AudioMetadata&           amt,
                    const AVStream*          st,
                    const AVCodecParameters* pt) -> void
{
  const AVCodec* dec = avcodec_find_decoder(pt->codec_id);

  char layout[64];
  av_channel_layout_describe(&pt->ch_layout, layout, sizeof(layout));

  amt.codec_name  = avcodec_get_name(pt->codec_id);
  amt.profile     = dec ? av_get_profile_name(dec, pt->profile) : "unknown";
  amt.sample_rate = pt->sample_rate;
  amt.nb_channels = pt->ch_layout.nb_channels;
  amt.ch_layout   = layout;
}

auto
get_metadata(std::string video_filepath, Metadata& metadata) -> bool
{
  indicators::ProgressSpinner spinner;
  osheet::create_spinner(spinner, "Starting collecting metadata");

  osheet::iter_spinner(spinner);

  if (!std::filesystem::exists(video_filepath)) {
    osheet::set_spinner_values(
      spinner, "File doesnot exist", SpinnerStatus::FAILED_FINISHED);

    return false;
  }

  osheet::iter_spinner(spinner);

  AVFormatContext* temp_fmt = nullptr;

  if (avformat_open_input(&temp_fmt, video_filepath.c_str(), NULL, NULL) < 0) {
    osheet::set_spinner_values(
      spinner, "File cannot open", SpinnerStatus::FAILED_FINISHED);

    return false;
  }

  osheet::set_spinner_values(
    spinner, "Video file is opened", SpinnerStatus::IN_PROGRESS);
  osheet::iter_spinner(spinner);

  std::unique_ptr<AVFormatContext, decltype(&close_avformat_context)> fmt(
    temp_fmt, &close_avformat_context);

  osheet::iter_spinner(spinner, 5);

  avformat_find_stream_info(fmt.get(), nullptr);

  for (unsigned int i = 0; i < fmt->nb_streams; ++i) {
    const AVStream*          st = fmt->streams[i];
    const AVCodecParameters* pt = st->codecpar;

    switch (pt->codec_type) {
      case AVMEDIA_TYPE_VIDEO:
        osheet::set_spinner_values(
          spinner, "Collecting video metadata", SpinnerStatus::IN_PROGRESS);
        fill_video_metadata(metadata.video, st, pt);
        break;
      case AVMEDIA_TYPE_AUDIO:
        osheet::set_spinner_values(
          spinner, "Collecting audio metadata", SpinnerStatus::IN_PROGRESS);
        fill_audio_metadata(metadata.audio, st, pt);
        break;
      default:
        break;
    }

    osheet::iter_spinner(spinner, 30);
  }

  osheet::set_spinner_values(
    spinner, "Metadata values are collected", SpinnerStatus::SUCCESS_FINISHED);

  return true;
}

} // end of osheet namespace
