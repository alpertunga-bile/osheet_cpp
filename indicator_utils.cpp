#include "indicator_utils.hpp"

namespace osheet {

auto
Spinner::init(const char* text) -> void
{
  set_values(text, Spinner::Status::STARTED);
}

auto
Spinner::set_values(const char* text, Spinner::Status status) -> void
{
  spinner.set_option(indicators::option::PostfixText{ text });
  spinner.set_option(indicators::option::ShowPercentage{ false });

  switch (status) {
    case Spinner::Status::IN_PROGRESS:
    case Spinner::Status::STARTED:
      spinner.set_option(
        indicators::option::ForegroundColor{ indicators::Color::yellow });
      break;
    case Spinner::Status::SUCCESS_FINISHED:
      spinner.set_option(
        indicators::option::ForegroundColor{ indicators::Color::green });
      spinner.set_option(indicators::option::PrefixText{ "✔" });
      spinner.set_option(indicators::option::ShowSpinner{ false });
      spinner.mark_as_completed();
      break;
    case Spinner::Status::FAILED_FINISHED:
      spinner.set_option(
        indicators::option::ForegroundColor{ indicators::Color::red });
      spinner.set_option(indicators::option::PrefixText{ "✖" });
      spinner.set_option(indicators::option::ShowSpinner{ false });
      spinner.mark_as_completed();
      break;
    default:
      break;
  }
}

auto
Spinner::tick(int loop_count, int64_t ms_sleep) -> void
{
  for (int i = 0; i < loop_count; ++i) {
    spinner.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(ms_sleep));
  }
}

}