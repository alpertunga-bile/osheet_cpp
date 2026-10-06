#include <filesystem>
#include <format>
#include <limits>
#include <print>
#include <tuple>

#include "metadata.hpp"
#include "sheet.hpp"
#include "tiles.hpp"

#include "argparse/argparse.hpp"

#define CHECK_FLOAT_VALUE(__var__, __msg__)                                    \
  if ((__var__) <= 0.0f || (__var__) > std::numeric_limits<float>::max()) {    \
    std::print("{} {} isnot applicable", __var__, __msg__);                    \
                                                                               \
    return 1;                                                                  \
  }

#define CHECK_UINT32_VALUE(__var__, __msg__)                                   \
  if (0 == (__var__) || (__var__) > std::numeric_limits<uint32_t>::max()) {    \
    std::print("{} {} isnot applicable", __var__, __msg__);                    \
                                                                               \
    return 1;                                                                  \
  }

#define CHECK_PATH_EXIST(__var__, __msg__)                                     \
  if (!std::filesystem::exists(__var__)) {                                     \
    std::print("{} {} doesnot exist", __var__, __msg__);                       \
                                                                               \
    return 1;                                                                  \
  }

auto
main(int argc, char* argv[]) -> int
{
  argparse::ArgumentParser program("osheet_cpp", "0.1.0");

  program.add_argument("-i", "--input")
    .required()
    .help("specify the input video file");
  program.add_argument("-f", "--font")
    .default_value("Kode")
    .help("Font style for the metadata");
  program.add_argument("-fd", "--font_dir")
    .default_value("fonts")
    .help("Directory that contains the font style");
  program.add_argument("-fs", "--font_size")
    .default_value(16)
    .help("Size of the font style for the metadata")
    .scan<'i', int>();
  program.add_argument("-r", "--rows")
    .default_value(4)
    .help("Total tile rows in the output")
    .scan<'i', int>();
  program.add_argument("-c", "--cols")
    .default_value(4)
    .help("Total tile columns in the output")
    .scan<'i', int>();
  program.add_argument("-tw", "--tile_w")
    .default_value(320)
    .help("Width of one tile")
    .scan<'i', int>();
  program.add_argument("-g", "--gap")
    .default_value(5)
    .help("Spacing between tiles")
    .scan<'i', int>();
  program.add_argument("-m", "--margin")
    .default_value(10)
    .help("Extra space between text and tiles")
    .scan<'i', int>();
  program.add_argument("-sg", "--sep_gap")
    .default_value(8)
    .help("Extra space between text and tiles")
    .scan<'i', int>();
  program.add_argument("-o", "--output")
    .default_value("sheet.png")
    .help("Output file");

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;

    return 1;
  }

  std::string video_filepath    = program.get<std::string>("--input");
  std::string font_name         = program.get<std::string>("--font");
  std::string font_folder       = program.get<std::string>("--font_dir");
  SkScalar    font_size         = program.get<int>("--font_size");
  uint32_t    total_row         = program.get<int>("--rows");
  uint32_t    total_column      = program.get<int>("--cols");
  uint32_t    wanted_tile_width = program.get<int>("--tile_w");
  SkScalar    gap               = program.get<int>("--gap");
  SkScalar    margin            = program.get<int>("--margin");
  SkScalar    seperator_gap     = program.get<int>("--sep_gap");
  std::string output_filepath   = program.get<std::string>("--output");

  CHECK_PATH_EXIST(video_filepath, "video file");
  CHECK_PATH_EXIST(font_folder, "font folder");

  CHECK_FLOAT_VALUE(font_size, "font size");

  CHECK_UINT32_VALUE(total_row, "total row");
  CHECK_UINT32_VALUE(total_column, "total column");
  CHECK_UINT32_VALUE(wanted_tile_width, "wanted tile width");

  CHECK_FLOAT_VALUE(gap, "gap");
  CHECK_FLOAT_VALUE(margin, "margin");
  CHECK_FLOAT_VALUE(seperator_gap, "seperator gap");

  auto&& png_tiles = osheet::extract_tiles(
    video_filepath, total_column * total_row, wanted_tile_width);

  if (png_tiles.empty()) {
    std::print("Cannot get video tiles from {} file", video_filepath);

    return 1;
  }

  osheet::Metadata metadata = {};

  if (!osheet::get_metadata(video_filepath, metadata)) {
    std::print("Cannot get metadata from {} file", video_filepath);

    return 1;
  }

  osheet::Sheet sheet = {};
  sheet.set_font(font_name, font_size, font_folder);

  auto [tile_width, tile_height] = sheet.get_tile_sizes(png_tiles[0]);

  if (0 == tile_width || 0 == tile_height) {
    std::print("Cannot get tile width or height");

    return 1;
  }

  SkScalar line_h   = sheet.get_font_size() + 4.0f;
  SkScalar header_h = margin + 10 * line_h + margin;

  SkScalar grid_w = total_column * tile_width + (total_column - 1) * gap;
  SkScalar grid_h = total_row * tile_height + (total_row - 1) * gap;

  SkScalar width  = margin * 2 + grid_w;
  SkScalar height = margin * 2 + header_h + seperator_gap + line_h + grid_h;

  SkScalar cur_x = margin;
  SkScalar cur_y = margin + line_h;

  sheet.init(width, height);

  sheet.write_metadata(video_filepath, metadata, cur_y, margin, line_h);

  sheet.draw_line(0, cur_y, width, cur_y, 4.0f);

  cur_y += seperator_gap + line_h;

  sheet.draw_tiles(png_tiles, cur_y, total_column, total_row, margin, gap);

  if (!sheet.save(output_filepath)) {
    std::print("Cannot save to {} file", output_filepath);

    return 1;
  }

  return 0;
}