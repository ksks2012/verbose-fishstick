#include "log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <string>

namespace noto3d::core {

void log_init(std::string_view name) {
    auto logger = spdlog::stdout_color_mt(std::string(name));
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    spdlog::set_level(spdlog::level::trace);
}

}  // namespace noto3d::core
