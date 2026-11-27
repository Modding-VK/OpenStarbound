#pragma once

#include <cstdint>

namespace Star {

// First H5 / MOD-04 cut: subsystem/platform compatibility matrix. Linux Clang
// remains deferred; GPU compute stays optional with CPU-complete fallback.

enum class WorldGenerationPlatform : uint8_t {
  Windows = 0,
  LinuxGcc,
  LinuxArm,
  MacOs,
  LinuxClangDeferred,
  Count
};

enum class WorldGenerationSubsystem : uint8_t {
  Generation = 0,
  Scripts,
  Save,
  Network,
  HardwareCompute,
  Count
};

enum class WorldGenerationCompatStatus : uint8_t {
  Supported = 0,
  Optional,
  Deferred,
  Unsupported,
  Count
};

struct WorldGenerationPlatformSupport {
  WorldGenerationPlatform platform;
  WorldGenerationCompatStatus status;
};

struct WorldGenerationSubsystemSupport {
  WorldGenerationSubsystem subsystem;
  WorldGenerationCompatStatus status;
  bool cpuFallbackRequired = false;
};

inline WorldGenerationPlatformSupport worldGenerationPlatformSupport(
    WorldGenerationPlatform platform) noexcept {
  switch (platform) {
    case WorldGenerationPlatform::Windows:
    case WorldGenerationPlatform::LinuxGcc:
    case WorldGenerationPlatform::LinuxArm:
    case WorldGenerationPlatform::MacOs:
      return {platform, WorldGenerationCompatStatus::Supported};
    case WorldGenerationPlatform::LinuxClangDeferred:
      return {platform, WorldGenerationCompatStatus::Deferred};
    case WorldGenerationPlatform::Count:
      break;
  }
  return {WorldGenerationPlatform::LinuxClangDeferred, WorldGenerationCompatStatus::Deferred};
}

inline WorldGenerationSubsystemSupport worldGenerationSubsystemSupport(
    WorldGenerationSubsystem subsystem) noexcept {
  switch (subsystem) {
    case WorldGenerationSubsystem::Generation:
    case WorldGenerationSubsystem::Scripts:
    case WorldGenerationSubsystem::Save:
    case WorldGenerationSubsystem::Network:
      return {subsystem, WorldGenerationCompatStatus::Supported, false};
    case WorldGenerationSubsystem::HardwareCompute:
      return {subsystem, WorldGenerationCompatStatus::Optional, true};
    case WorldGenerationSubsystem::Count:
      break;
  }
  return {WorldGenerationSubsystem::HardwareCompute, WorldGenerationCompatStatus::Optional, true};
}

inline bool worldGenerationPlatformInScope(WorldGenerationPlatform platform) noexcept {
  return worldGenerationPlatformSupport(platform).status == WorldGenerationCompatStatus::Supported;
}

inline bool worldGenerationSubsystemRequiresCpuFallback(
    WorldGenerationSubsystem subsystem) noexcept {
  return worldGenerationSubsystemSupport(subsystem).cpuFallbackRequired;
}

inline uint32_t worldGenerationSupportedPlatformCount() noexcept {
  uint32_t count = 0;
  for (uint32_t i = 0; i < static_cast<uint32_t>(WorldGenerationPlatform::Count); ++i) {
    if (worldGenerationPlatformInScope(static_cast<WorldGenerationPlatform>(i)))
      ++count;
  }
  return count;
}

}
