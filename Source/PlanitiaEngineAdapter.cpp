#include "PlanitiaEngineAdapter.h"
#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"

void PlanitiaEngineAdapter::Init(const std::string& configfile)
{
    LoadConfigFile(m_EngineConfig, configfile);
    if (m_EngineConfig.count("milliseconds_between_updates"))
        m_NumberOfMillisecondsBetweenUpdates = static_cast<UINT>(m_EngineConfig["milliseconds_between_updates"].numdata);
}

void PlanitiaEngineAdapter::Shutdown() {}
void PlanitiaEngineAdapter::Draw() {}

void PlanitiaEngineAdapter::SyncTiming()
{
    m_GameTimeInMS = static_cast<DWORD>(g_Engine->GameTimeInMS());
    m_GameTimeInSeconds = static_cast<float>(m_GameTimeInMS) / 1000.0f;
    m_DurationOfLastUpdateInMS = static_cast<DWORD>(g_Engine->m_lastUpdateInMS);
    m_DurationOfLastUpdateInSeconds = static_cast<float>(m_DurationOfLastUpdateInMS) / 1000.0f;
    m_Done = g_Engine->m_Done;
}

void PlanitiaEngineAdapter::Update()
{
    SyncTiming();
    ++m_GameFrames;

    m_UpdateTimer += m_DurationOfLastUpdateInMS;
    m_NumberOfUpdatesToDoThisFrame = m_UpdateTimer / m_NumberOfMillisecondsBetweenUpdates;
    m_UpdateTimer = m_UpdateTimer % m_NumberOfMillisecondsBetweenUpdates;
    m_GameUpdates += m_NumberOfUpdatesToDoThisFrame;
}