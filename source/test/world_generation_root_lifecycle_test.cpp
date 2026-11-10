#include "StarRoot.hpp"
#include "StarWorldGenerationTestDirectory.hpp"
#include "StarWorldGenerationLookupSources.hpp"
#include "StarMaterialDatabase.hpp"

#include <stdexcept>

#include "gtest/gtest.h"

using namespace Star;

namespace {
class WorldGenerationRootLifecycleTest : public testing::Test {
protected:
  Star::Test::GenerationTestDirectory source;
  Star::Test::GenerationTestDirectory storage;
  Star::Test::GenerationTestDirectory mods;

  void SetUp() override {
    ASSERT_EQ(RootBase::singletonPtr(), nullptr);
    source.write("preload.config", "[]");
    source.write("_metadata", "{\"name\":\"root-lifecycle-fixture\"}");
    source.write("probe.config", "{\"marker\":1}");
    source.write("metamaterials.config", "[]");
    source.write("client.config", "{\"defaultFootstepSound\":\"\"}");
    writeMaterial(true);
  }
  void TearDown() override { EXPECT_EQ(RootBase::singletonPtr(), nullptr); }
  void writeMaterial(bool supportsMods) {
    source.write("probe.material", material(supportsMods).repr());
  }
  static Json material(bool supportsMods) {
    return JsonObject{{"materialId", 21}, {"materialName", "fixturematerial"}, {"category", "fixture"},
        {"supportsMods", supportsMods}, {"damageTable", JsonObject{{"damageFactors", JsonObject{{"blockish", 1}}}, {"damageRecovery", 1}}}};
  }
  void prepareMod() {
    auto overridePath = File::relativeTo(mods.path(), "override");
    File::makeDirectory(overridePath);
    File::writeFile(String("{\"name\":\"root-fixture-override\",\"priority\":10}"), File::relativeTo(overridePath, "_metadata"));
    File::writeFile(String("{\"marker\":9}"), File::relativeTo(overridePath, "probe.config"));
    File::writeFile(material(false).repr(), File::relativeTo(overridePath, "probe.material"));
  }
  Root::Settings settings(unsigned workers = 0) const {
    Root::Settings result{};
    result.assetsSettings.assetTimeToLive = 60.0f;
    result.assetsSettings.audioDecompressLimit = 0.0f;
    result.assetsSettings.workerPoolSize = workers;
    result.assetSources = {source.path()};
    result.storageDirectory = storage.path();
    result.defaultConfiguration = JsonObject{{"configurationVersion", 1}, {"safe", JsonObject{{"allowRemoteAssets", false}}}};
    result.logLevel = LogLevel::Error;
    result.quiet = false;
    result.includeUGC = false;
    return result;
  }
};
}

TEST_F(WorldGenerationRootLifecycleTest, NormalConstructionLoadsHotReloadsReplacesAssetsAndClearsSingleton) {
  auto root = std::make_unique<Root>(settings(2));
  EXPECT_EQ(Root::singletonPtr(), root.get());
  auto first = root->assets(); auto version = first->generationRevision();
  EXPECT_EQ(first->json("/probe.config").getUInt("marker"), 1u);
  root->hotReload(); EXPECT_EQ(root->assets(), first); EXPECT_NE(first->generationRevision(), version);
  root->reload(); auto second = root->assets();
  EXPECT_NE(first, second); EXPECT_NE(first->generationRevision(), second->generationRevision());
  EXPECT_EQ(second->json("/probe.config").getUInt("marker"), 1u);
  root.reset(); EXPECT_EQ(RootBase::singletonPtr(), nullptr);
}

TEST_F(WorldGenerationRootLifecycleTest, FactoryLoadsRealMaterialDefinitionsAndCapturesTheActualSourceVersions) {
  Root root(settings()); auto sources = root.worldGenerationLookupSources(); ASSERT_TRUE(sources.valid());
  auto scope = sources.scope(1, 0); ASSERT_TRUE(scope);
  EXPECT_NE(scope->sourceEpoch, 0u); EXPECT_EQ(scope->assetsGeneration, root.assets()->generationRevision());
  EXPECT_EQ(scope->materialsGeneration, root.materialDatabase()->generationRevision());
  EXPECT_EQ(sources.assets(), root.assets()); EXPECT_EQ(sources.materials(), root.materialDatabase());
  EXPECT_EQ(sources.materials()->materialId("fixturematerial"), 21u);
  EXPECT_TRUE(sources.materials()->supportsMod(21, 31));
}

TEST_F(WorldGenerationRootLifecycleTest, RootHotReloadInvalidatesSourcesButDoesNotReconstructTheMaterialBank) {
  Root root(settings()); auto before = root.worldGenerationLookupSources(); auto oldScope = before.scope(1, 0); ASSERT_TRUE(oldScope);
  writeMaterial(false); root.hotReload(); auto after = root.worldGenerationLookupSources(); ASSERT_TRUE(after.valid());
  auto newScope = after.scope(1, 0); ASSERT_TRUE(newScope);
  EXPECT_FALSE(before.valid()); EXPECT_EQ(after.materials(), before.materials()); EXPECT_EQ(after.assets(), before.assets());
  EXPECT_TRUE(after.materials()->supportsMod(21, 31));
  EXPECT_FALSE(root.assets()->json("/probe.material").getBool("supportsMods"));
  EXPECT_NE(newScope->sourceEpoch, oldScope->sourceEpoch); EXPECT_NE(newScope->assetsGeneration, oldScope->assetsGeneration);
  EXPECT_EQ(newScope->materialsGeneration, oldScope->materialsGeneration);
}

TEST_F(WorldGenerationRootLifecycleTest, DirectAssetsHotReloadDoesNotAdvanceTheParentEpochOrReconstructDefinitions) {
  Root root(settings()); auto before = root.worldGenerationLookupSources(); auto oldScope = before.scope(1, 0); ASSERT_TRUE(oldScope);
  root.assets()->hotReload(); auto after = root.worldGenerationLookupSources(); auto newScope = after.scope(1, 0); ASSERT_TRUE(newScope);
  EXPECT_FALSE(before.valid()); EXPECT_TRUE(after.valid()); EXPECT_EQ(after.materials(), before.materials());
  EXPECT_EQ(newScope->sourceEpoch, oldScope->sourceEpoch); EXPECT_NE(newScope->assetsGeneration, oldScope->assetsGeneration);
}

TEST_F(WorldGenerationRootLifecycleTest, ReloadRebuildsTheBankButKeepsOldDefinitionsAliveUntilLastHandle) {
  Root root(settings()); auto before = root.worldGenerationLookupSources(); std::weak_ptr<MaterialDatabase const> oldBank = before.materials();
  auto oldScope = before.scope(1, 0); ASSERT_TRUE(oldScope);
  writeMaterial(false); root.reload(); auto after = root.worldGenerationLookupSources(); auto newScope = after.scope(1, 0); ASSERT_TRUE(newScope);
  EXPECT_FALSE(before.valid()); EXPECT_TRUE(after.valid()); EXPECT_FALSE(oldBank.expired());
  EXPECT_NE(after.assets(), before.assets()); EXPECT_NE(after.materials(), before.materials());
  EXPECT_TRUE(before.materials()->supportsMod(21, 31)); EXPECT_FALSE(after.materials()->supportsMod(21, 31));
  EXPECT_NE(newScope->sourceEpoch, oldScope->sourceEpoch); EXPECT_NE(newScope->materialsGeneration, oldScope->materialsGeneration);
  before = {}; EXPECT_TRUE(oldBank.expired());
}

TEST_F(WorldGenerationRootLifecycleTest, ListenerCannotCaptureAValidFactoryBindingDuringHotReload) {
  Root root(settings()); auto before = root.worldGenerationLookupSources(); unsigned calls = 0;
  auto listener = std::make_shared<CallbackListener>([&] {
    ++calls; EXPECT_FALSE(before.valid()); EXPECT_FALSE(root.worldGenerationLookupSources().valid());
  });
  root.registerReloadListener(listener); root.hotReload();
  EXPECT_EQ(calls, 1u); EXPECT_TRUE(root.worldGenerationLookupSources().valid());
}

TEST_F(WorldGenerationRootLifecycleTest, FailedHotReloadListenerLeavesTheFactoryUnavailableUntilSuccessfulRecovery) {
  Root root(settings()); auto before = root.worldGenerationLookupSources();
  auto listener = std::make_shared<CallbackListener>([] { throw std::runtime_error("intentional hot reload fixture failure"); });
  root.registerReloadListener(listener);
  EXPECT_THROW(root.hotReload(), std::runtime_error);
  EXPECT_FALSE(before.valid()); EXPECT_FALSE(root.worldGenerationLookupSources().valid());
  listener.reset(); root.hotReload(); auto after = root.worldGenerationLookupSources();
  EXPECT_TRUE(after.valid()); EXPECT_EQ(after.materials(), before.materials());
}

TEST_F(WorldGenerationRootLifecycleTest, FailedReloadListenerLeavesOldHandlesRetainedAndCanRecoverWithFreshDefinitions) {
  Root root(settings()); auto before = root.worldGenerationLookupSources();
  auto listener = std::make_shared<CallbackListener>([] { throw std::runtime_error("intentional reload fixture failure"); });
  root.registerReloadListener(listener);
  EXPECT_THROW(root.reload(), std::runtime_error);
  EXPECT_FALSE(before.valid()); EXPECT_FALSE(root.worldGenerationLookupSources().valid());
  EXPECT_TRUE(before.materials()->supportsMod(21, 31));
  listener.reset(); root.reload(); auto after = root.worldGenerationLookupSources();
  EXPECT_TRUE(after.valid()); EXPECT_NE(after.materials(), before.materials());
}

TEST_F(WorldGenerationRootLifecycleTest, LoadModsWithoutReloadChangesTheEpochButDefersApplyingTheNewSource) {
  prepareMod(); Root root(settings()); auto before = root.worldGenerationLookupSources(); auto oldScope = before.scope(1, 0); ASSERT_TRUE(oldScope);
  root.loadMods(StringList{mods.path()}, false); auto deferred = root.worldGenerationLookupSources(); auto newScope = deferred.scope(1, 0); ASSERT_TRUE(newScope);
  EXPECT_FALSE(before.valid()); EXPECT_TRUE(deferred.valid()); EXPECT_NE(newScope->sourceEpoch, oldScope->sourceEpoch);
  EXPECT_EQ(deferred.assets(), before.assets()); EXPECT_EQ(deferred.materials(), before.materials());
  EXPECT_EQ(root.assets()->json("/probe.config").getUInt("marker"), 1u);
  root.reload(); auto applied = root.worldGenerationLookupSources(); ASSERT_TRUE(applied.valid());
  EXPECT_EQ(root.assets()->json("/probe.config").getUInt("marker"), 9u); EXPECT_FALSE(applied.materials()->supportsMod(21, 31));
}

TEST_F(WorldGenerationRootLifecycleTest, LoadModsWithReloadKeepsNestedChangeUnavailableThroughoutTheSingleListenerCall) {
  prepareMod(); Root root(settings()); auto before = root.worldGenerationLookupSources(); unsigned calls = 0;
  auto listener = std::make_shared<CallbackListener>([&] {
    ++calls; EXPECT_FALSE(before.valid()); EXPECT_FALSE(root.worldGenerationLookupSources().valid());
  });
  root.registerReloadListener(listener); root.loadMods(StringList{mods.path()}, true);
  auto after = root.worldGenerationLookupSources(); ASSERT_TRUE(after.valid()); EXPECT_EQ(calls, 1u);
  EXPECT_NE(after.assets(), before.assets()); EXPECT_NE(after.materials(), before.materials());
  EXPECT_EQ(root.assets()->json("/probe.config").getUInt("marker"), 9u); EXPECT_FALSE(after.materials()->supportsMod(21, 31));
}

TEST_F(WorldGenerationRootLifecycleTest, DestructionRetiresBindingsWithoutKeepingRootAliveAndReleasesSourcesAtLastHandle) {
  auto root = std::make_unique<Root>(settings()); auto sources = root->worldGenerationLookupSources(); ASSERT_TRUE(sources.valid());
  std::weak_ptr<Assets const> assets = sources.assets(); std::weak_ptr<MaterialDatabase const> materials = sources.materials();
  root.reset(); EXPECT_EQ(Root::singletonPtr(), nullptr); EXPECT_FALSE(sources.valid());
  EXPECT_FALSE(assets.expired()); EXPECT_FALSE(materials.expired());
  // Metadata-only definitions remain readable, not arbitrary Root-dependent asset callbacks.
  EXPECT_TRUE(sources.materials()->supportsMod(21, 31)); sources = {};
  EXPECT_TRUE(assets.expired()); EXPECT_TRUE(materials.expired());
}

TEST_F(WorldGenerationRootLifecycleTest, ConfigurationChangesPersistAcrossTheRealReloadBoundary) {
  auto options = settings(); options.runtimeConfigFile = String("root-fixture.config");
  Root root(options); root.configuration()->set("fixtureMarker", 17);
  root.reload(); EXPECT_EQ(root.configuration()->get("fixtureMarker").toUInt(), 17u);
  EXPECT_TRUE(File::isFile(File::relativeTo(storage.path(), "root-fixture.config")));
}

TEST_F(WorldGenerationRootLifecycleTest, FailedLazyMaterialLoadingDoesNotReturnAPartialBankAndCanRetryAfterHotReload) {
  source.write("probe.material", "{"); Root root(settings());
  EXPECT_THROW(root.worldGenerationLookupSources(), MaterialException);
  writeMaterial(true); root.hotReload(); auto recovered = root.worldGenerationLookupSources();
  ASSERT_TRUE(recovered.valid()); EXPECT_TRUE(recovered.materials()->supportsMod(21, 31));
}
