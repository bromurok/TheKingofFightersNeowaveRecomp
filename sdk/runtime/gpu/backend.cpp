#include "gpu/backend.h"
#include "app/app.h"
#include "core/log.h"

namespace gpu {

std::unique_ptr<Backend> CreateBackend() {
    LOGR("gpu: using Direct3D 12 backend");
    return CreateD3D12Backend();
}

}  // namespace gpu
