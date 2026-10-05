#include "sheet.hpp"

#include "include/encode/SkPngEncoder.h"
#include "skia/core/SkColor.h"
#include "skia/core/SkFontMetrics.h"
#include "skia/core/SkFontMgr.h"
#include "skia/core/SkImage.h"
#include "skia/core/SkStream.h"
#include "skia/core/SkTypeface.h"

#include "skia/ports/SkFontMgr_directory.h"

#include <filesystem>
#include <format>
#include <memory>
#include <string>

#include <fstream>

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

  return true;
}

auto
Sheet::write_metadata(const std::string& video_filepath,
                      const Metadata&    metadata,
                      SkScalar&          cur_y,
                      SkScalar           margin,
                      SkScalar           line_h) -> void
{
  auto&& filename = std::filesystem::path(video_filepath).filename();

  std::vector<std::tuple<std::string, std::string>> pairs = {
    { "Filename", filename },
    { "Resolution",
     std::format("{}x{}", metadata.video.width, metadata.video.height) },
    { "Duration", get_duration_str(metadata.video.duration) },
    { "Video Codec",
     std::format("{} {}", metadata.video.codec_name, metadata.video.profile) },
    { "Frame Rate", std::format("{:0.2f} fps", metadata.video.frame_rate) },
    { "Pixel Format", metadata.video.pix_fmt },
    { "Color Space", metadata.video.color_space },
    { "Audio",
     std::format("{} {} {} {} {} channels",
     metadata.audio.codec_name,
     metadata.audio.profile,
     metadata.audio.ch_layout,
     metadata.audio.sample_rate,
     metadata.audio.nb_channels) }
  };

  SkScalar max_width = 0.0f;
  SkScalar cur_x     = margin;

  for (auto [name, value] : pairs) {
    SkScalar width = calc_text_width(name + " ");

    max_width = std::max(max_width, width);
  }

  for (auto [name, value] : pairs) {
    draw_text(name, cur_x, cur_y);
    draw_text(": ", cur_x + max_width, cur_y);
    draw_text(value, cur_x + max_width + calc_text_width(": "), cur_y);

    cur_y += line_h;
  }
}

auto
Sheet::draw_tiles(const std::vector<std::vector<uint8_t>>& png_tiles,
                  SkScalar&                                cur_y,
                  uint8_t                                  total_column,
                  uint8_t                                  total_row,
                  SkScalar                                 margin,
                  SkScalar                                 gap) -> void
{
  const uint8_t total_tiles = png_tiles.size();

  for (uint8_t i = 0; i < total_tiles; ++i) {
    SkScalar column = static_cast<SkScalar>(i % total_column);
    SkScalar row    = static_cast<SkScalar>(i / total_column);

    sk_sp<SkData> data =
      SkData::MakeWithCopy(png_tiles[i].data(), png_tiles[i].size());
    sk_sp<SkImage> image = SkImages::DeferredFromEncodedData(data);

    SkScalar width  = static_cast<SkScalar>(image->width());
    SkScalar height = static_cast<SkScalar>(image->height());

    SkScalar x = margin + column * (width + gap);
    SkScalar y = cur_y + row * (height + gap);

    canvas->drawImage(image, x, y);
  }
}

std::tuple<SkScalar, SkScalar>
Sheet::get_tile_sizes(const std::vector<uint8_t>& tile)
{
  sk_sp<SkData>  data  = SkData::MakeWithCopy(tile.data(), tile.size());
  sk_sp<SkImage> image = SkImages::DeferredFromEncodedData(data);

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

  return std::format("{:02d}:{:02d}:{:02d}", hours, minutes, pruned_duration);
}

}