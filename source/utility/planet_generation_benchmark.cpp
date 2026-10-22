#include "StarLexicalCast.hpp"
#include "StarLogging.hpp"
#include "StarRootLoader.hpp"
#include "StarWorldServer.hpp"
#include "StarWorldTemplate.hpp"

using namespace Star;

int main(int argc, char** argv) {
  try {
    RootLoader rootLoader({{}, {}, {}, LogLevel::Error, false, {}});
    rootLoader.addParameter("type", "planet type", OptionParser::Optional, "planet type, defaults to garden");
    rootLoader.addParameter("size", "planet size", OptionParser::Optional, "planet size, defaults to small");
    rootLoader.addParameter("seed", "seed", OptionParser::Optional, "world seed, defaults to 123456789");
    rootLoader.addParameter("sectors", "sectors", OptionParser::Optional, "number of surface sectors to generate, defaults to 32");

    RootUPtr root;
    OptionParser::Options options;
    tie(root, options) = rootLoader.commandInitOrDie(argc, argv);

    coutf("Fully loading root...");
    root->fullyLoad();
    coutf(" done\n");

    String planetType = "garden";
    if (auto parameter = options.parameters.maybe("type"))
      planetType = parameter->first();

    String planetSize = "small";
    if (auto parameter = options.parameters.maybe("size"))
      planetSize = parameter->first();

    uint64_t worldSeed = 123456789;
    if (auto parameter = options.parameters.maybe("seed"))
      worldSeed = lexicalCast<uint64_t>(parameter->first());

    unsigned requestedSectors = 32;
    if (auto parameter = options.parameters.maybe("sectors"))
      requestedSectors = max(1u, lexicalCast<unsigned>(parameter->first()));

    auto worldParameters = generateTerrestrialWorldParameters(planetType, planetSize, worldSeed);
    auto worldTemplate = make_shared<WorldTemplate>(worldParameters, SkyParameters(), worldSeed);
    WorldServer worldServer(worldTemplate, File::ephemeralFile());
    worldServer.setUpdateProfilingEnabled(true);

    Vec2U worldSize = worldServer.geometry().size();
    unsigned horizontalSectorCount = (worldSize[0] + WorldSectorSize - 1) / WorldSectorSize;
    unsigned verticalSectorCount = (worldSize[1] + WorldSectorSize - 1) / WorldSectorSize;
    unsigned sectorCount = min(requestedSectors, max(1u, horizontalSectorCount / 2));
    unsigned surfaceTileY = (unsigned)max(0.0f, worldTemplate->surfaceLevel());
    unsigned surfaceSectorY = min(surfaceTileY / (unsigned)WorldSectorSize, verticalSectorCount - 1);

    Vec2I spawnPosition = worldTemplate->findSensiblePlayerStart().value(Vec2I((int)worldSize[0] / 2, (int)worldTemplate->surfaceLevel()));
    unsigned spawnSectorX = (unsigned)pmod<int>(spawnPosition[0], (int)worldSize[0]) / WorldSectorSize;
    unsigned firstSectorX = (spawnSectorX + max(1u, horizontalSectorCount / 3)) % horizontalSectorCount;

    coutf("Generating {} terrestrial surface sectors on {}:{} with seed {}\n", sectorCount, planetType, planetSize, worldSeed);
    double start = Time::monotonicTime();
    for (unsigned i = 0; i < sectorCount; ++i) {
      unsigned sectorX = (firstSectorX + i) % horizontalSectorCount;
      RectI sectorRegion = RectI::withSize(Vec2I(sectorX * WorldSectorSize, surfaceSectorY * WorldSectorSize), Vec2I(WorldSectorSize, WorldSectorSize));
      worldServer.generateRegion(sectorRegion);
    }
    double elapsed = Time::monotonicTime() - start;

    coutf("Generated {} surface sectors in {} seconds ({} ms per requested sector)\n", sectorCount, elapsed, elapsed * 1000.0 / sectorCount);
    coutf("Generation stage timings (milliseconds): {}\n", worldServer.updateProfilingData().get("worldGenerationStages").repr(1));
    return 0;
  } catch (std::exception const& e) {
    cerrf("Exception caught: {}\n", outputException(e, true));
    return 1;
  }
}
