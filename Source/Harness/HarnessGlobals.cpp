// The harness does not start the engine. Terrain texture loads still mention this symbol.
#include "Geist/Config.h"
#include "Geist/ResourceManager.h"

#include <memory>

std::unique_ptr<ResourceManager> g_ResourceManager;
