// EN: Pipeline Task implementation - Stub implementation for compilation
// FR: Implémentation Pipeline Task - Implémentation stub pour compilation

#include "orchestrator/pipeline_engine.hpp"

namespace BBP {
namespace Orchestrator {

#define UNUSED(x) ((void)(x))

// EN: PipelineTask implementation
// FR: Implémentation PipelineTask
PipelineTask::PipelineTask(const PipelineStageConfig& config, PipelineExecutionContext* context) : config_(config), context_(context) { UNUSED(context); }

void PipelineTask::addDependency(const std::string& dependency) { UNUSED(dependency); }
void PipelineTask::removeDependency(const std::string& dependency) { UNUSED(dependency); }
PipelineStageResult PipelineTask::execute() { return PipelineStageResult{}; }
void PipelineTask::cancel() {}
bool PipelineTask::isCancelled() const { return false; }
bool PipelineTask::areDependenciesMet() const { return true; }
PipelineStageStatus PipelineTask::getStatus() const { return PipelineStageStatus::COMPLETED; }

} // namespace Orchestrator
} // namespace BBP