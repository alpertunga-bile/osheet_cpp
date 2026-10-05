#include <format>
#include <print>
#include <tuple>

#include "metadata.hpp"
#include "sheet.hpp"
#include "tiles.hpp"

auto
main() -> int
{
  std::string video_filepath = "temp.mkv";
  uint8_t     total_column   = 4;
  uint8_t     total_row      = 4;

  auto&& png_tiles =
    osheet::extract_tiles(video_filepath, total_column * total_row, 320);

  if (png_tiles.empty()) {
    return 1;
  }

  osheet::Metadata metadata = {};

  if (!osheet::get_metadata(video_filepath, metadata)) {
    return 1;
  }

  osheet::Sheet sheet = {};
  sheet.set_font("Kode", 24.0f);

  auto [tile_width, tile_height] = sheet.get_tile_sizes(png_tiles[0]);

  SkScalar margin = 10.0f;
  SkScalar gap    = 5.0f;

  SkScalar line_h        = sheet.get_font_size() + 4.0f;
  SkScalar header_h      = margin + 11 * line_h + margin;
  SkScalar seperator_gap = 8.0f;

  SkScalar grid_w = total_column * tile_width + (total_column - 1) * gap;
  SkScalar grid_h = total_row * tile_height + (total_row - 1) * gap;

  SkScalar width  = margin * 2 + grid_w;
  SkScalar height = margin * 2 + header_h + seperator_gap + grid_h;

  SkScalar cur_x = margin, cur_y = margin + line_h;

  sheet.init(width, height);

  sheet.write_metadata(video_filepath, metadata, cur_y, margin, line_h);

  cur_y += seperator_gap;

  sheet.draw_tiles(png_tiles, cur_y, total_column, total_row, margin, gap);

  sheet.save("output.png");

  return 0;
}