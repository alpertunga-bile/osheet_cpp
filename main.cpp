#include "metadata.hpp"
#include <print>

auto
main() -> int
{
  std::string      video_filepath = "test.mp4";
  osheet::Metadata mt             = {};

  if (!osheet::get_metadata(video_filepath, mt)) {
    std::println("Cant finished");
    return 1;
  }

  std::println("finished");

  return 0;
}