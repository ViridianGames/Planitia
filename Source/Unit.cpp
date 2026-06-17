#include "Unit.h"
#include "PlanitiaConfig.h"

Unit::~Unit() { Shutdown(); }

void Unit::Init(const std::string& configfile)
{
	LoadConfigFile(m_UnitConfig, configfile);

	m_IsDead = false;
}

void Unit::Update()
{

}

void Unit::Draw()
{

}

void Unit::Shutdown()
{
      
}

