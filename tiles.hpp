#ifndef OSHEET_CPP_TILES_HPP_
#define OSHEET_CPP_TILES_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace osheet {

auto
extract_tiles(std::string video_filepath,
              uint16_t    total_tiles,
              uint16_t    tile_width) -> std::vector<std::vector<uint8_t>>;

}

#endif