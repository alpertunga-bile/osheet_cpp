#include "sheet.hpp"

#include "include/encode/SkPngEncoder.h"
#include "skia/core/SkColor.h"
#include "skia/core/SkFontMetrics.h"
#include "skia/core/SkFontMgr.h"
#include "skia/core/SkImage.h"
#include "skia/core/SkStream.h"
#include "skia/core/SkTypeface.h"

#include "skia/ports/SkFontMgr_directory.h"

#include "tiles.hpp"
#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <print>
#include <string>

namespace osheet {

auto
Sheet::init(int width, int height) -> void
{
  bmp.allocPixels(SkImageInfo::MakeN32Premul(width, height));

  canvas = std::make_unique<SkCanvas>(bmp);

  canvas->drawColor(SK_ColorBLACK);

  text_paint.setAntiAlias(true);
  text_paint.setColor(SK_ColorWHITE);
}

auto
Sheet::set_font(std::string font_name,
                SkScalar    font_size,
                std::string font_folder) -> bool
{
  auto&& mgr = SkFontMgr_New_Custom_Directory(font_folder.c_str());
  typeface   = mgr->matchFamilyStyle(font_name.c_str(), SkFontStyle::Normal());

  if (!typeface) {
    return false;
  }

  font.setTypeface(typeface);
  font.setSize(font_size);
  font.setEdging(SkFont::Edging::kAntiAlias);

  return true;
}

auto
Sheet::draw_line(int x0, int y0, int x1, int y1, float line_width) -> bool
{
  SkPaint line_paint = {};

  line_paint.setStyle(SkPaint::kStroke_Style);
  line_paint.setStrokeWidth(line_width);
  line_paint.setColor(SK_ColorWHITE);
  line_paint.setAntiAlias(true);

  canvas->drawLine(x0, y0, x1, y1, line_paint);

  return true;
}

auto
Sheet::draw_text(std::string text, SkScalar x, SkScalar y) -> bool
{
  canvas->drawString(text.c_str(), x, y, font, text_paint);

  return true;
}

auto
Sheet::save(std::string out_path) -> bool
{
  SkPngEncoder::Options options = {};
  sk_sp<SkData>         png     = SkPngEncoder::Encode(bmp.pixmap(), options);

  if (!png) {
    return false;
  }

  std::ofstream out(out_path, std::ios::out | std::ios::binary);

  if (!out.is_open()) {
    return false;
  }

  out.write(static_cast<const char*>(png->data()),
            static_cast<std::streamsize>(png->size()));

  out.close();

  return out.good();
}

auto
Sheet::decode_store_images(const TileInfos& tile_infos) -> bool
{
  const size_t total_images = tile_infos.indices.size() - 1;

  for (size_t i = 0; i < total_images; ++i) {
    const size_t start_point = tile_infos.indices[i];
    const size_t next_point  = tile_infos.indices[i + 1];

    sk_sp<SkData> data = SkData::MakeWithoutCopy(
      tile_infos.tiles.data() + start_point, next_point - start_point);
    sk_sp<SkImage> image = SkImages::DeferredFromEncodedData(data);

    if (nullptr == image) {
      return false;
    }

    images.push_back(image);
  }

  return true;
}

auto
Sheet::create_meta_pairs(const std::string& video_filepath,
                         const Metadata&    metadata) -> void
{
  auto&& filename = std::filesystem::path(video_filepath).filename();

  meta_pairs.push_back({ "Filename", filename });
  meta_pairs.push_back(
    { "Resolution",
      std::format("{}x{}", metadata.video.width, metadata.video.height) });
  meta_pairs.push_back(
    { "Duration", get_duration_str(metadata.video.duration) });
  meta_pairs.push_back(
    { "Number of Frames", std::format("{}", metadata.video.nb_frames) });
  meta_pairs.push_back({ "Video Codec",
                         std::format("{} {}",
                                     metadata.video.codec_name,
                                     metadata.video.profile) });
  meta_pairs.push_back(
    { "Video Bitrate", std::format("{} bps", metadata.video.bitrate) });
  meta_pairs.push_back({ "Frame Rate / Avg Frame Rate",
                         std::format("{:0.2f} fps / {:0.2f} fps",
                                     metadata.video.frame_rate,
                                     metadata.video.avg_frame_rate) });
  meta_pairs.push_back({ "Pixel Format", metadata.video.pix_fmt });
  meta_pairs.push_back({ "Color Space/Transfer/Primaries/Range",
                         std::format("{} / {} / {} / {}",
                                     metadata.video.color_space,
                                     metadata.video.color_transfer,
                                     metadata.video.color_primaries,
                                     metadata.video.color_range) });
  meta_pairs.push_back({ "Input / Output Format",
                         std::format("{} / {}",
                                     metadata.video.iformat_name,
                                     metadata.video.oformat_name) });
  meta_pairs.push_back({ "Audio Codec",
                         std::format("{} {} {} {} channels",
                                     metadata.audio.codec_name,
                                     metadata.audio.profile,
                                     metadata.audio.ch_layout,
                                     metadata.audio.nb_channels) });
  meta_pairs.push_back(
    { "Audio Bitrate", std::format("{} bps", metadata.audio.bitrate) });

  meta_pairs.push_back({ "Audio Sample",
                         std::format("{} Hz {}",
                                     metadata.audio.sample_rate,
                                     metadata.audio.sample_format) });
}

auto
Sheet::write_metadata(SkScalar& cur_y, SkScalar margin, SkScalar line_h) -> void
{
  SkScalar max_width = 0.0f;
  SkScalar cur_x     = margin;

  for (auto [name, value] : meta_pairs) {
    SkScalar width = calc_text_width(name + " ");

    max_width = std::max(max_width, width);
  }

  for (auto [name, value] : meta_pairs) {
    draw_text(name, cur_x, cur_y);
    draw_text(": ", cur_x + max_width, cur_y);
    draw_text(value, cur_x + max_width + calc_text_width(": "), cur_y);

    cur_y += line_h;
  }
}

auto
Sheet::draw_tiles(SkScalar& cur_y,
                  uint32_t  total_column,
                  uint32_t  total_row,
                  SkScalar  margin,
                  SkScalar  gap) -> void
{
  const size_t total_tiles = images.size();

  sk_sp<SkImage> image = images[0];

  SkScalar width  = static_cast<SkScalar>(image->width());
  SkScalar height = static_cast<SkScalar>(image->height());

  for (size_t i = 0; i < total_tiles; ++i) {
    SkScalar column = static_cast<SkScalar>(i % total_column);
    SkScalar row    = static_cast<SkScalar>(i / total_column);

    image = images[i];

    SkScalar x = margin + column * (width + gap);
    SkScalar y = cur_y + row * (height + gap);

    canvas->drawImage(image, x, y);
  }
}

auto
Sheet::get_tile_sizes() -> std::tuple<SkScalar, SkScalar>
{
  sk_sp<SkImage> image = images[0];

  SkScalar width  = static_cast<SkScalar>(image->width());
  SkScalar height = static_cast<SkScalar>(image->height());

  return std::make_tuple(width, height);
}

auto
Sheet::get_duration_str(SkScalar duration) -> std::string
{
  size_t pruned_duration = duration;

  size_t hours     = pruned_duration / 3600;
  pruned_duration -= hours * 3600;

  size_t minutes   = pruned_duration / 60;
  pruned_duration -= minutes * 60;

  return std::format("{:02u}:{:02u}:{:02u}", hours, minutes, pruned_duration);
}
}