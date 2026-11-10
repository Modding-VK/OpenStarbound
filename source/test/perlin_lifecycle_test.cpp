#include "StarPerlin.hpp"

#include <array>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"

using namespace Star;

namespace {
template <typename F> auto bits(F value) {
  std::array<unsigned char, sizeof(F)> out;
  std::memcpy(out.data(), &value, sizeof(F));
  return out;
}

template <typename F> Perlin<F> noise(PerlinType type = PerlinType::Perlin) {
  return Perlin<F>(type, 3, F(0.03125), F(1.5), F(-0.125), F(2), F(2), 731);
}

template <typename F> void samplesEqual(Perlin<F> const& a, Perlin<F> const& b) {
  EXPECT_EQ(a.toJson(), b.toJson());
  EXPECT_EQ(a.allocatedStorageBytes(), b.allocatedStorageBytes());
  for (int i = -8; i != 9; ++i) {
    F x = F(i) / 2, y = F(i + 3) / 4, z = F(i - 2) / 8;
    EXPECT_EQ(bits(a.get(x)), bits(b.get(x)));
    EXPECT_EQ(bits(a.get(x, y)), bits(b.get(x, y)));
    EXPECT_EQ(bits(a.get(x, y, z)), bits(b.get(x, y, z)));
  }
}

template <typename F> void defaultMetadata() {
  Perlin<F> empty;
  EXPECT_EQ(PerlinType::Uninitialized, empty.type());
  EXPECT_EQ(0u, empty.allocatedStorageBytes());
  EXPECT_EQ(0u, empty.octaves());
  EXPECT_EQ(F(0), empty.frequency()); EXPECT_EQ(F(0), empty.amplitude());
  EXPECT_EQ(F(0), empty.bias()); EXPECT_EQ(F(0), empty.alpha()); EXPECT_EQ(F(0), empty.beta());
  for (char const* key : {"seed", "octaves", "frequency", "amplitude", "bias", "alpha", "beta", "offset", "gain"})
    EXPECT_EQ(0.0, empty.toJson().getDouble(key));
  EXPECT_THROW(empty.get(1), PerlinException);
  EXPECT_THROW(empty.get(1, 2), PerlinException);
  EXPECT_THROW(empty.get(1, 2, 3), PerlinException);
}

template <typename F> void assignEmpty() {
  Perlin<F> empty;
  auto target = noise<F>();
  target = empty;
  EXPECT_EQ(empty.toJson(), target.toJson());
  EXPECT_EQ(0u, target.allocatedStorageBytes());
  EXPECT_THROW(target.get(1, 2), PerlinException);
}

template <typename F> void copyEmpty() {
  Perlin<F> empty, copied(empty);
  EXPECT_EQ(empty.toJson(), copied.toJson());
  EXPECT_EQ(0u, copied.allocatedStorageBytes());
  EXPECT_THROW(copied.get(1), PerlinException);
}

template <typename F> void explicitEmpty() {
  // Json construction initializes all metadata even for an unqueryable type.
  Perlin<F> source(noise<F>().toJson().set("type", "uninitialized").set("seed", 91));
  auto target = noise<F>(PerlinType::Billow);
  target = source;
  EXPECT_EQ(source.toJson(), target.toJson());
  Perlin<F> copied(source);
  EXPECT_EQ(source.toJson(), copied.toJson());
  EXPECT_EQ(0u, target.allocatedStorageBytes());
  EXPECT_EQ(0u, copied.allocatedStorageBytes());
  EXPECT_THROW(target.get(1), PerlinException);
}

template <typename F> void containers() {
  Perlin<F> empty;
  std::vector<Perlin<F>> values(3, empty);
  values.reserve(12);
  for (auto& value : values) {
    value = value;
    Perlin<F> moved(std::move(value));
    EXPECT_EQ(empty.toJson(), moved.toJson());
    EXPECT_EQ(0u, moved.allocatedStorageBytes());
    EXPECT_THROW(moved.get(1), PerlinException);
  }
}

template <typename F> void initialized() {
  for (auto type : {PerlinType::Perlin, PerlinType::Billow, PerlinType::RidgedMulti}) {
    auto source = noise<F>(type);
    Perlin<F> copied(source), assigned;
    assigned = source;
    samplesEqual(source, copied); samplesEqual(source, assigned);
    Perlin<F> moved(std::move(copied)), moveAssigned;
    moveAssigned = std::move(assigned);
    samplesEqual(source, moved); samplesEqual(source, moveAssigned);
    EXPECT_EQ(0u, copied.allocatedStorageBytes());
    EXPECT_EQ(0u, assigned.allocatedStorageBytes());
    // Moved-from initialized noises are not sampled; their legacy metadata
    // remains initialized, but they no longer own the lookup arrays.
  }
}

template <typename F> void selfAssignment() {
  auto source = noise<F>();
  auto reference = source;
  auto before = source.allocatedStorageBytes();
  source = source;
  EXPECT_EQ(before, source.allocatedStorageBytes());
  samplesEqual(reference, source);
}
}

TEST(PerlinLifecycleTest, DefaultMetadataIsAlreadyFullyInitialized) { defaultMetadata<float>(); defaultMetadata<double>(); }
TEST(PerlinLifecycleTest, AssigningEmptyReplacesPreviouslyInitializedMetadata) { assignEmpty<float>(); assignEmpty<double>(); }
TEST(PerlinLifecycleTest, CopyingEmptyInitializesAllMetadata) { copyEmpty<float>(); copyEmpty<double>(); }
TEST(PerlinLifecycleTest, ExplicitUninitializedJsonMetadataSurvivesCopy) { explicitEmpty<float>(); explicitEmpty<double>(); }
TEST(PerlinLifecycleTest, EmptyCopiesSurviveContainerGrowthMoveAndSelfAssignment) { containers<float>(); containers<double>(); }
TEST(PerlinLifecycleTest, InitializedCopyMoveAndAssignmentKeepSampleBits) { initialized<float>(); initialized<double>(); }
TEST(PerlinLifecycleTest, InitializedSelfAssignmentKeepsSampleBits) { selfAssignment<float>(); selfAssignment<double>(); }
TEST(PerlinLifecycleTest, ExplicitNoiseConstructionAndCopiesDoNotConsumeGlobalRng) {
  Random::init(924);
  auto first = Random::randu64(), second = Random::randu64();
  Random::init(924);
  EXPECT_EQ(first, Random::randu64());
  auto source = noise<float>();
  auto copied = source;
  auto moved = std::move(copied);
  (void)moved;
  EXPECT_EQ(second, Random::randu64());
}
