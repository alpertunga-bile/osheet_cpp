#ifndef OSHEET_CPP_TILES_HPP_
#define OSHEET_CPP_TILES_HPP_

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace osheet {

struct TileInfos
{
  std::vector<size_t>  indices;
  std::vector<uint8_t> tiles;
};

auto
extract_tiles(std::string video_filepath,
              uint16_t    total_tiles,
              uint16_t    tile_width) -> TileInfos;

}

#endif