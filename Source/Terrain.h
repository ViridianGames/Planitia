///////////////////////////////////////////////////////////////////////////
//
// Name:     TERRAIN.H
// Author:   Anthony Salter
// Date:     2/03/05
// Purpose:  This object represents a heightfield terrain.  It derives from
//           Unit so that it can go into the unit list and be drawn.  A
//           terrain can have one vertex buffer or a number depending on
//           how it is created.  A terrain vertex buffer will be a simple
//           triangle list, since each "cell" in the terrain could require
//           its own texture coordinates.  Terrains are initialized using
//           .ini files just like everything else.
//
///////////////////////////////////////////////////////////////////////////

#pragma warning(disable:4786)

#ifndef _TERRAIN_H_
#define _TERRAIN_H_

#include "Unit.h"
#include "PlanitiaPrimitives.h"
#include "PlanitiaD3DDevice.h"

#include <string>

enum TerrainTypes
{
	TT_SAND = 0,
	TT_BEACH,
	TT_GRASS,
	TT_BLESSEDLAND,
	TT_RUINEDLAND,
	TT_LAVA,
	TT_WATER,
	TT_FARMLAND,
	TT_HOUSE,
	TT_FLATLAND,
	TT_SWAMP,

	TT_LASTTERRAINTYPE
};

#define NUMBER_OF_VERTEX_BUFFERS 16

struct TerrainCell
{
	int m_TerrainType;
	DWORD m_AdditionalData;
};

class Background : public Unit
{
public:
   Background() {};
   virtual ~Background();

   virtual void Init(const std::string& configfile) override;
   virtual void Shutdown();
   virtual void Update();
   virtual void Draw();

private:
   Bitmap* m_Background;
   PlanitiaMesh* m_Mesh;

};

class Terrain : public Unit
{
public:

	Terrain(){};
	virtual ~Terrain();

	virtual void Init(const std::string& configfile) override;
	virtual void Shutdown();
	virtual void Update();
	virtual void Draw();

	void InitializeMap(int seed);

	void FindTerrainTypes();
	void SetTerrainType(int x, int y, int type);
	void SetTerrainAdditionalData(int x, int y, DWORD data);
	int GetTerrainType(int x, int y);
	DWORD GetTerrainAdditionalData(int x, int y);
	int FindTerrainType(int i, int j);
	bool IsWaterCell(int i, int j);
	void ScrubTerrainCell(int i, int j); //  Recalculates a cell and all surrounding cells

	void CreateVertexBuffers(bool rebuildall);
	void CreateIndexBuffers();
	void CreateWaterVertexBuffer();
	void CreateHighlightVertexBuffer();
	void CreateWaterIndexBuffer();

	void DrawTerrain();
	void DrawWater();
	void DrawHighlightMarker();

	void UpdateMiniMap();

	virtual float GetValue(int x, int y);
	virtual void  SetValue(int x, int y, float value);
	virtual void  OffsetValue(int x, int y, float value);
	virtual float GetMiddle(int x, int y);

	virtual bool IsValidVillageTerrain(int x, int y);

	

	virtual float GetHeight(float x, float y);  //  Uses LERP

	virtual float Lerp(float a, float b, float t) {	return a - (a*t) + (b*t); }

	virtual void SetUpODE();

	void GetTerrainHit(int &x, int &y);  //  Returns the current XY coord of the mouse on the terrain
    void GetTerrainHitWithUV(double &x, double &y, double &u, double &v, int &whichTriangle);  //  Returns the current XY coord of the mouse on the terrain

	void AddHill();
	void AddHill(float x, float y, float height);

	void PushUp(int x, int y, int numberofpushes);
	void PushDown(int x, int y, int numberofpushes);

	DWORD ComputeLighting(int x, int y);
//	DWORD ComputeLightingSand(int x, int y);
//	DWORD ComputeLightingShallowWater(int x, int y);
//	DWORD ComputeLightingRuinedLand(int x, int y);
	int GetLightingComponent(int x, int y);
	int GetAlphaComponent(int x, int y);
	int GetMiddleAlphaComponent(int x, int y);

	inline bool IsWater(int x, int y) { return (GetValue(x, y) == 0 && GetValue(x + 1, y) == 0 && GetValue(x + 1, y + 1) == 0 && GetValue(x, y + 1) == 0);}

	float GetWater(float x, float y);

	bool IsCellVisible(int x, int y);
	bool IsPointVisible(int x, int y);

	void RuinAllLand();



	//friend dReal HeightfieldCallback( void* pUserData, int x, int z );

	int m_VertexWidth; //  The number of actual vertices wide the terrain is.
	int m_VertexHeight; //  The number of actual vertices tall the terrain is.

	int m_CellWidth;
	int m_CellHeight;

	float* m_Values;
	TerrainCell* m_TerrainTypes;

//	Bitmap* m_Texture;

	Bitmap* m_ShallowWaterTexture;
	Bitmap* m_DeepWaterTexture;
	Bitmap* m_SandTexture;
	Bitmap* m_GrassTexture;
	Bitmap* m_RuinTexture;
	Bitmap* m_BlessTexture;
	Bitmap* m_LavaTexture;
	Bitmap* m_FarmTexture;
	Bitmap* m_BlessedFarmTexture;
	Bitmap* m_HouseTexture;
	Bitmap* m_SwampTexture;

	Bitmap* m_MaskTexture;

	Bitmap* m_MiniMapTexture;

	Bitmap* m_TerrainHighlightRing;

	PlanitiaVertexBuffer* m_VertexBuffer = nullptr;
	int m_VertexBufferSize = 0;
	PlanitiaIndexBuffer* m_IndexBuffer[NUMBER_OF_VERTEX_BUFFERS] = {};
	int m_IndexBufferSizes[NUMBER_OF_VERTEX_BUFFERS] = {};

	PlanitiaVertexBuffer* m_WaterVertexBuffer = nullptr;
	int m_WaterVertexBufferSize = 0;
	PlanitiaIndexBuffer* m_WaterIndexBuffer = nullptr;
	int m_WaterIndexBufferSize = 0;

	PlanitiaVertexBuffer* m_HighlightVertexBuffer = nullptr;
	PlanitiaVertexBuffer* m_ColoredHighlightVertexBuffer[4] = {};

	bool m_DoWeNeedToRecreateVertexBuffers;
	bool m_DoWeNeedToRecreateIndexBuffers;
	bool m_DidWeRecreateVertexBuffersLastFrame;

	float m_WaterHeight;

	bool m_TerrainSwitcher;

	DWORD m_TimeSinceLastWaterUpdate;

	int m_TerrainViewRange;


	bool m_ShowTerrainHit;
	DWORD m_TerrainHitColor;

	int m_NumberOfTrisDrawnThisFrame;
};

#endif