#ifndef OSHEET_CPP_METADATA_HPP_
#define OSHEET_CPP_METADATA_HPP_

#include <string>

namespace osheet {

struct VideoMetadata
{
  std::string codec_name      = "";
  std::string profile         = {};
  size_t      width           = 0;
  size_t      height          = 0;
  std::string pix_fmt         = {};
  std::string color_space     = {};
  float       frame_rate      = 0.0f;
  double      duration        = 0.0;
  int64_t     bitrate         = 0;
  std::string color_transfer  = {};
  std::string color_primaries = {};
  std::string color_range     = {};
  std::string iformat_name    = {};
  std::string oformat_name    = {};
  float       avg_frame_rate  = 0.0f;
  int64_t     nb_frames       = 0;
};

struct AudioMetadata
{
  std::string codec_name    = {};
  std::string profile       = {};
  int         sample_rate   = 0;
  int         nb_channels   = 0;
  std::string ch_layout     = {};
  int64_t     bitrate       = 0;
  std::string sample_format = {};
};

struct Metadata
{
  VideoMetadata video = {};
  AudioMetadata audio = {};
};

auto
get_metadata(std::string video_filepath, Metadata& metadata) -> bool;

}

#endif