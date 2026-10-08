#ifndef OSHEET_CPP_SHEET_HPP_
#define OSHEET_CPP_SHEET_HPP_

#include "metadata.hpp"

#include "skia/core/SkBitmap.h"
#include "skia/core/SkCanvas.h"
#include "skia/core/SkFont.h"
#include "skia/core/SkImage.h"
#include "skia/core/SkPaint.h"

#include <tuple>
#include <vector>

namespace osheet {

struct TileInfos;

class Sheet
{
public:
  auto init(int width = 640, int height = 320) -> void;
  auto set_font(std::string font_name,
                SkScalar    font_size   = 16.0f,
                std::string font_folder = "fonts") -> bool;

  auto draw_line(int x0, int y0, int x1, int y1, float line_width) -> bool;
  auto draw_text(std::string text, SkScalar x, SkScalar y) -> bool;

  auto save(std::string out_path) -> bool;

  inline auto get_font_size() -> SkScalar { return font.getSize(); }
  inline auto calc_text_width(std::string text) -> SkScalar
  {
    return font.measureText(text.c_str(), text.length(), SkTextEncoding::kUTF8);
  }
  inline auto get_total_meta_pairs() -> size_t { return meta_pairs.size(); }

  auto decode_store_images(const TileInfos& tile_infos) -> bool;
  auto create_meta_pairs(const std::string& video_filepath,
                         const Metadata&    metadata) -> void;

  auto write_metadata(SkScalar& cur_y, SkScalar margin, SkScalar line_h)
    -> void;

  auto draw_tiles(SkScalar& cur_y,
                  uint32_t  total_column,
                  SkScalar  margin,
                  SkScalar  gap) -> void;

  auto print_meta() -> void;

  auto get_tile_sizes() -> std::tuple<SkScalar, SkScalar>;

private:
  auto get_duration_str(SkScalar duration) -> std::string;

private:
  SkBitmap                                          bmp        = {};
  std::unique_ptr<SkCanvas>                         canvas     = nullptr;
  SkFont                                            font       = {};
  sk_sp<SkTypeface>                                 typeface   = nullptr;
  SkPaint                                           text_paint = {};
  std::vector<sk_sp<SkImage>>                       images     = {};
  std::vector<std::tuple<std::string, std::string>> meta_pairs = {};
};

}

#endif