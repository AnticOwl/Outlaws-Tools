#include "PostProcessSystem.h"
namespace outlaws {
bool PostProcessSystem::read(PostProcessState&) const { return false; }
bool PostProcessSystem::apply(const PostProcessState&) { return false; }
}
