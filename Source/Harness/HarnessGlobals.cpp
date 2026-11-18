// The harness does not start the engine. Globals.cpp is not linked here because
// it also defines g_ResourceManager. Primitives.cpp still references g_Engine,
// so this translation unit owns a null engine pointer.
#include "Geist/Config.h"
#include "Geist/Engine.h"
#include "Geist/ResourceManager.h"

#include <memory>

std::unique_ptr<Engine> g_Engine;
std::unique_ptr<ResourceManager> g_ResourceManager;
