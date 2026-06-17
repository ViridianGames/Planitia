#include "Unit.h"

Unit::~Unit() { Shutdown(); }

void Unit::Init(const std::string& configfile)
{
	m_UnitConfig.Load(configfile);

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