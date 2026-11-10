#pragma once

#include "StarFile.hpp"
#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>

namespace Star::Test {

// Owns only an exclusively created temporary directory; no engine automatic
// RNG seed, user installation, installed assets, logs or saves are used.
class GenerationTestDirectory {
public:
  GenerationTestDirectory() {
    static std::atomic<uint64_t> counter{0};
    auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
      auto path = std::filesystem::temp_directory_path() / ("openstarbound-generation-fixture-" + std::to_string(tick)
          + "-" + std::to_string(counter.fetch_add(1)));
      std::error_code error;
      if (std::filesystem::create_directory(path, error)) { m_path = std::move(path); return; }
      if (error && error != std::errc::file_exists) throw std::filesystem::filesystem_error("create generation fixture", path, error);
    }
    throw std::runtime_error("Cannot create an exclusive generation fixture directory");
  }
  GenerationTestDirectory(GenerationTestDirectory const&) = delete;
  GenerationTestDirectory& operator=(GenerationTestDirectory const&) = delete;
  ~GenerationTestDirectory() {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
    EXPECT_FALSE(error) << error.message();
  }
  String path() const { return String(m_path.u8string()); }
  void write(String const& name, String const& data) const {
    if (name.empty() || name.hasChar('/') || name.hasChar('\\') || name.contains(".."))
      throw std::runtime_error("Fixture writes must name one local file");
    File::writeFile(data, File::relativeTo(path(), name));
  }
private:
  std::filesystem::path m_path;
};

}
