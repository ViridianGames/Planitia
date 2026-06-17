#pragma warning(disable:4786)

#ifndef _UNIT_H_
#define _UNIT_H_

#include "PlanitiaObject.h"
#include "PlanitiaConfig.h"

#include <map>
#include <string>

class Unit : public PlanitiaObject
{
public:
   Unit() = default;
   virtual ~Unit();

   virtual void Init(const std::string& configfile) override;
   virtual void Shutdown() override;
   virtual void Update() override;
   virtual void Draw() override;

   virtual bool IsDead() { return m_IsDead; }

   bool m_IsDead;

   std::map<std::string, PlanitiaConfigInfo> m_UnitConfig;
};

#endif