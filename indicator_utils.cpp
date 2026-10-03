#include "indicator_utils.hpp"

namespace osheet {

auto
create_spinner(indicators::ProgressSpinner& spinner, const char* text) -> void
{
  set_spinner_values(spinner, text, SpinnerStatus::STARTED);
}

auto
set_spinner_values(indicators::ProgressSpinner& spinner,
                   const char*                  text,
                   SpinnerStatus                status) -> void
{
  spinner.set_option(indicators::option::PostfixText{ text });
  spinner.set_option(indicators::option::ShowPercentage{ false });

  switch (status) {
    case SpinnerStatus::IN_PROGRESS:
    case SpinnerStatus::STARTED:
      spinner.set_option(
        indicators::option::ForegroundColor{ indicators::Color::yellow });
      break;
    case SpinnerStatus::SUCCESS_FINISHED:
      spinner.set_option(
        indicators::option::ForegroundColor{ indicators::Color::green });
      spinner.set_option(indicators::option::PrefixText{ "✔" });
      spinner.set_option(indicators::option::ShowSpinner{ false });
      spinner.mark_as_completed();
      break;
    case SpinnerStatus::FAILED_FINISHED:
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
iter_spinner(indicators::ProgressSpinner& spinner,
             int                          loop_count,
             int64_t                      ms_sleep) -> void
{
  for (int i = 0; i < loop_count; ++i) {
    spinner.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(ms_sleep));
  }
}

}