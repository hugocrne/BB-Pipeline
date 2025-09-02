// EN: Pipeline Execution Context implementation - Stub implementation for compilation
// FR: Implémentation Pipeline Execution Context - Implémentation stub pour compilation

#include "orchestrator/pipeline_engine.hpp"

namespace BBP {
namespace Orchestrator {

#define UNUSED(x) ((void)(x))

// EN: PipelineExecutionContext implementation  
// FR: Implémentation PipelineExecutionContext
PipelineExecutionContext::PipelineExecutionContext(const std::string& pipeline_id, const PipelineExecutionConfig& config) 
    : pipeline_id_(pipeline_id), config_(config) {}

bool PipelineExecutionContext::isCancelled() const { return false; }
bool PipelineExecutionContext::shouldContinue() const { return true; }
void PipelineExecutionContext::setEventCallback(std::function<void(const PipelineEvent&)> callback) { UNUSED(callback); }
void PipelineExecutionContext::updateStageResult(const std::string& stage_id, const PipelineStageResult& result) { UNUSED(stage_id); UNUSED(result); }
void PipelineExecutionContext::notifyStageStarted(const std::string& stage_id) { UNUSED(stage_id); }
void PipelineExecutionContext::notifyStageCompleted(const std::string& stage_id, const PipelineStageResult& result) { UNUSED(stage_id); UNUSED(result); }
void PipelineExecutionContext::requestCancellation() {}
std::optional<PipelineStageResult> PipelineExecutionContext::getStageResult(const std::string& stage_id) const { UNUSED(stage_id); return std::nullopt; }
std::vector<PipelineStageResult> PipelineExecutionContext::getAllStageResults() const { return {}; }
PipelineProgress PipelineExecutionContext::getCurrentProgress() const { return PipelineProgress{}; }

} // namespace Orchestrator
} // namespace BBP