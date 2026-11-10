INCLUDE_DIRECTORIES (
    ${STAR_EXTERN_INCLUDES}
    ${STAR_CORE_INCLUDES}
    ${STAR_BASE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES}
    ${STAR_GAME_INCLUDES}
  )

# Add top-level test directory so  gtest-all.cc can find its own include file
INCLUDE_DIRECTORIES (.)

SET (star_core_tests_SOURCES
      gtest/gtest-all.cc

      core_tests_main.cpp

      algorithm_test.cpp
      block_allocator_test.cpp
      blocks_along_line_test.cpp
      btree_database_test.cpp
      btree_test.cpp
      byte_array_test.cpp
      clock_test.cpp
      color_test.cpp
      container_test.cpp
      encode_test.cpp
      file_test.cpp
      hash_test.cpp
      host_address_test.cpp
      ref_ptr_test.cpp
      json_test.cpp
      flat_hash_test.cpp
      formatted_json_test.cpp
      line_test.cpp
      lua_test.cpp
      lua_json_test.cpp
      math_test.cpp
      multi_table_test.cpp
      net_states_test.cpp
      ordered_map_test.cpp
      ordered_set_test.cpp
      periodic_test.cpp
      poly_test.cpp
      random_test.cpp
      random_seed_test.cpp
      rect_test.cpp
      serialization_test.cpp
      static_vector_test.cpp
      small_vector_test.cpp
      sha_test.cpp
      shell_parse.cpp
      string_test.cpp
      strong_typedef_test.cpp
      thread_test.cpp
      worker_pool_test.cpp
      variant_test.cpp
      vlq_test.cpp
    )
ADD_EXECUTABLE (core_tests
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core>
  ${star_core_tests_SOURCES})
IF(STAR_ENABLE_TRACY)
  TARGET_LINK_LIBRARIES(core_tests PRIVATE ${STAR_EXT_LIBS} Tracy::TracyClient)
ELSE()
  TARGET_LINK_LIBRARIES(core_tests PRIVATE ${STAR_EXT_LIBS})
ENDIF()
ADD_TEST (NAME core_tests WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/core_tests)

SET (star_game_tests_SOURCES
      gtest/gtest-all.cc

      game_tests_main.cpp

      StarTestUniverse.cpp
      assets_test.cpp
      function_test.cpp
      item_test.cpp
      root_test.cpp
      server_test.cpp
      spawn_test.cpp
      stat_test.cpp
      tile_array_test.cpp
      world_geometry_test.cpp
      universe_connection_test.cpp
      weather_domain_test.cpp
      weather_test.cpp
    )
ADD_EXECUTABLE (game_tests
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_base> $<TARGET_OBJECTS:star_game>
  ${star_game_tests_SOURCES})
IF(STAR_ENABLE_TRACY)
  TARGET_LINK_LIBRARIES(game_tests PRIVATE ${STAR_EXT_LIBS} Tracy::TracyClient)
ELSE()
  TARGET_LINK_LIBRARIES(game_tests PRIVATE ${STAR_EXT_LIBS})
ENDIF()
ADD_TEST (NAME game_tests WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/game_tests)

SET_TESTS_PROPERTIES(core_tests PROPERTIES
                     LABELS "NoAssets")

# Ownership and geometry contracts must be runnable without game assets or a
# mutable game Root/World. The Assets lifecycle suite supplies a configuration-
# only RootBase host and temporary first-party files, not installed game assets.
ADD_EXECUTABLE(world_generation_contract_tests
  $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_base>
  ../game/StarWorldRegionPartition.cpp
  ../game/StarWorldGenerationDependency.cpp
  ../game/StarWorldGenerationTasks.cpp
  ../game/StarWorldGenerationMemory.cpp
  ../game/StarLiquidTypes.cpp
  ../game/StarWorldGenerationBaseTiles.cpp
  ../game/StarWorldGenerationCustomTerrain.cpp
  ../game/StarTerrainSelector.cpp
  ../game/StarWorldGenerationBlockInfoStaticInputs.cpp
  ../game/StarWorldGenerationLookupReaders.cpp
  ../game/StarWorldGenerationLookupDefaults.cpp
  ../game/StarTileDamageState.cpp
  ../game/StarWorldTiles.cpp
  ../game/StarWorldGenerationMemoryEnvelope.cpp
  ../game/StarWorldGenerationMemoryEstimate.cpp
  ../game/StarBiomePlacementSampling.cpp
  ../game/StarBiomePlacementDefaults.cpp
  ../game/StarWorldRegionCoordinator.cpp
  gtest/gtest-all.cc
  core_tests_main.cpp
  world_region_partition_test.cpp
  world_generation_dependency_test.cpp
  world_generation_pass_test.cpp
  StarWorldGenerationPassHarness.hpp
  world_generation_placement_queue_test.cpp
  world_generation_tasks_test.cpp
  world_generation_memory_test.cpp
  world_generation_memory_envelope_test.cpp
  world_generation_memory_admission_test.cpp
  world_generation_memory_batch_test.cpp
  world_generation_memory_retry_test.cpp
  world_generation_memory_capacity_test.cpp
  world_generation_base_tile_liquid_test.cpp
  world_generation_base_tiles_test.cpp
  world_generation_block_info_test.cpp
  world_generation_custom_terrain_test.cpp
  world_generation_lookup_sources_test.cpp
  world_generation_lookup_readers_test.cpp
  byte_array_test.cpp
  random_seed_test.cpp
  perlin_lifecycle_test.cpp
  world_generation_selector_snapshot_test.cpp
  world_generation_estimate_test.cpp
  world_generation_worker_cache_test.cpp
  world_generation_buffer_test.cpp
  world_generation_distribution_test.cpp
  world_generation_planning_storage_test.cpp
  world_generation_payload_test.cpp
  world_generation_temporary_test.cpp
  world_generation_container_storage_test.cpp
  world_generation_json_storage_test.cpp
  block_allocator_exception_test.cpp
  world_region_coordinator_test.cpp)
IF(STAR_ENABLE_TRACY)
  TARGET_LINK_LIBRARIES(world_generation_contract_tests PRIVATE ${STAR_EXT_LIBS} Tracy::TracyClient)
  TARGET_COMPILE_DEFINITIONS(world_generation_contract_tests PRIVATE STAR_ENABLE_TRACY)
ELSE()
  TARGET_LINK_LIBRARIES(world_generation_contract_tests PRIVATE ${STAR_EXT_LIBS})
ENDIF()
# Keep the value-only world-region checks independently diagnosable in CI. A
# Tracy client issue in one executable must identify its GTest suite and must
# not consume the entire Actions job timeout.
FOREACH(world_region_test_suite
    WorldRegionPartitionTest
    WorldGenerationDependencyTest
    WorldGenerationPassTest
    WorldGenerationPlacementQueueTest
    WorldGenerationTasksTest
    WorldGenerationMemoryTest
    WorldGenerationMemoryEnvelopeTest
    WorldGenerationMemoryAdmissionTest
    WorldGenerationMemoryBatchTest
    WorldGenerationMemoryRetryTest
    WorldGenerationMemoryCapacityTest
    WorldGenerationBaseTileLiquidTest
    WorldGenerationBaseTilesTest
    WorldGenerationBlockInfoTest
    WorldGenerationCustomTerrainTest
    WorldGenerationBlockInfoStaticInputsTest
    WorldGenerationBaseTileInputsTest
    WorldGenerationBaseTileObservationTest
    WorldGenerationBaseTileLookupContextTest
    WorldGenerationSourceVersionTest
    WorldGenerationLookupSourcesTest
    WorldGenerationLookupReadersTest
    WorldGenerationAssetsLifecycleTest
    ByteArrayTest
    AutomaticRandomSeedTest
    PerlinLifecycleTest
    WorldGenerationSelectorSnapshotTest
    WorldGenerationEstimateTest
    WorldGenerationWorkerCacheTest
    WorldGenerationBufferTest
    WorldGenerationDistributionTest
    WorldGenerationPlanningStorageTest
    WorldGenerationPayloadTest
    WorldGenerationTemporaryTest
    WorldGenerationContainerStorageTest
    WorldGenerationJsonStorageTest
    WorldGenerationVariantJsonTest
    BlockAllocatorExceptionTest
    WorldRegionCoordinatorTest
    WorldRegionObservationTest)
  ADD_TEST(NAME world_generation_contract_${world_region_test_suite}
    COMMAND $<TARGET_FILE:world_generation_contract_tests>
      --gtest_filter=${world_region_test_suite}.*)
  SET_TESTS_PROPERTIES(world_generation_contract_${world_region_test_suite}
    PROPERTIES LABELS "NoAssets" TIMEOUT 120)
ENDFOREACH()

# Full Root lifecycle, deliberately opt-in: uses real game methods plus minimal
# first-party fixture files, not an installed Starbound asset package. Section
# GC drops unrelated services; it does not substitute symbols or sanitizers.
OPTION(STAR_BUILD_ROOT_LIFECYCLE_TESTS "Build isolated real-Root lifecycle tests" OFF)
IF(STAR_BUILD_ROOT_LIFECYCLE_TESTS)
  ADD_EXECUTABLE(world_generation_root_lifecycle_tests
    $<TARGET_OBJECTS:star_extern> $<TARGET_OBJECTS:star_core> $<TARGET_OBJECTS:star_base>
    ../game/StarRoot.cpp
    ../game/StarWorldTemplateSampling.cpp
    ../game/StarWorldLayoutSampling.cpp
    ../game/StarWorldLayoutRegion.cpp
    ../game/StarBiomeDatabase.cpp
    ../game/StarStoredFunctions.cpp
    ../game/StarAmbient.cpp
    ../game/StarSpawnTypeDatabase.cpp
    ../game/StarBiomePlacement.cpp
    ../game/StarBiomePlacementSampling.cpp
    ../game/StarBiome.cpp
    ../game/StarParallax.cpp
    ../game/StarPlantDatabase.cpp
    ../game/StarTerrainSelector.cpp
    ../game/StarTerrainDatabase.cpp
    ../game/terrain/StarCacheSelector.cpp
    ../game/terrain/StarDisplacementSelector.cpp
    ../game/terrain/StarIslandSurfaceSelector.cpp
    ../game/terrain/StarKarstCave.cpp
    ../game/terrain/StarMaxSelector.cpp
    ../game/terrain/StarMinMaxSelector.cpp
    ../game/terrain/StarMixSelector.cpp
    ../game/terrain/StarPerlinSelector.cpp
    ../game/terrain/StarRidgeBlocksSelector.cpp
    ../game/terrain/StarRotateSelector.cpp
    ../game/terrain/StarWormCave.cpp
    ../game/terrain/StarConstantSelector.cpp
    ../game/terrain/StarFlatSurfaceSelector.cpp
    ../game/StarWorldGenerationCustomTerrain.cpp
    ../game/StarWorldGenerationBlockInfoStaticInputs.cpp
    ../game/StarWorldGenerationMemory.cpp
    ../game/StarWorldGenerationMemoryEnvelope.cpp
    ../game/StarWorldGenerationMemoryEstimate.cpp
    ../game/StarWorldGenerationBaseTiles.cpp
    ../game/StarWorldTiles.cpp
    ../game/StarLiquidTypes.cpp
    ../game/StarDatabaseMaintenance.cpp
    ../game/StarItemDescriptor.cpp
    ../game/StarMaterialDatabase.cpp
    ../game/StarWorldGenerationLookupReaders.cpp
    ../game/StarWorldGenerationLookupDefaults.cpp
    ../game/StarBiomePlacementDefaults.cpp
    ../game/StarParticleDatabase.cpp
    ../game/StarParticle.cpp
    ../game/StarAnimation.cpp
    ../game/StarMaterialRenderProfile.cpp
    ../game/StarTileDamage.cpp
    ../game/StarTileDamageState.cpp
    ../game/StarImageMetadataDatabase.cpp
    ../game/StarCollisionBlock.cpp
    ../game/StarGameTypes.cpp
    gtest/gtest-all.cc
    core_tests_main.cpp
    StarWorldGenerationTestDirectory.hpp
    world_generation_root_lifecycle_test.cpp
    world_generation_real_block_info_test.cpp)
  IF(MSVC)
    TARGET_COMPILE_OPTIONS(world_generation_root_lifecycle_tests PRIVATE /Gy /Gw)
    TARGET_LINK_OPTIONS(world_generation_root_lifecycle_tests PRIVATE /OPT:REF)
  ELSE()
    TARGET_COMPILE_OPTIONS(world_generation_root_lifecycle_tests PRIVATE -ffunction-sections -fdata-sections)
    IF(APPLE)
      TARGET_LINK_OPTIONS(world_generation_root_lifecycle_tests PRIVATE -Wl,-dead_strip)
    ELSE()
      # The project's -rdynamic otherwise keeps every unrelated game method
      # alive. Keep DWARF/sanitizers; disable only the test's blanket export.
      TARGET_LINK_OPTIONS(world_generation_root_lifecycle_tests PRIVATE -Wl,--no-export-dynamic -Wl,--gc-sections)
    ENDIF()
  ENDIF()
  IF(STAR_ENABLE_TRACY)
    TARGET_LINK_LIBRARIES(world_generation_root_lifecycle_tests PRIVATE ${STAR_EXT_LIBS} Tracy::TracyClient)
    TARGET_COMPILE_DEFINITIONS(world_generation_root_lifecycle_tests PRIVATE STAR_ENABLE_TRACY)
  ELSE()
    TARGET_LINK_LIBRARIES(world_generation_root_lifecycle_tests PRIVATE ${STAR_EXT_LIBS})
  ENDIF()
  ADD_TEST(NAME world_generation_root_lifecycle
    COMMAND $<TARGET_FILE:world_generation_root_lifecycle_tests> --gtest_filter=WorldGenerationRootLifecycleTest.*)
  SET_TESTS_PROPERTIES(world_generation_root_lifecycle PROPERTIES LABELS "NoAssets;RootLifecycle" TIMEOUT 120)
  ADD_TEST(NAME world_generation_real_block_info
    COMMAND $<TARGET_FILE:world_generation_root_lifecycle_tests> --gtest_filter=WorldGenerationRealBlockInfoTest.*)
  SET_TESTS_PROPERTIES(world_generation_real_block_info PROPERTIES LABELS "NoAssets;RealBlockInfo" TIMEOUT 120)
ENDIF()
