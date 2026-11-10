#include "StarWorldTemplateTypes.hpp"

namespace Star {

WorldGenerationBlockInfoStaticInputs WorldTemplate::captureBlockInfoStaticInputs(
    WorldGenerationMemoryLedger& ledger, bool measureJson, size_t maximumJsonValues) const {
  // Structural measurement only, never config validation/conversion. Refused
  // or incomplete measurement MUST NOT discard or filter generation inputs.
  auto configuration = captureWorldGenerationJson(ledger, m_templateConfig, measureJson, maximumJsonValues);
  return {std::move(configuration), captureCustomTerrain()};
}

}
