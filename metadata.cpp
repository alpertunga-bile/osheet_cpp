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

#include "ffmpeg_utils.hpp"
#include "indicator_utils.hpp"

namespace osheet {

auto
fill_video_metadata(VideoMetadata&           vmt,
                    FmtUniqType&             fmt,
                    const AVStream*          st,
                    const AVCodecParameters* pt) -> void
{
  const char*    media_type = av_get_media_type_string(pt->codec_type);
  const AVCodec* dec        = avcodec_find_decoder(pt->codec_id);
  double         duration =
    fmt->duration != AV_NOPTS_VALUE
      ? static_cast<double>(fmt->duration) / AV_TIME_BASE
      : static_cast<double>(st->duration) * av_q2d(st->time_base);

  vmt.codec_name = avcodec_get_name(pt->codec_id);
  vmt.profile = dec ? av_get_profile_name(dec, pt->profile) : "unknown profile";
  vmt.pix_fmt = av_get_pix_fmt_name(static_cast<AVPixelFormat>(pt->format));
  vmt.color_space =
    av_color_space_name(static_cast<AVColorSpace>(pt->color_space));
  vmt.width      = pt->width;
  vmt.height     = pt->height;
  vmt.duration   = duration;
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

  amt.codec_name      = avcodec_get_name(pt->codec_id);
  const char* profile = dec ? av_get_profile_name(dec, pt->profile) : nullptr;
  amt.profile         = profile ? std::string(profile) : "unknown profile";
  amt.sample_rate     = pt->sample_rate;
  amt.nb_channels     = pt->ch_layout.nb_channels;
  amt.ch_layout       = layout;
}

auto
get_metadata(std::string video_filepath, Metadata& metadata) -> bool
{
  osheet::Spinner spinner;
  spinner.init("Starting collecting metadata");

  spinner.tick();

  if (!std::filesystem::exists(video_filepath)) {
    spinner.set_values("File doesnot exist", Spinner::Status::FAILED_FINISHED);

    return false;
  }

  spinner.tick();

  AVFormatContext* temp_fmt = nullptr;

  if (avformat_open_input(&temp_fmt, video_filepath.c_str(), NULL, NULL) < 0) {
    spinner.set_values("File cannot open", Spinner::Status::FAILED_FINISHED);

    return false;
  }

  spinner.set_values("Video file is opened", Spinner::Status::IN_PROGRESS);
  spinner.tick();

  CREATE_FMT_UNIQUE(fmt, temp_fmt);

  spinner.tick(5);

  avformat_find_stream_info(fmt.get(), nullptr);

  for (unsigned int i = 0; i < fmt->nb_streams; ++i) {
    const AVStream*          st = fmt->streams[i];
    const AVCodecParameters* pt = st->codecpar;

    switch (pt->codec_type) {
      case AVMEDIA_TYPE_VIDEO:
        spinner.set_values("Collecting video metadata",
                           Spinner::Status::IN_PROGRESS);
        fill_video_metadata(metadata.video, fmt, st, pt);
        spinner.tick();
        break;
      case AVMEDIA_TYPE_AUDIO:
        spinner.set_values("Collecting audio metadata",
                           Spinner::Status::IN_PROGRESS);
        fill_audio_metadata(metadata.audio, st, pt);
        spinner.tick();
        break;
      default:
        break;
    }
  }

  spinner.set_values("Metadata values are collected",
                     Spinner::Status::SUCCESS_FINISHED);

  return true;
}

} // end of osheet namespace
