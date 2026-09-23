#pragma once

#include <spdlog/spdlog.h>
#include <string_view>

namespace noto3d::core {

/// Initialise the project-wide spdlog logger.  Call once from main() before
/// any other noto3d code emits log messages.
void log_init(std::string_view name = "noto3d");

}  // namespace noto3d::core

// ---------------------------------------------------------------------------
// Convenience macros — thin wrappers around spdlog's default logger so that
// all noto3d code uses a single, replaceable call-site spelling (ADR-0008).
// ---------------------------------------------------------------------------
#define NOTO3D_TRACE(...)    ::spdlog::trace(__VA_ARGS__)
#define NOTO3D_DEBUG(...)    ::spdlog::debug(__VA_ARGS__)
#define NOTO3D_INFO(...)     ::spdlog::info(__VA_ARGS__)
#define NOTO3D_WARN(...)     ::spdlog::warn(__VA_ARGS__)
#define NOTO3D_ERROR(...)    ::spdlog::error(__VA_ARGS__)
#define NOTO3D_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)
