#include "PlanitiaGlobals.h"
#include "PlanitiaDisplay.h"
#include "PlanitiaEngineAdapter.h"
#include "PlanitiaResourceManager.h"
#include "PlanitiaInput.h"
#include "Terrain.h"
#include "PlanitiaConfig.h"
#include "PlanitiaProfile.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace std;

Background::~Background()
{
   Shutdown();
}

void Background::Init(const std::string& configfile)
{
   m_IsDead = false;
   m_Mesh = gp_ResourceManager->GetMesh("Data/Meshes/standard.txt");
   m_Background = gp_ResourceManager->GetBitmap("Images/clouds.png");
}

void Background::Shutdown()
{
   /* resource manager owns bitmap */
}

void Background::Update()
{
   gp_Display->AddUnit(g_Background);
}

void Background::Draw()
{
   gp_Display->m_D3DDevice.SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
   gp_Display->m_D3DDevice.SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
   gp_Display->m_D3DDevice.SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

   D3DXMATRIX _Translate, _Scale;

   gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
   gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

   gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHABLENDENABLE, true);
   gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
   gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHAREF, (DWORD)8);
   gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);

   D3DXMATRIX matTrans;
   D3DXMatrixIdentity(&matTrans);
   gp_Display->m_D3DDevice.SetTransform(D3DTS_WORLD, &matTrans);

   gp_Display->m_D3DDevice.SetStreamSource(0, m_Mesh->m_VertexBuffer, 0, sizeof(PlanitiaVertex));

   gp_Display->m_D3DDevice.SetFVF(FVF);

   gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
   gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
   gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

   gp_Display->m_D3DDevice.SetTexture(0, m_Background->m_Bitmap);
   gp_Display->m_D3DDevice.DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

   gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
   gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
}


Terrain::~Terrain()
{
	Shutdown();
}

void Terrain::Init(const std::string& configfile)
{
	m_TimeSinceLastWaterUpdate = 0;

	m_Values = NULL;
	m_TerrainTypes = NULL;

	m_TerrainHitColor = D3DCOLOR_ARGB(255, 255, 255, 255);

	m_VertexBuffer = NULL;
	for(int i = 0; i < NUMBER_OF_VERTEX_BUFFERS; ++i)
	{
		m_IndexBuffer[i] = NULL;
	}

	m_WaterVertexBuffer = NULL;
	m_WaterIndexBuffer = NULL;

	LoadConfigFile(m_UnitConfig, configfile);

	m_IsDead = false;

	m_ShowTerrainHit = false;

	m_CellWidth = m_UnitConfig["width"].numdata;
	m_CellHeight = m_UnitConfig["height"].numdata;

	m_VertexWidth = m_CellWidth + 1;
	m_VertexHeight = m_CellHeight + 1;

	m_Values = new float[m_VertexWidth * m_VertexHeight];
	m_TerrainTypes = new TerrainCell[m_CellWidth * m_CellHeight];

	m_WaterHeight = 1.0f;

	ifstream instream;
	string filename = m_UnitConfig["map"].stringdata;

	

	char tempchar = 0;
	uint8_t actualvalue;

	m_DoWeNeedToRecreateVertexBuffers = true;
	m_DoWeNeedToRecreateIndexBuffers = true;

	memset(m_Values, 0, m_VertexWidth * m_VertexHeight * sizeof(float));

	InitializeMap(m_UnitConfig["seed"].numdata);
//	gp_Display->m_Camera.m_LookAtPoint.x = m_VertexWidth / 2;
//	gp_Display->m_Camera.m_LookAtPoint.z = m_VertexHeight / 2;
//	gp_Display->m_Camera.m_LookAtPoint.y = GetHeight(m_VertexWidth / 2, m_VertexHeight / 2);

//	instream.close();

//	m_Texture = gp_ResourceManager->GetBitmap(m_UnitConfig["texture"].stringdata);

	m_ShallowWaterTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["shallowwatertexture"].stringdata);

	m_DeepWaterTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["deepwatertexture"].stringdata);

	m_SandTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["sandtexture"].stringdata);

	m_GrassTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["grasstexture"].stringdata);

	m_RuinTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["ruintexture"].stringdata);

	m_MaskTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["masktexture"].stringdata);

	m_BlessTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["blesstexture"].stringdata);

	m_LavaTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["lavatexture"].stringdata);

	m_FarmTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["farmtexture"].stringdata);

	m_HouseTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["housetexture"].stringdata);

	m_SwampTexture = gp_ResourceManager->GetBitmap(m_UnitConfig["swamptexture"].stringdata);

	m_TerrainHighlightRing = gp_ResourceManager->GetBitmap("Images/TerrainHighlightRing.png");

	//  Since we are constructing this one ourselves, we will "own" this bitmap rather than the
	//  resource manager (which only handles loaded files).

	m_MiniMapTexture = new Bitmap();
	m_MiniMapTexture->m_Owned = true;
	gp_Display->m_D3DDevice.CreateTexture(m_CellWidth, m_CellHeight, 1, D3DUSAGE_DYNAMIC,
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_MiniMapTexture->m_Bitmap, nullptr);
	m_MiniMapTexture->m_Width = static_cast<float>(m_CellWidth);
	m_MiniMapTexture->m_Height = static_cast<float>(m_CellHeight);
	
	FindTerrainTypes();
	UpdateMiniMap();

	m_TerrainViewRange = 25;


	if(gp_Display->m_HardwareVertexProcessingSupported)
	{
		m_VertexBufferSize = m_CellHeight * m_CellWidth * 5;
	}
	else
	{
		m_VertexBufferSize = m_CellHeight * m_CellWidth * 4;
	}

	gp_Display->m_D3DDevice.CreateVertexBuffer( sizeof(PlanitiaVertex) * m_VertexBufferSize, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, FVF,
		D3DPOOL_DEFAULT, &m_VertexBuffer, NULL);

	gp_Display->m_D3DDevice.CreateVertexBuffer( sizeof(PlanitiaVertex) * m_CellWidth * m_CellHeight * 4, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, FVF,
		D3DPOOL_DEFAULT, &m_WaterVertexBuffer, NULL);

	gp_Display->m_D3DDevice.CreateVertexBuffer( sizeof(PlanitiaVertex) * 12, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, FVF,
		D3DPOOL_DEFAULT, &m_HighlightVertexBuffer, NULL);

    for(int i = 0; i < 4; ++i)
    {
        gp_Display->m_D3DDevice.CreateVertexBuffer( sizeof(PlanitiaVertex) * 12, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, FVF,
            D3DPOOL_DEFAULT, &m_ColoredHighlightVertexBuffer[i], NULL);
    }

	for(int i = 0; i < NUMBER_OF_VERTEX_BUFFERS; ++i)
	{
		gp_Display->m_D3DDevice.CreateIndexBuffer( sizeof(WORD) * m_CellWidth * m_CellHeight * 12, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
			D3DPOOL_MANAGED, &m_IndexBuffer[i], NULL);
	}

	gp_Display->m_D3DDevice.CreateIndexBuffer( sizeof(WORD) * m_CellWidth * m_CellHeight * 6, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
		D3DPOOL_MANAGED, &m_WaterIndexBuffer, NULL);

//	CreateVertexBuffers(true);
//	CreateIndexBuffers();
//	CreateWaterVertexBuffer();
//	CreateWaterIndexBuffer();

	m_TerrainSwitcher = true;

	m_NumberOfTrisDrawnThisFrame = 0;

	m_DidWeRecreateVertexBuffersLastFrame = false;

	g_NumberOfPlayers = m_UnitConfig["players"].numdata;

	int stopper = 0;
}

void Terrain::InitializeMap(int seed)
{
	g_VitalRNG.SeedRNG( seed );

	float _LowestHeight = 9999;
	float _HeighestHeight = -9999;
	float _HeightRange = 0;

	memset(m_TerrainTypes, -1, sizeof(int) * m_CellWidth * m_CellHeight);

	for(int i = 0; i < (m_VertexWidth + m_VertexHeight); ++i)
	{
		AddHill();
	}


	for(int i = 0; i < m_VertexWidth; ++i)
	{
		for(int j = 0; j < m_VertexHeight; ++j)
		{
			if(GetValue(i, j) < _LowestHeight) _LowestHeight = GetValue(i, j);
			if(GetValue(i, j) > _HeighestHeight) _HeighestHeight = GetValue(i, j);
		}
	}

	float _Scalar = 8.0f/_HeighestHeight;

	for(int i = 0; i < m_VertexWidth; ++i)
	{
		for(int j = 0; j < m_VertexHeight; ++j)
		{
			SetValue(i, j, GetValue(i, j) * _Scalar);
		}
	}

	//  Now we "sink" the terrain so that we don't always have lots of land

 	int _TerrainSinker = g_VitalRNG.Random(3);
 
 	for(int i = 0; i < m_VertexWidth; ++i)
 	{
 		for(int j = 0; j < m_VertexHeight; ++j)
 		{
			float originalvalue = GetValue(i, j);
			originalvalue -= _TerrainSinker;
			if(originalvalue < 0.0f) originalvalue = 0.0f;
 			SetValue(i, j, originalvalue);
 		}
 	}

	//  Now seam up the edges so that the water does not show through.
	for(int i = 0; i < m_VertexWidth; ++i)
 	{
 		for(int j = 0; j < m_VertexHeight; ++j)
 		{
			float originalvalue = GetValue(i, j);
			if(i == 0 || i == m_CellWidth || j == 0 || j == m_CellHeight)
			{
				SetValue(i, j, 0);				
			}

			if(i == 1 || i == m_CellWidth - 1 || j == 1 || j == m_CellHeight - 1)
			{
				if(originalvalue > 1) SetValue(i, j, 1);				
			}
 		}
 	}


	//  Experimental .5/0 split
// 	for(int i = 0; i < m_VertexWidth; ++i)
//  	{
//  		for(int j = 0; j < m_VertexHeight; ++j)
//  		{
// 			float fractionalPart = GetValue(i, j) - int(GetValue(i, j));
// 			if(fractionalPart >.3 && fractionalPart <= .7)
// 			{
// 				float newValue = int(GetValue(i, j)) + .5;
// 				SetValue(i, j, newValue);
// 			}
// 			else
// 			{
// 				SetValue(i, j, int(GetValue(i, j)));
// 			}
//  		}
//  	}
}

void Terrain::Draw()
{
	gp_Display->m_D3DDevice.SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	gp_Display->m_D3DDevice.SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	gp_Display->m_D3DDevice.SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	m_NumberOfTrisDrawnThisFrame = 0;

 	DrawTerrain();
   DrawHighlightMarker();
	DrawWater();
}

float Terrain::GetHeight(float x, float z)
{
	if(x < 0 || x > m_VertexWidth - 1 || z < 0 || z > m_VertexHeight - 1)
	{
		return -9999;
	}


	int _XWhole = int(x);
	int _ZWhole = int(z);

	float A = GetValue(_XWhole    , _ZWhole    );
	float B = GetValue(_XWhole + 1, _ZWhole    );
	float C = GetValue(_XWhole    , _ZWhole + 1);
	float D = GetValue(_XWhole + 1, _ZWhole + 1);
	float E = (A + B + C + D) / 4;

	float _XFrac = x - _XWhole;
	float _ZFrac = z - _ZWhole;

	float height = 0.0f;

	if(_ZFrac + _XFrac < 1.0)// && _ZFrac + (1.0 - _XFrac) > 1.0)  // Upper triangle ABE
	{
		float uy = B - A; // A->B
		float vy = C - A; // A->C
		height = A + Lerp(0.0f, uy, _XFrac) + Lerp(0.0f, vy, _ZFrac);
	}

// 	if(_ZFrac + _XFrac < 1.0 && _ZFrac + (1.0 - _XFrac) < 1.0)  // Left triangle DAE
// 	{
// 		float uy = D - E; // A->B
// 		float vy = A - E; // A->C
// 		height = E + Lerp(0.0f, uy, _XFrac) + Lerp(0.0f, vy, _ZFrac);
// 	}
// 
// 	if(_ZFrac + _XFrac > 1.0 && _ZFrac + (1.0 - _XFrac) > 1.0)  // Right triangle BCE
// 	{
// 		float uy = B - E; // A->B
// 		float vy = C - E; // A->C
// 		height = E + Lerp(0.0f, uy, _XFrac) + Lerp(0.0f, vy, _ZFrac);
// 	}

	else // Lower triangle CDE
	{
		float uy = C - D; // D->C
		float vy = B - D; // D->B
		height = D + Lerp(0.0f, uy, 1.0f - _XFrac) + Lerp(0.0f, vy, 1.0f - _ZFrac);
	}

	return height + .1;
}

void Terrain::Update()
{
//	ProfileBegin("Filling Unit Location Table");
	//  Update the location map for all units
/*	for(int i = 0; i < m_CellWidth * m_CellHeight; ++i)
	{
		g_UnitLocations[i].clear();
	}

	for( list<PlanitiaUnit*>::iterator node = g_UnitList.begin();
		node != g_UnitList.end(); ++node)
	{
		g_UnitLocations[ int((*node)->m_Pos.x) + int(((*node)->m_Pos.z) * m_CellWidth)].push_back((*node)->m_UnitID);

	}*/

//	ProfileEnd("Filling Unit Location Table");

	//  Update the terrain from its internal data.
	for(int i = 0; i < m_CellWidth; ++i)
	{
		for(int j = 0; j < m_CellHeight; ++j)
		{
			if(GetTerrainType(i, j) == TT_RUINEDLAND)
			{
				DWORD data = GetTerrainAdditionalData( i, j );
				if( data > 0 )
				{
					if( gp_Engine->m_NumberOfUpdatesToDoThisFrame > 0 )
					{
						SetTerrainAdditionalData( i, j, data - 1 );
					}
				}
				else
				{
					SetTerrainAdditionalData(i, j, 0);
					SetTerrainType(i, j, FindTerrainType(i, j));
					m_DoWeNeedToRecreateIndexBuffers = true;
					m_DoWeNeedToRecreateVertexBuffers = true;
				}
			}

			if(GetTerrainType(i, j) == TT_BLESSEDLAND)
			{
				DWORD data = GetTerrainAdditionalData( i, j );
				if(data > 0)
				{
					if( gp_Engine->m_NumberOfUpdatesToDoThisFrame > 0 )
					{
						SetTerrainAdditionalData( i, j, data - 1 );
					}
				}
				else
				{
					SetTerrainAdditionalData(i, j, 0);
					SetTerrainType(i, j, TT_FARMLAND);
					m_DoWeNeedToRecreateIndexBuffers = true;
					m_DoWeNeedToRecreateVertexBuffers = true;
				}
			}

			if(GetTerrainType(i, j) == TT_SWAMP)
			{
				DWORD data = GetTerrainAdditionalData(i, j);
				if( data > 0 )
				{
					if( gp_Engine->m_NumberOfUpdatesToDoThisFrame > 0 )
					{
						SetTerrainAdditionalData(i, j, data - 1);
					}
				}
				else
				{
					SetTerrainAdditionalData(i, j, 0);
					SetTerrainType(i, j, FindTerrainType(i, j));
					m_DoWeNeedToRecreateIndexBuffers = true;
					m_DoWeNeedToRecreateVertexBuffers = true;
				}
			}
		}
	}



	bool _DoWeNeedToUpdateMinimap = false;
	if(m_DoWeNeedToRecreateVertexBuffers)
	{
		_DoWeNeedToUpdateMinimap = true;
	}

	if(m_DoWeNeedToRecreateVertexBuffers || gp_Display->m_Camera.m_DidCameraChangeThisFrame)
	{
		CreateVertexBuffers(false);
		m_DidWeRecreateVertexBuffersLastFrame = true;
	}
	else
	{
		m_DidWeRecreateVertexBuffersLastFrame = false;
	}

	CreateWaterVertexBuffer();
	CreateHighlightVertexBuffer();

	if(m_DoWeNeedToRecreateIndexBuffers || gp_Display->m_Camera.m_DidCameraChangeThisFrame)
	{
		CreateIndexBuffers();
	}

	if(_DoWeNeedToUpdateMinimap)
	{
		UpdateMiniMap();
	}

   
	gp_Display->AddUnit( g_Terrain );
   
}

void Terrain::Shutdown()
{
	delete [] m_Values;
	delete [] m_TerrainTypes;

	if (m_VertexBuffer) { delete m_VertexBuffer; m_VertexBuffer = nullptr; }
	if (m_WaterVertexBuffer) { delete m_WaterVertexBuffer; m_WaterVertexBuffer = nullptr; }
	if (m_HighlightVertexBuffer) { delete m_HighlightVertexBuffer; m_HighlightVertexBuffer = nullptr; }
	for (int i = 0; i < 4; ++i)
	{
		if (m_ColoredHighlightVertexBuffer[i]) { delete m_ColoredHighlightVertexBuffer[i]; m_ColoredHighlightVertexBuffer[i] = nullptr; }
	}
	if (m_WaterIndexBuffer) { delete m_WaterIndexBuffer; m_WaterIndexBuffer = nullptr; }
	for (int i = 0; i < NUMBER_OF_VERTEX_BUFFERS; ++i)
	{
		if (m_IndexBuffer[i]) { delete m_IndexBuffer[i]; m_IndexBuffer[i] = nullptr; }
	}
	if (m_MiniMapTexture) { delete m_MiniMapTexture; m_MiniMapTexture = nullptr; }
}

float Terrain::GetValue(int x, int y)
{
	if(x >= 0 && x < m_VertexWidth && y >= 0 && y < m_VertexHeight)
	{
		return m_Values[x + (y * m_VertexWidth)];
	}
	else
	{
		return 0;
	}
}

void Terrain::SetValue(int x, int y, float value)
{
	if(x >= 0 && x < m_VertexWidth && y >= 0 && y < m_VertexHeight)
	{
		m_Values[x + (y * m_VertexWidth)] = value;
		m_DoWeNeedToRecreateVertexBuffers = true;
	}
}

void Terrain::SetUpODE()
{
// 	dTriMeshDataID Data = dGeomTriMeshDataCreate();
// 
// 	dGeomTriMeshDataBuildSingle
// 		(
// 		Data, 
// 		world_vertices, 
// 		3 * sizeof(float), 
// 		numv, 
// 		world_indices, 
// 		numi, 
// 		3 * sizeof(int)
// 		);
// 
// 	m_ODEMesh = dCreateTriMesh(g_ODESpace, Data, 0, 0, 0);
}

void Terrain::CreateHighlightVertexBuffer()
{
	double i = 0;
	double j = 0;
    double u = 0;
    double v = 0;
	int whichTriangle;
	GetTerrainHitWithUV(i, j, u, v, whichTriangle);

    float x, y;
	x = i - .5;
    y = j - .5;

	PlanitiaVertex* _Vertices;

	m_HighlightVertexBuffer->Lock(0, 12 * sizeof(PlanitiaVertex), (void**)&_Vertices, 0);

	DWORD lighting = D3DCOLOR_ARGB(255, 255, 255, 255);

    D3DXVECTOR3 corners[4];
    corners [0] = D3DXVECTOR3( -.5, 0, -.5 );
    corners [1] = D3DXVECTOR3( -.5, 0,  .5 );
    corners [2] = D3DXVECTOR3(  .5, 0,  .5 );
    corners [3] = D3DXVECTOR3(  .5, 0, -.5 );


    D3DXMATRIX transform;
//    D3DXMatrixTranslation( &transform, -.5, 0, -.5 );
    D3DXVECTOR3 coord = D3DXVECTOR3( 1, 0, 1 );
//    D3DXVec3TransformCoord( &coord, &coord, &transform );
    D3DXMatrixRotationY( &transform, gp_Engine->m_GameTimeInSeconds );

    for( int i = 0; i < 4; ++ i )
    {
        D3DXVec3TransformCoord( &corners[i], &corners[i], &transform );
        corners[i].x += .5;
        corners[i].z += .5;
    }
//    D3DXMatrixTranslation( &transform, .5, 0, .5 );
//    D3DXVec3TransformCoord( &coord, &coord, &transform );
    

	_Vertices[0] =  PlanitiaVertex( x     , GetHeight (x     , y      ), y     , lighting,  corners[0].x, corners[0].z  );
	_Vertices[1] =  PlanitiaVertex( x +  1, GetHeight (x +  1, y      ), y     , lighting,  corners[1].x, corners[1].z  );
	_Vertices[2] =  PlanitiaVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,                .5,        .5 );
	_Vertices[3] =  PlanitiaVertex( x +  1, GetHeight (x +  1, y      ), y     , lighting,  corners[1].x, corners[1].z  );
	_Vertices[4] =  PlanitiaVertex( x +  1, GetHeight (x +  1, y +  1 ), y +  1, lighting,  corners[2].x, corners[2].z  );
	_Vertices[5] =  PlanitiaVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,                .5, .5 );
	_Vertices[6] =  PlanitiaVertex( x +  1, GetHeight (x +  1, y +  1 ), y +  1, lighting,  corners[2].x, corners[2].z  );
	_Vertices[7] =  PlanitiaVertex( x     , GetHeight (x     , y +  1 ), y +  1, lighting,  corners[3].x, corners[3].z  );
	_Vertices[8] =  PlanitiaVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,  .5, .5 );
	_Vertices[9] =  PlanitiaVertex( x     , GetHeight (x     , y +  1 ), y +  1, lighting,  corners[3].x, corners[3].z  );
	_Vertices[10] = PlanitiaVertex( x     , GetHeight (x     , y      ), y     , lighting,  corners[0].x, corners[0].z  );
	_Vertices[11] = PlanitiaVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,  .5, .5 );

	m_HighlightVertexBuffer->Unlock();

    for( int i = 0; i < 4; ++i )
    {
        if( g_Players[i] != NULL )
        {
            if( g_Players[i]->m_General != NULL )
            {
                PlanitiaVertex* _Vertices;

                float x = g_Players[i]->m_General->m_Pos.x - .5;
                float y = g_Players[i]->m_General->m_Pos.z - .5;

                m_ColoredHighlightVertexBuffer[i]->Lock(0, 12 * sizeof(PlanitiaVertex), (void**)&_Vertices, 0);

                DWORD lighting = D3DCOLOR_ARGB(255, 255, 255, 255);

                _Vertices[0] =  PlanitiaVertex( x     , GetHeight (x     , y      ), y     , lighting,  0,         0         );
                _Vertices[1] =  PlanitiaVertex( x +  1, GetHeight (x +  1, y      ), y     , lighting,  0,         1  );
                _Vertices[2] =  PlanitiaVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,                .5,        .5 );
                _Vertices[3] =  PlanitiaVertex( x +  1, GetHeight (x +  1, y      ), y     , lighting,  0,         1  );
                _Vertices[4] =  PlanitiaVertex( x +  1, GetHeight (x +  1, y +  1 ), y +  1, lighting,  1 , 1  );
                _Vertices[5] =  PlanitiaVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,                .5, .5 );
                _Vertices[6] =  PlanitiaVertex( x +  1, GetHeight (x +  1, y +  1 ), y +  1, lighting,  1 , 1  );
                _Vertices[7] =  PlanitiaVertex( x     , GetHeight (x     , y +  1 ), y +  1, lighting,  1 , 0         );
                _Vertices[8] =  PlanitiaVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,  .5, .5 );
                _Vertices[9] =  PlanitiaVertex( x     , GetHeight (x     , y +  1 ), y +  1, lighting,  1 , 0         );
                _Vertices[10] = PlanitiaVertex( x     , GetHeight (x     , y      ), y     , lighting,  0       , 0         );
                _Vertices[11] = PlanitiaVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,  .5, .5 );

                m_ColoredHighlightVertexBuffer[i]->Unlock();
            }
        }
    }
}

void Terrain::CreateIndexBuffers()
{
	//  Now we need to actually fill out the vertex and index buffers.
	vector<WORD> _IndexBuckets[NUMBER_OF_VERTEX_BUFFERS];

	for(int i = 0; i < NUMBER_OF_VERTEX_BUFFERS; ++i)
	{
		m_IndexBufferSizes[i] = 0;
		_IndexBuckets[i].clear();
	}

	//  FIRST we must count up how many vertices there will be for each terrain type.
	int starti = gp_Display->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
	if(starti < 0) starti = 0;
	int stopi = gp_Display->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
	if(stopi > m_CellWidth) stopi = m_CellWidth;

	int startj = gp_Display->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
	if(startj < 0) startj = 0;
	int stopj = gp_Display->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
	if(stopj > m_CellHeight) stopj = m_CellHeight;

	for(int i = starti; i <= stopi; ++i)
	{
		for(int j = startj; j < stopj; ++j)
		{
			if(GlobalIsDistanceLessThan(i, j, gp_Display->m_Camera.m_LookAtPoint.x,
				gp_Display->m_Camera.m_LookAtPoint.z, m_TerrainViewRange) && IsCellVisible(i, j))
			{
				int TerrainType = GetTerrainType(i, j);

				if(TerrainType != -1) //  We ran off the grid somehow
				{
					if(gp_Display->m_HardwareVertexProcessingSupported)
					{
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5)    );
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 1);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 4);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 1);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 2);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 4);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 2);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 3);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 4);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 3);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 0);
						_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 5) + 4);
					}
					else
					{
						float diffA = abs(GetValue(i, j) - GetValue(i - 1, j - 1));
						float diffB = abs(GetValue(i, j - 1) - GetValue(i - 1, j));
						bool triFlip = diffA > diffB;

						if(!triFlip)
						{
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4)    );
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4) + 1);
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4) + 2);
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4) + 2);
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4) + 3);
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4)    );
						}
						else
						{
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4) + 1);
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4) + 2);
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4) + 3);
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4) + 3);
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4)    );
							_IndexBuckets[TerrainType].push_back(((i * m_CellWidth + j) * 4) + 1);

						}
					}
				}
			}
		}
	}

	for(int i = 0; i < NUMBER_OF_VERTEX_BUFFERS; ++i)
	{
		if(_IndexBuckets[i].size() > 0)
		{
			WORD* _Indices;
			m_IndexBufferSizes[i] = _IndexBuckets[i].size();
			m_IndexBuffer[i]->Lock(0, _IndexBuckets[i].size() * sizeof(WORD), (void**)&_Indices, 0);
			for(int j = 0; j < m_IndexBufferSizes[i]; ++j)
			{
				_Indices[j] = _IndexBuckets[i][j];
			}
			m_IndexBuffer[i]->Unlock();
		}
	}

	m_DoWeNeedToRecreateIndexBuffers = false;
}

void Terrain::CreateVertexBuffers(bool rebuildall)
{
	PlanitiaVertex* _Vertices;

	m_VertexBuffer->Lock(0, m_VertexBufferSize * sizeof(PlanitiaVertex), (void**)&_Vertices, D3DLOCK_DISCARD);

	int starti = gp_Display->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
	if(starti < 0) starti = 0;
	int stopi = gp_Display->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
	if(stopi > m_CellWidth - 1) stopi = m_CellWidth - 1;

	int startj = gp_Display->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
	if(startj < 0) startj = 0;
	int stopj = gp_Display->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
	if(stopj > m_CellWidth - 1) stopj = m_CellWidth - 1;

	for(int i = starti; i <= stopi; ++i)
	{
		for(int j = startj; j <= stopj; ++j)
		{
			if(rebuildall || GlobalIsDistanceLessThan(i, j, gp_Display->m_Camera.m_LookAtPoint.x, gp_Display->m_Camera.m_LookAtPoint.z, m_TerrainViewRange))
			{
				int lightingUL;
				if(i == 0 || i == m_CellWidth || j == 0 || j == m_CellHeight)
					lightingUL = 0;
				else					
					lightingUL = GetLightingComponent(i, j);
				int alphaUL = GetAlphaComponent(i, j);

				int lightingUR;
				if((i + 1) == 0 || (i + 1) == m_CellWidth || j == 0 || j == m_CellHeight)
					lightingUR = 0;
				else					
					lightingUR = GetLightingComponent(i + 1, j);
				int alphaUR = GetAlphaComponent(i + 1, j);

				int lightingLL;
				if(i == 0 || i == m_CellWidth || (j + 1) == 0 || (j + 1) == m_CellHeight)
					lightingLL = 0;
				else					
					lightingLL = GetLightingComponent(i, j + 1);
				int alphaLL = GetAlphaComponent(i, j + 1);

				int lightingLR;
				if((i + 1) == 0 || (i + 1) == m_CellWidth || (j + 1) == 0 || (j + 1) == m_CellHeight)
					lightingLR = 0;
				else					
					lightingLR = GetLightingComponent(i + 1, j + 1);
				int alphaLR = GetAlphaComponent(i + 1, j + 1);

				int lightingMID = (lightingUL + lightingUR + lightingLL + lightingLR) / 4;
				int alphaMID = (alphaUL + alphaUR + alphaLL + alphaLR) / 4;

				float _u = 0;
				float _v = 0;

				float _Aggregate = 0;

				if(GetTerrainType(i, j) == TT_RUINEDLAND)
				{
					if(GetTerrainType(i    , j - 1) == TT_RUINEDLAND) _Aggregate += 1;
					if(GetTerrainType(i + 1, j    ) == TT_RUINEDLAND) _Aggregate += 2;
					if(GetTerrainType(i    , j + 1) == TT_RUINEDLAND) _Aggregate += 4;
					if(GetTerrainType(i - 1, j    ) == TT_RUINEDLAND) _Aggregate += 8;
				}

				if(GetTerrainType(i, j) == TT_BLESSEDLAND)
				{
					if(GetTerrainType(i    , j - 1) == TT_BLESSEDLAND) _Aggregate += 1;
					if(GetTerrainType(i + 1, j    ) == TT_BLESSEDLAND) _Aggregate += 2;
					if(GetTerrainType(i    , j + 1) == TT_BLESSEDLAND) _Aggregate += 4;
					if(GetTerrainType(i - 1, j    ) == TT_BLESSEDLAND) _Aggregate += 8;
				}

				if(GetTerrainType(i, j) == TT_SWAMP)
				{
					if(GetTerrainType(i    , j - 1) == TT_SWAMP) _Aggregate += 1;
					if(GetTerrainType(i + 1, j    ) == TT_SWAMP) _Aggregate += 2;
					if(GetTerrainType(i    , j + 1) == TT_SWAMP) _Aggregate += 4;
					if(GetTerrainType(i - 1, j    ) == TT_SWAMP) _Aggregate += 8;
				}

				if(GetTerrainType(i, j) == TT_LAVA)
				{
					if(GetTerrainType(i    , j - 1) == TT_LAVA) _Aggregate += 1;
					if(GetTerrainType(i + 1, j    ) == TT_LAVA) _Aggregate += 2;
					if(GetTerrainType(i    , j + 1) == TT_LAVA) _Aggregate += 4;
					if(GetTerrainType(i - 1, j    ) == TT_LAVA) _Aggregate += 8;
				}

				_u = _Aggregate * .0625f;

				if(gp_Display->m_HardwareVertexProcessingSupported)
				{
					_Vertices[((i * m_CellWidth) + j) * 5 + 0] =  PlanitiaVertex(i     , GetValue (i    , j    ), j     , D3DCOLOR_ARGB(alphaUL, lightingUL, lightingUL, lightingUL),      i     , j     , _u, _v );
					_Vertices[((i * m_CellWidth) + j) * 5 + 1] =  PlanitiaVertex(i +  1, GetValue (i + 1, j    ), j     , D3DCOLOR_ARGB(alphaUR, lightingUR, lightingUR, lightingUR),      i +  1, j     , _u + .0625 , _v );
					_Vertices[((i * m_CellWidth) + j) * 5 + 2] =  PlanitiaVertex(i +  1, GetValue (i + 1, j + 1), j +  1, D3DCOLOR_ARGB(alphaLR, lightingLR, lightingLR, lightingLR),      i +  1, j +  1, _u + .0625 , _v + 1 );
					_Vertices[((i * m_CellWidth) + j) * 5 + 3] =  PlanitiaVertex(i     , GetValue (i    , j + 1), j +  1, D3DCOLOR_ARGB(alphaLL, lightingLL, lightingLL, lightingLL),      i     , j +  1, _u         , _v + 1 );
					_Vertices[((i * m_CellWidth) + j) * 5 + 4] =  PlanitiaVertex(i + .5, GetMiddle(i    , j    ), j + .5, D3DCOLOR_ARGB(alphaMID, lightingMID, lightingMID, lightingMID),  i + .5, j + .5, _u + .03125, _v + .5 );
				}
				else
				{
					_Vertices[((i * m_CellWidth) + j) * 4 + 0] =  PlanitiaVertex(i     , GetValue (i    , j    ), j     , D3DCOLOR_ARGB(alphaUL, lightingUL, lightingUL, lightingUL),  i    , j    , _u, _v);
					_Vertices[((i * m_CellWidth) + j) * 4 + 1] =  PlanitiaVertex(i +  1, GetValue (i + 1, j    ), j     , D3DCOLOR_ARGB(alphaUR, lightingUR, lightingUR, lightingUR),  i + 1, j    , _u + .0625 , _v);
					_Vertices[((i * m_CellWidth) + j) * 4 + 2] =  PlanitiaVertex(i +  1, GetValue (i + 1, j + 1), j +  1, D3DCOLOR_ARGB(alphaLR, lightingLR, lightingLR, lightingLR),  i + 1, j + 1, _u + .0625 , _v + 1);
					_Vertices[((i * m_CellWidth) + j) * 4 + 3] =  PlanitiaVertex(i     , GetValue (i    , j + 1), j +  1, D3DCOLOR_ARGB(alphaLL, lightingLL, lightingLL, lightingLL),  i    , j + 1, _u         , _v + 1);
				}
			}
		}
	}

	m_VertexBuffer->Unlock();

	m_DoWeNeedToRecreateVertexBuffers = false;
}

void Terrain::CreateWaterVertexBuffer()
{
	//  Scan the terrain and find the squares that require water cover.
	m_WaterVertexBufferSize = 0;

	int starti = gp_Display->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
	if(starti < 0) starti = 0;
	int stopi = gp_Display->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
	if(stopi > m_CellWidth - 1) stopi = m_CellWidth - 1;

	int startj = gp_Display->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
	if(startj < 0) startj = 0;
	int stopj = gp_Display->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
	if(stopj > m_CellWidth - 1) stopj = m_CellWidth - 1;

	for(int i = starti; i <= stopi; ++i)
	{
		for(int j = startj; j <= stopj; ++j)
		{
			if(GlobalIsDistanceLessThan(i, j, gp_Display->m_Camera.m_LookAtPoint.x, gp_Display->m_Camera.m_LookAtPoint.z, m_TerrainViewRange))
			{
				if(IsWaterCell(i, j))
					m_WaterVertexBufferSize += 4;
			}
		}
	}

	PlanitiaVertex* _WaterVertices;

	m_WaterVertexBuffer->Lock(0, m_WaterVertexBufferSize * sizeof(PlanitiaVertex), (void**)&_WaterVertices, D3DLOCK_DISCARD);

	int _LocalWaterVertexCounter = 0;

	for(float i = starti; i <= stopi; ++i)
	{
		for(float j = startj; j <= stopj; ++j)
		{
			if(GlobalIsDistanceLessThan(i, j, gp_Display->m_Camera.m_LookAtPoint.x, gp_Display->m_Camera.m_LookAtPoint.z, m_TerrainViewRange))
			{
				if(IsWaterCell(i, j))
				{
					int lightingUL;
					if(i == 0 || i == m_CellWidth || j == 0 || j == m_CellHeight)
						lightingUL = 0;
					else					
						lightingUL = 192;
					
					int lightingUR;
					if((i + 1) == 0 || (i + 1) == m_CellWidth || j == 0 || j == m_CellHeight)
						lightingUR = 0;
					else					
						lightingUR = 192;
					
					int lightingLL;
					if(i == 0 || i == m_CellWidth || (j + 1) == 0 || (j + 1) == m_CellHeight)
						lightingLL = 0;
					else					
						lightingLL = 192;
					
					int lightingLR;
					if((i + 1) == 0 || (i + 1) == m_CellWidth || (j + 1) == 0 || (j + 1) == m_CellHeight)
						lightingLR = 0;
					else					
						lightingLR = 192;

                    int alphaUL = 192;
                    int alphaUR = 192;
                    int alphaLL = 192;
					int alphaLR = 192;

					float UL = GetWater(i    , j) / 2;
					float UR = GetWater(i + 1, j) / 2;
					float LL = GetWater(i    , j + 1) / 2;
					float LR = GetWater(i + 1, j + 1) / 2;

					float x, y, xplus1, yplus1;

					x = i;
					y = j;
					xplus1 = x + 1;
					yplus1 = y + 1;

					if(x == 0)
						x -= .5;
					if(y == 0)
						y -= .5;
					if(xplus1 == m_CellWidth)
						xplus1 += .5;
					if(yplus1 == m_CellHeight)
						yplus1 += .5;

					_WaterVertices[_LocalWaterVertexCounter + 0] =  PlanitiaVertex(x     , .2 + UL, y     , D3DCOLOR_ARGB(alphaUL, lightingUL, lightingUL, lightingUL),  ((i +     (gp_Engine->m_GameTimeInSeconds / 2)) * .25) + (UL * 2) - (.1 *gp_Display->m_Camera.m_LookAtPoint.x) + (2 * GetValue(i    , j    )), ((j +     (gp_Engine->m_GameTimeInSeconds / 2)) * .25) + (UL * 2) - (.1 *gp_Display->m_Camera.m_LookAtPoint.z) + (2 * GetValue(i    , j    )));
					_WaterVertices[_LocalWaterVertexCounter + 1] =  PlanitiaVertex(xplus1, .2 + UR, y     , D3DCOLOR_ARGB(alphaUR, lightingUR, lightingUR, lightingUR),  ((i + 1 + (gp_Engine->m_GameTimeInSeconds / 2)) * .25) + (UR * 2) - (.1 *gp_Display->m_Camera.m_LookAtPoint.x) + (2 * GetValue(i + 1, j    )), ((j +     (gp_Engine->m_GameTimeInSeconds / 2)) * .25) + (UR * 2) - (.1 *gp_Display->m_Camera.m_LookAtPoint.z) + (2 * GetValue(i + 1, j    )));
					_WaterVertices[_LocalWaterVertexCounter + 2] =  PlanitiaVertex(xplus1, .2 + LR, yplus1, D3DCOLOR_ARGB(alphaUL, lightingLR, lightingLR, lightingLR),  ((i + 1 + (gp_Engine->m_GameTimeInSeconds / 2)) * .25) + (LR * 2) - (.1 *gp_Display->m_Camera.m_LookAtPoint.x) + (2 * GetValue(i + 1, j + 1)), ((j + 1 + (gp_Engine->m_GameTimeInSeconds / 2)) * .25) + (LR * 2) - (.1 *gp_Display->m_Camera.m_LookAtPoint.z) + (2 * GetValue(i + 1, j + 1)));
					_WaterVertices[_LocalWaterVertexCounter + 3] =  PlanitiaVertex(x     , .2 + LL, yplus1, D3DCOLOR_ARGB(alphaUL, lightingLL, lightingLL, lightingLL),  ((i +     (gp_Engine->m_GameTimeInSeconds / 2)) * .25) + (LL * 2) - (.1 *gp_Display->m_Camera.m_LookAtPoint.x) + (2 * GetValue(i    , j + 1)), ((j + 1 + (gp_Engine->m_GameTimeInSeconds / 2)) * .25) + (LL * 2) - (.1 *gp_Display->m_Camera.m_LookAtPoint.z) + (2 * GetValue(i    , j + 1)));

					_LocalWaterVertexCounter += 4;
				}
			}
		}
	}

	m_WaterVertexBuffer->Unlock();

	if(m_WaterVertexBufferSize != _LocalWaterVertexCounter)
	{
		int stopper = 0;
	}

	CreateWaterIndexBuffer();
}

void Terrain::CreateWaterIndexBuffer()
{
	//  Create Index buffer.  This is simple, since we're just drawing everything.

	//  Now we need to actually fill out the vertex and index buffers.
	int _NumberOfWaterCells = (m_WaterVertexBufferSize / 4);
	m_WaterIndexBufferSize = _NumberOfWaterCells * 6;


	WORD* _Indices;
	int _LocalIndexCounter = 0;

	m_WaterIndexBuffer->Lock(0, m_WaterIndexBufferSize * sizeof(WORD), (void**)&_Indices, 0);

	int cellindex = 0;
	for(int i = 0; i < _NumberOfWaterCells; ++i)
	{
		_Indices[_LocalIndexCounter +  0] =  i * 4;
		_Indices[_LocalIndexCounter +  1] =  i * 4 + 1;
		_Indices[_LocalIndexCounter +  2] =  i * 4 + 3;
		_Indices[_LocalIndexCounter +  3] =  i * 4 + 2;
		_Indices[_LocalIndexCounter +  4] =  i * 4 + 3;
		_Indices[_LocalIndexCounter +  5] =  i * 4 + 1;

		_LocalIndexCounter += 6;
	}

	m_WaterIndexBuffer->Unlock();
}

float Terrain::GetWater(float x, float y)
{
//	float animation = cos((x + y + gp_Engine->m_GameTimeInSeconds) * 3);//float(int(x * x + y * x + gp_Engine->m_GameTimeInMS) % 1000) / 1000.0f;

	float _gt = gp_Engine->m_GameTimeInSeconds;
	float _i = x;
	float _j = y;
	float animation = ( sin(2*(_gt)+1.3f*(_i)+0.7f*(_j)) + cos(3*(_gt)+1.5f*(_i)-1.5f*(_j)) );

//	if(animation > .5) animation = 1 - animation;

	animation *= .02;

	return animation;
}

void Terrain::GetTerrainHit(int &x, int &y)
{
	static int resultx = 0;
	static int resulty = 0;
	//  First things first; we must compute the picking ray.  We know the origin
	//  of the picking ray; it's the current position of the camera.  We need
	//  to turn the 2D mouse coordinates into a 3D point that will represent the
	//  end of the ray.  Since we can get the current mouse coordinates straight
	//  from the Input subsystem and already know our projection matrix, we
	//  don't need any additional information to get the ray.

	//  We turn our 2D coordinates into 3D coordinates by doing an inverse of
	//  the projection transform.  The projection transform turns 3D points into
	//  2D screen space points; we need to do the exact opposite.  We already
	//  know our z-coordinate; the projection matrix always projects points
	//  such that z is equal to 1.
	float _ProjectedX;
	float _ProjectedY;
	float _ProjectedZ = 1.0f;

	D3DXMATRIX _Proj; // The inverse 
	gp_Display->m_D3DDevice.GetTransform(D3DTS_PROJECTION, &_Proj);

	_ProjectedX = (( float( 2.0f * gp_Input->m_MouseX) / float(gp_Display->m_HRes) ) - 1.0f) / _Proj(0, 0);
	_ProjectedY = (( float(-2.0f * gp_Input->m_MouseY) / float(gp_Display->m_VRes) ) + 1.0f) / _Proj(1, 1);

	D3DXVECTOR3 _RayOrigin = D3DXVECTOR3(0, 0, 0);
	D3DXVECTOR3 _RayDirection = D3DXVECTOR3(_ProjectedX, _ProjectedY, _ProjectedZ);
	D3DXVec3Normalize(&_RayDirection, &_RayDirection);

	//  Woohoo, we've got our ray!  But we've got a problem.  This ray is in VIEW
	//  SPACE.  We need to get the ray into WORLD SPACE.  We do that by finding
	//  the inverse of the view transformation and applying it to both the location
	//  and the direction of the ray.

	D3DXMATRIX _correctForCamera;
	gp_Display->m_D3DDevice.GetTransform(D3DTS_VIEW, &_correctForCamera);
	D3DXMatrixInverse(&_correctForCamera, 0, &_correctForCamera);
	D3DXVec3TransformCoord(&_RayOrigin, &_RayOrigin, &_correctForCamera);
	D3DXVec3TransformNormal(&_RayDirection, &_RayDirection, &_correctForCamera);

	//  Woohoo, we've got our ray!  But we've got a problem.  This ray is in
	//  WORLD SPACE.  The triangle we're testing against is in its own
	//  MODEL SPACE.  One is going to have to get transformed into the other
	//  before we can compare the two.  Now, once upon a time we would have had
	//  to transform our model's vertices into world space by hand on every
	//  frame, thus we would have had a handy copy of our model in world space
	//  to use.  Since D3D does all the transformation for us, we have no handy
	//  copy and the only geometry we have is in model space.  Thus, we will
	//  transform the ray into the model space.  We do this by tranforming it
	//  by the INVERSE of the model's current transformation matrix (which we
	//  must create and store ourselves, so we do have access to it).

//	D3DXMATRIX _Inv;

//	D3DXMatrixInverse(&_Inv, 0, &m_CurrentTransform);

	//  All right, we apply this matrix to our ray.

//	D3DXVec3TransformCoord(&_RayOrigin, &_RayOrigin, &_Inv);
//	D3DXVec3TransformNormal(&_RayDirection, &_RayDirection, &_Inv);




	//  Find out what square the mouse is in, we're going to highlight it.
	//  If we didn't click on a unit, see if we clicked on the terrain.
	D3DXMATRIX _Identity;
	D3DXMatrixIdentity(&_Identity);
	int i, j;
	struct result{int x; int y; double distance;};
	vector<result> results; 

	int starti = gp_Display->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
	if(starti < 0) starti = 0;
	int stopi = gp_Display->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
	if(stopi > m_CellWidth - 1) stopi = m_CellWidth - 1;

	int startj = gp_Display->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
	if(startj < 0) startj = 0;
	int stopj = gp_Display->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
	if(stopj > m_CellWidth - 1) stopj = m_CellWidth - 1;

	for(int i = starti; i <= stopi; ++i)
	{
		for(int j = startj; j < stopj; ++j)
		{
			double distance;
			if(gp_Display->Pick(_RayOrigin, _RayDirection, 
				D3DXVECTOR3(i    , GetValue(i    , j    ), j    ),
				D3DXVECTOR3(i + 1, GetValue(i + 1, j    ), j    ),
				D3DXVECTOR3(i    , GetValue(i    , j + 1), j + 1), _Identity, distance) ||
				gp_Display->Pick(_RayOrigin, _RayDirection,
				D3DXVECTOR3(i + 1, GetValue(i + 1, j + 1), j + 1),
				D3DXVECTOR3(i    , GetValue(i    , j + 1), j + 1),
				D3DXVECTOR3(i + 1, GetValue(i + 1, j    ), j    ), _Identity, distance)
				)
			{
				result temp = {i, j, distance};
				results.push_back(temp);
			}
		}
	}

	//  Got our results, sort the vector and set the targetX and targetY
	int finalx = 0;
	int finaly = 0;
	double finaldistance = 99999;
	vector<result>::iterator node = results.begin();
	for(node; node != results.end(); ++node)
	{
		if((*node).distance < finaldistance)
		{
			finalx = (*node).x;
			finaly = (*node).y;
			finaldistance = (*node).distance;
		}
	}

	if(results.size() == 0) //  No hit
	{
		int stopper = 0;
		x = resultx;
		y = resulty;

	}
	else
	{
		x = finalx;
		y = finaly;
		resultx = finalx;
		resulty = finaly;
	}

 	if(x > m_CellWidth - 1) x = m_CellWidth - 1;
 	if(x < 0) x = 0;
 
 	if(y > m_CellHeight - 1) y = m_CellHeight - 1;
 	if(y < 0) y = 0;

}

void Terrain::GetTerrainHitWithUV(double &x, double &y, double &u, double &v, int &whichTriangle)
{
//    static int resultx = 0;
//    static int resulty = 0;
    //  First things first; we must compute the picking ray.  We know the origin
    //  of the picking ray; it's the current position of the camera.  We need
    //  to turn the 2D mouse coordinates into a 3D point that will represent the
    //  end of the ray.  Since we can get the current mouse coordinates straight
    //  from the Input subsystem and already know our projection matrix, we
    //  don't need any additional information to get the ray.

    //  We turn our 2D coordinates into 3D coordinates by doing an inverse of
    //  the projection transform.  The projection transform turns 3D points into
    //  2D screen space points; we need to do the exact opposite.  We already
    //  know our z-coordinate; the projection matrix always projects points
    //  such that z is equal to 1.
    float _ProjectedX;
    float _ProjectedY;
    float _ProjectedZ = 1.0f;

    D3DXMATRIX _Proj; // The inverse 
    gp_Display->m_D3DDevice.GetTransform(D3DTS_PROJECTION, &_Proj);

    _ProjectedX = (( float( 2.0f * gp_Input->m_MouseX) / float(gp_Display->m_HRes) ) - 1.0f) / _Proj(0, 0);
    _ProjectedY = (( float(-2.0f * gp_Input->m_MouseY) / float(gp_Display->m_VRes) ) + 1.0f) / _Proj(1, 1);

    D3DXVECTOR3 _RayOrigin = D3DXVECTOR3(0, 0, 0);
    D3DXVECTOR3 _RayDirection = D3DXVECTOR3(_ProjectedX, _ProjectedY, _ProjectedZ);
    D3DXVec3Normalize(&_RayDirection, &_RayDirection);

    //  Woohoo, we've got our ray!  But we've got a problem.  This ray is in VIEW
    //  SPACE.  We need to get the ray into WORLD SPACE.  We do that by finding
    //  the inverse of the view transformation and applying it to both the location
    //  and the direction of the ray.

    D3DXMATRIX _correctForCamera;
    gp_Display->m_D3DDevice.GetTransform(D3DTS_VIEW, &_correctForCamera);
    D3DXMatrixInverse(&_correctForCamera, 0, &_correctForCamera);
    D3DXVec3TransformCoord(&_RayOrigin, &_RayOrigin, &_correctForCamera);
    D3DXVec3TransformNormal(&_RayDirection, &_RayDirection, &_correctForCamera);

    //  Woohoo, we've got our ray!  But we've got a problem.  This ray is in
    //  WORLD SPACE.  The triangle we're testing against is in its own
    //  MODEL SPACE.  One is going to have to get transformed into the other
    //  before we can compare the two.  Now, once upon a time we would have had
    //  to transform our model's vertices into world space by hand on every
    //  frame, thus we would have had a handy copy of our model in world space
    //  to use.  Since D3D does all the transformation for us, we have no handy
    //  copy and the only geometry we have is in model space.  Thus, we will
    //  transform the ray into the model space.  We do this by tranforming it
    //  by the INVERSE of the model's current transformation matrix (which we
    //  must create and store ourselves, so we do have access to it).

    //	D3DXMATRIX _Inv;

    //	D3DXMatrixInverse(&_Inv, 0, &m_CurrentTransform);

    //  All right, we apply this matrix to our ray.

    //	D3DXVec3TransformCoord(&_RayOrigin, &_RayOrigin, &_Inv);
    //	D3DXVec3TransformNormal(&_RayDirection, &_RayDirection, &_Inv);




    //  Find out what square the mouse is in, we're going to highlight it.
    //  If we didn't click on a unit, see if we clicked on the terrain.
    D3DXMATRIX _Identity;
    D3DXMatrixIdentity(&_Identity);
    int i, j;
    struct result{double x; double y; double distance; double u; double v; int whichTriangle; };
    vector<result> results; 

    int starti = gp_Display->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
    if(starti < 0) starti = 0;
    int stopi = gp_Display->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
    if(stopi > m_CellWidth - 1) stopi = m_CellWidth - 1;

    int startj = gp_Display->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
    if(startj < 0) startj = 0;
    int stopj = gp_Display->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
    if(stopj > m_CellWidth - 1) stopj = m_CellWidth - 1;

    for(int i = starti; i <= stopi; ++i)
    {
        for(int j = startj; j < stopj; ++j)
        {
            double distance, u, v;
            if(gp_Display->PickWithUV(_RayOrigin, _RayDirection, 
                D3DXVECTOR3(i    , GetValue(i    , j    ), j    ),
                D3DXVECTOR3(i + 1, GetValue(i + 1, j    ), j    ),
                D3DXVECTOR3(i    , GetValue(i    , j + 1), j + 1), _Identity, distance, u, v) )
			{
				result temp = {i, j, distance, u, v, 0};
                results.push_back(temp);
			}
			else if(gp_Display->PickWithUV(_RayOrigin, _RayDirection,
                D3DXVECTOR3(i + 1, GetValue(i + 1, j + 1), j + 1),
                D3DXVECTOR3(i    , GetValue(i    , j + 1), j + 1),
                D3DXVECTOR3(i + 1, GetValue(i + 1, j    ), j    ), _Identity, distance, u, v) )
            {
				result temp = {i, j, distance, u, v, 1};
                results.push_back(temp);
            }
        }
    }

    if(results.size() == 0) //  No hit
    {
        x = 0;
        y = 0;
        u = 0;
        v = 0;
		whichTriangle = 0;

        return;
    }

    //  Got our results, sort the vector and set the targetX and targetY
    double finaldistance = 99999;
    vector<result>::iterator node = results.begin();
    vector<result>::iterator finalResult;
    for(node; node != results.end(); ++node)
    {
        if((*node).distance < finaldistance)
        {
            finalResult = node;
            finaldistance = (*node).distance;
        }
    }

    _RayOrigin += ( _RayDirection * (*finalResult).distance );

    x = _RayOrigin.x;
    y = _RayOrigin.z;
}


void Terrain::AddHill()
{
	// pick a size for the hill
	float fRadius = g_VitalRNG.RandomRange(1, 8);

	// pick a centerpoint for the hill
	int x = g_VitalRNG.Random(m_VertexWidth);
	int y = g_VitalRNG.Random(m_VertexHeight);


	for(float i = -fRadius; i <= fRadius; ++i)
	{
		for(float j = -sqrt(fRadius*fRadius-i*i); j <= sqrt(fRadius*fRadius-i*i); ++j)
		{
			float value = fRadius * fRadius - i * i - j * j;
			if(value < 0) value = 0;

			if(x + i >= 0 && x + i < m_VertexWidth && y + j >= 0 && y + j < m_VertexHeight)
			{
				OffsetValue(x + i, y + j, sqrt(value));
			}
		}
	}
}

void Terrain::AddHill(float x, float y, float height)
{
	// pick a size for the hill
	float fRadius = height;

	// pick a centerpoint for the hill
//	int x = x;
//	int y = y;


	for(float i = -fRadius; i <= fRadius; ++i)
	{
		for(float j = -sqrt(fRadius*fRadius-i*i); j <= sqrt(fRadius*fRadius-i*i); ++j)
		{
			float value = fRadius * fRadius - i * i - j * j;
			if(value < 0) value = 0;

			if(x + i >= 0 && x + i < m_VertexWidth && y + j >= 0 && y + j < m_VertexHeight)
			{
				OffsetValue(x + i, y + j, sqrt(value));
			}
		}
	}
}

void Terrain::OffsetValue(int x, int y, float height)
{
	SetValue(x, y, GetValue(x, y) + height);
}

float Terrain::GetMiddle(int x, int y)
{
	float _NumberOfValidEntries = 0;
	float _RunningTotal = 0; 

	if(x >= 0 && x < m_VertexWidth && y >= 0 && y < m_VertexHeight)
	{
		++_NumberOfValidEntries;
		_RunningTotal += GetValue(x, y);
	}

	if(x + 1 >= 0 && x + 1 < m_VertexWidth && y >= 0 && y < m_VertexHeight)
	{
		++_NumberOfValidEntries;
		_RunningTotal += GetValue(x + 1, y);
	}

	if(x + 1 >= 0 && x + 1 < m_VertexWidth && y + 1 >= 0 && y + 1 < m_VertexHeight)
	{
		++_NumberOfValidEntries;
		_RunningTotal += GetValue(x, y + 1);
	}

	if(x + 1 >= 0 && x + 1 < m_VertexWidth && y + 1 >= 0 && y + 1 < m_VertexHeight)
	{
		++_NumberOfValidEntries;
		_RunningTotal += GetValue(x + 1, y + 1);
	}

	return _RunningTotal / _NumberOfValidEntries;
}

void Terrain::PushUp(int x, int y, int numberofpushes)
{
	for(int i = 0; i < numberofpushes; ++i)
	{
		//  Make sure data point is valid.
		if(x >= 0 && x < m_VertexWidth && y >= 0 && y <= m_VertexHeight)
		{
			//  Make sure we're not already at highest height
			if(GetValue(x, y) < 8)
			{
				SetValue(x, y, GetValue(x, y) + 1);
			}

			for(int i = x - 1; i <= x + 1; ++i)
			{
				for(int j = y - 1; j <= y + 1; ++j)
				{
					if(x >= 0 && x < m_VertexWidth && y >= 0 && y <= m_VertexHeight)
					{
						if(GetValue(x, y) - GetValue(i, j) >= 2)
						{
							PushUp(i, j, 1);
						}
					}
				}
			}
		}
	}

	m_DoWeNeedToRecreateVertexBuffers = true;
}


void Terrain::PushDown(int x, int y, int numberofpushes)
{
	for(int i = 0; i < numberofpushes; ++i)
	{
		//  Make sure data point is valid.
		if(x >= 0 && x < m_VertexWidth && y >= 0 && y <= m_VertexHeight)
		{
			//  Make sure we're not already at highest height
			if(GetValue(x, y) > 0 )
			{
				SetValue(x, y, GetValue(x, y) - 1);
			}

			for(int i = x - 1; i <= x + 1; ++i)
			{
				for(int j = y - 1; j <= y + 1; ++j)
				{
					if(x >= 0 && x < m_VertexWidth && y >= 0 && y <= m_VertexHeight)
					{
						if(GetValue(i, j) - GetValue(x, y) >= 2)
						{
							PushDown(i, j, 1);
						}
					}
				}
			}
		}
	}
}

DWORD Terrain::ComputeLighting(int x, int y)
{
	D3DXVECTOR3 directionToLight(1, 1, 1);
	D3DXVec3Normalize(&directionToLight, &directionToLight);

	float heightA = GetValue(x, y);
	float heightB = GetValue(x + 1, y);
	float heightC = GetValue(x, y + 1);

	D3DXVECTOR3 u(1, heightB - heightA, 0.0f);
	D3DXVECTOR3 v(0, heightC - heightA, -1);

	D3DXVECTOR3 n;
	D3DXVec3Cross(&n, &u, &v);
	D3DXVec3Normalize(&n, &n);

	float cosine = D3DXVec3Dot(&n, &directionToLight);
	if(cosine < 0)
		cosine = 0;

	int lightComponent = (cosine * 127);
	int heightComponent = 128;// + (64 * (float(GetValue(x, y)) / 8.0f));
	int colorComponent = heightComponent + lightComponent;

	int alphacomponent = 255;

	if(GetValue(x, y) < .2f)
	{
		alphacomponent = 0;
	}

	else if(GetValue(x, y) > .5f)
	{
		alphacomponent = 255;
	}
	else
	{
		float alphamult = (GetValue(x, y) - .2f) * 3.333333333;
		alphacomponent = alphamult * 255;
	}

	return D3DCOLOR_ARGB(alphacomponent, colorComponent, colorComponent, colorComponent);
}


int Terrain::GetLightingComponent(int x, int y)
{
	D3DXVECTOR3 directionToLight(1, 1, 1);
	D3DXVec3Normalize(&directionToLight, &directionToLight);

	float heightA = GetValue(x, y);
	float heightB = GetValue(x + 1, y);
	float heightC = GetValue(x, y + 1);

	D3DXVECTOR3 u(1, heightB - heightA, 0.0f);
	D3DXVECTOR3 v(0, heightC - heightA, -1);

	D3DXVECTOR3 n;
	D3DXVec3Cross(&n, &u, &v);
	D3DXVec3Normalize(&n, &n);

	float cosine = D3DXVec3Dot(&n, &directionToLight);
	if(cosine < 0)
		cosine = 0;

	int lightComponent = (cosine * 127);
	int heightComponent = 128;// + (64 * (float(GetValue(x, y)) / 8.0f));
	int colorComponent = heightComponent + lightComponent;

	return colorComponent;
}

int Terrain::GetAlphaComponent(int x, int y)
{
	int alphacomponent = 255;

	if(GetValue(x, y) < .2f)
	{
		alphacomponent = 0;
	}

	else if(GetValue(x, y) > .5f)
	{
		alphacomponent = 255;
	}
	else
	{
		float alphamult = (GetValue(x, y) - .2f) * 3.333333333;
		alphacomponent = alphamult * 255;
	}

	return alphacomponent;
}

int Terrain::GetMiddleAlphaComponent(int x, int y)
{
	return(GetAlphaComponent(x , y) + GetAlphaComponent(x + 1, y) +
		GetAlphaComponent(x + 1, y + 1) + GetAlphaComponent(x , y + 1)) / 4;
}

int Terrain::FindTerrainType(int i, int j)
{

	//  OKAY LET'S GET THIS RIGHT.

	//  IF A CELL HAS ALL FOUR CORNERS ON THE FLOOR, IT'S SAND/WATER

	//  IF A CELL HAS ONE OR MORE CORNERS ON THE FLOOR BUT NOT ALL FOUR, IT'S BEACH.

	//  OTHERWISE, IT'S GRASS.

	if(i >= 0 && i < m_CellWidth && j >= 0 && j < m_CellHeight)
	{
		if(GetValue(i, j) == 0.0f
			&& GetValue(i + 1, j) == 0.0f
			&& GetValue(i + 1, j + 1) == 0.0f
			&& GetValue(i, j + 1) == 0.0f)
		{
			return TT_SAND;
		}

		if(GetValue(i, j) < 0.4f
			|| GetValue(i + 1, j) < 0.4f 
			|| GetValue(i + 1, j + 1) < 0.4f 
			|| GetValue(i, j + 1) < 0.4f )
		{
			return TT_BEACH;
		}

//		if(GetValue(i, j) > 0.2f || GetValue(i + 1, j) > 0.2f  || GetValue(i + 1, j + 1) > 0.2f || GetValue(i, j + 1) > 0.2f)
//		{
//			return TT_BEACH;
//		}

//		if(GetValue(i, j) < 1 || GetValue(i + 1, j) < 1  || GetValue(i + 1, j + 1) < 1 || GetValue(i, j + 1) < 1)
//		{
			return TT_GRASS;
//		}

//		else
//		{
			//if(m_TerrainTypes[i + j * m_CellWidth] == -1 || m_TerrainTypes[i + j * m_CellWidth] == TT_WATER || m_TerrainTypes[i + j * m_CellWidth] == TT_BEACH)
//				return TT_GRASS;
//		}

	}
	else
		return 0;
}

int Terrain::GetTerrainType(int x, int y)
{
	if(x >= 0 && x < m_CellWidth && y >= 0 && y < m_CellHeight)
		return m_TerrainTypes[x + y * m_CellWidth].m_TerrainType;
	else
		return -1;
}

DWORD Terrain::GetTerrainAdditionalData(int x, int y)
{
	if(x >= 0 && x < m_CellWidth && y >= 0 && y < m_CellHeight)
		return m_TerrainTypes[x + y * m_CellWidth].m_AdditionalData;
	else
		return 0;
}

void Terrain::SetTerrainType(int x, int y, int type)
{
	if(x >= 0 && x < m_CellWidth && y >= 0 && y < m_CellHeight)
		m_TerrainTypes[x + y * m_CellWidth].m_TerrainType = type;
}

void Terrain::SetTerrainAdditionalData(int x, int y, DWORD data)
{
	if(x >= 0 && x < m_CellWidth && y >= 0 && y < m_CellHeight)
		m_TerrainTypes[x + y * m_CellWidth].m_AdditionalData = data;
}

void Terrain::FindTerrainTypes()
{
	for(int i = 0; i < m_CellWidth; ++i)
	{
		for(int j = 0; j < m_CellHeight; ++j)
		{
			SetTerrainType(i, j, FindTerrainType(i, j));
		}
	}
}




void Terrain::DrawTerrain()
{
	gp_Display->m_D3DDevice.SetFVF(FVF);

	gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

	gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

	gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHABLENDENABLE, true);

	gp_Display->m_D3DDevice.SetStreamSource( 0, m_VertexBuffer, 0, sizeof(PlanitiaVertex));

	//  Simple terrain draw for debug purposes.
//	gp_Display->m_D3DDevice.SetTexture(0, m_GrassTexture->m_Bitmap);
//	gp_Display->m_D3DDevice.DrawPrimitive(D3DPT_TRIANGLELIST, 0, m_VertexBufferSize / 3);



	for(int i = 0; i < NUMBER_OF_VERTEX_BUFFERS; ++i)
	{
		if(m_IndexBufferSizes[i] > 0)
		{
			if(i == TT_WATER || i == TT_SAND || i == TT_BEACH)
			{
				gp_Display->m_D3DDevice.SetTexture(0, m_SandTexture->m_Bitmap);
			}
			else if(i == TT_RUINEDLAND || i == TT_BLESSEDLAND || i == TT_LAVA || i == TT_FARMLAND)
			{
				gp_Display->m_D3DDevice.SetTexture(0, m_GrassTexture->m_Bitmap);
			}
			else if(i == TT_GRASS)
			{
				gp_Display->m_D3DDevice.SetTexture(0, m_GrassTexture->m_Bitmap);
			}

			gp_Display->m_D3DDevice.SetIndices(m_IndexBuffer[i]);
			gp_Display->m_D3DDevice.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_VertexBufferSize, 0, m_IndexBufferSizes[i] / 3);
			m_NumberOfTrisDrawnThisFrame += m_IndexBufferSizes[i] / 3;
		}
	}

	//  Okay, if we're going to blend grass and sand then we need to do another pass over the sand with the grass
	//  texture.

	if(m_IndexBufferSizes[TT_BEACH] > 0)
	{
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE2X);

		gp_Display->m_D3DDevice.SetTexture(0, m_GrassTexture->m_Bitmap);
		gp_Display->m_D3DDevice.SetStreamSource( 0, m_VertexBuffer, 0, sizeof(PlanitiaVertex));
		gp_Display->m_D3DDevice.SetIndices(m_IndexBuffer[TT_BEACH]);
		gp_Display->m_D3DDevice.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_VertexBufferSize, 0, m_IndexBufferSizes[TT_BEACH] / 3);
		m_NumberOfTrisDrawnThisFrame += m_IndexBufferSizes[TT_BEACH] / 3;
	}

	if(m_IndexBufferSizes[TT_HOUSE] > 0)
	{
		gp_Display->m_D3DDevice.SetTexture(0, m_HouseTexture->m_Bitmap);

		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		gp_Display->m_D3DDevice.SetStreamSource( 0, m_VertexBuffer, 0, sizeof(PlanitiaVertex));
		gp_Display->m_D3DDevice.SetIndices(m_IndexBuffer[TT_HOUSE]);
		gp_Display->m_D3DDevice.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_VertexBufferSize, 0, m_IndexBufferSizes[TT_HOUSE] / 3);
		m_NumberOfTrisDrawnThisFrame += m_IndexBufferSizes[TT_HOUSE] / 3;
	}

	if(m_IndexBufferSizes[TT_FARMLAND] > 0)
	{
		gp_Display->m_D3DDevice.SetTexture(0, m_FarmTexture->m_Bitmap);

		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		gp_Display->m_D3DDevice.SetStreamSource( 0, m_VertexBuffer, 0, sizeof(PlanitiaVertex));
		gp_Display->m_D3DDevice.SetIndices(m_IndexBuffer[TT_FARMLAND]);
		gp_Display->m_D3DDevice.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_VertexBufferSize, 0, m_IndexBufferSizes[TT_FARMLAND] / 3);
		m_NumberOfTrisDrawnThisFrame += m_IndexBufferSizes[TT_FARMLAND] / 3;
	}





	if(m_IndexBufferSizes[TT_LAVA] > 0)
	{
		gp_Display->m_D3DDevice.SetTexture(1, m_MaskTexture->m_Bitmap);
		gp_Display->m_D3DDevice.SetTexture(0, m_LavaTexture->m_Bitmap);

		int startingvalue = gp_Engine->m_GameTimeInMS % 1024;
		startingvalue /= 16;
		if(startingvalue > 32) startingvalue = 64 - startingvalue;
		startingvalue += 192;

		gp_Display->m_D3DDevice.SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_XRGB(startingvalue, startingvalue, startingvalue));


		D3DXMATRIX matTrans;
		D3DXMatrixIdentity(&matTrans);
		matTrans._31 = gp_Engine->m_GameTimeInSeconds / 8;
		matTrans._32 = gp_Engine->m_GameTimeInSeconds / 8;

		// Set up the matrix for the desired transformation.
		gp_Display->m_D3DDevice.SetTransform( D3DTS_TEXTURE0, &matTrans );

		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);


		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);


//		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU);  //  Use the previous stage's UV coordinates



		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		gp_Display->m_D3DDevice.SetStreamSource( 0, m_VertexBuffer, 0, sizeof(PlanitiaVertex));
		gp_Display->m_D3DDevice.SetIndices(m_IndexBuffer[TT_LAVA]);
		gp_Display->m_D3DDevice.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_VertexBufferSize, 0, m_IndexBufferSizes[TT_LAVA] / 3);
		m_NumberOfTrisDrawnThisFrame += m_IndexBufferSizes[TT_LAVA] / 3;

		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	}

	if(m_IndexBufferSizes[TT_BLESSEDLAND] > 0)
	{
		gp_Display->m_D3DDevice.SetTexture(0, m_BlessTexture->m_Bitmap);
		gp_Display->m_D3DDevice.SetTexture(1, m_MaskTexture->m_Bitmap);

		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);

		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		

		gp_Display->m_D3DDevice.SetStreamSource( 0, m_VertexBuffer, 0, sizeof(PlanitiaVertex));
		gp_Display->m_D3DDevice.SetIndices(m_IndexBuffer[TT_BLESSEDLAND]);
		gp_Display->m_D3DDevice.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_VertexBufferSize, 0, m_IndexBufferSizes[TT_BLESSEDLAND] / 3);
		m_NumberOfTrisDrawnThisFrame += m_IndexBufferSizes[TT_BLESSEDLAND] / 3;

		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	}

	if(m_IndexBufferSizes[TT_RUINEDLAND] > 0)
	{
		gp_Display->m_D3DDevice.SetTexture(0, m_RuinTexture->m_Bitmap);
		gp_Display->m_D3DDevice.SetTexture(1, m_MaskTexture->m_Bitmap);

		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		gp_Display->m_D3DDevice.SetStreamSource( 0, m_VertexBuffer, 0, sizeof(PlanitiaVertex));
		gp_Display->m_D3DDevice.SetIndices(m_IndexBuffer[TT_RUINEDLAND]);
		gp_Display->m_D3DDevice.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_VertexBufferSize, 0, m_IndexBufferSizes[TT_RUINEDLAND] / 3);
		m_NumberOfTrisDrawnThisFrame += m_IndexBufferSizes[TT_RUINEDLAND] / 3;

		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	}

	if(m_IndexBufferSizes[TT_SWAMP] > 0)
	{
		gp_Display->m_D3DDevice.SetTexture(1, m_MaskTexture->m_Bitmap);
		gp_Display->m_D3DDevice.SetTexture(0, m_SwampTexture->m_Bitmap);

// 		int startingvalue = gp_Engine->m_GameTimeInMS % 1024;
// 		startingvalue /= 16;
// 		if(startingvalue > 32) startingvalue = 64 - startingvalue;
// 		startingvalue += 192;
// 
// 		gp_Display->m_D3DDevice.SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_XRGB(startingvalue, startingvalue, startingvalue));


		D3DXMATRIX matTrans;
		D3DXMatrixIdentity(&matTrans);
		matTrans._31 = gp_Engine->m_GameTimeInSeconds / 8;
		matTrans._32 = gp_Engine->m_GameTimeInSeconds / 8;

		// Set up the matrix for the desired transformation.
		gp_Display->m_D3DDevice.SetTransform( D3DTS_TEXTURE0, &matTrans );

		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);


		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);


		//		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_PASSTHRU);  //  Use the previous stage's UV coordinates



		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		gp_Display->m_D3DDevice.SetStreamSource( 0, m_VertexBuffer, 0, sizeof(PlanitiaVertex));
		gp_Display->m_D3DDevice.SetIndices(m_IndexBuffer[TT_SWAMP]);
		gp_Display->m_D3DDevice.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_VertexBufferSize, 0, m_IndexBufferSizes[TT_SWAMP] / 3);
		m_NumberOfTrisDrawnThisFrame += m_IndexBufferSizes[TT_SWAMP] / 3;

		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		gp_Display->m_D3DDevice.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	}

	//  Wireframe over selection, for debug purposes


	gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHABLENDENABLE, false);
}

void Terrain::DrawWater()
{
	//  Draw the water

	if(m_WaterIndexBufferSize > 0)
	{
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

		gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHABLENDENABLE, true);
		gp_Display->m_D3DDevice.SetFVF(FVF);
		gp_Display->m_D3DDevice.SetTexture(0, m_DeepWaterTexture->m_Bitmap);
		gp_Display->m_D3DDevice.SetStreamSource( 0, m_WaterVertexBuffer, 0, sizeof(PlanitiaVertex));
		gp_Display->m_D3DDevice.SetIndices(m_WaterIndexBuffer);
		gp_Display->m_D3DDevice.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_WaterVertexBufferSize, 0, m_WaterIndexBufferSize / 3);
		m_NumberOfTrisDrawnThisFrame += m_WaterIndexBufferSize / 3;
		//gp_Display->m_D3DDevice.DrawPrimitive(D3DPT_TRIANGLELIST, 0, m_WaterVertexBufferSize / 3);

		gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHABLENDENABLE, false);
	}
}

void Terrain::UpdateMiniMap()
{
	if (!m_MiniMapTexture || !m_MiniMapTexture->m_Bitmap) return;
	Image img = LoadImageFromTexture(*m_MiniMapTexture->m_Bitmap);
	for (int i = 0; i < m_CellWidth; ++i)
	{
		for (int j = 0; j < m_CellHeight; ++j)
		{
			Color c = BLACK;
			if (GetTerrainType(i, j) == TT_GRASS)
				c = {78, 161, 53, 255};
			else if (GetTerrainType(i, j) == TT_BEACH)
				c = GetAlphaComponent(i, j) < 128 ? Color{219, 198, 148, 255} : Color{78, 161, 53, 255};
			else if (GetTerrainType(i, j) == TT_WATER || GetTerrainType(i, j) == TT_SAND)
				c = {69, 98, 144, 255};
			ImageDrawPixel(&img, j, i, c);
		}
	}
	Texture2D updated = LoadTextureFromImage(img);
	UnloadTexture(*m_MiniMapTexture->m_Bitmap);
	*m_MiniMapTexture->m_Bitmap = updated;
	UnloadImage(img);
}

bool Terrain::IsValidVillageTerrain(int x, int y)
{
	if(x < 0 || x > m_CellWidth || y < 0 || y > m_CellHeight) return false;

	float target = GetValue(x, y);
	return( (GetValue(x + 1, y + 1) == target)
		&& (GetValue(x, y + 1) == target)
		&& (GetValue(x + 1, y) == target)
		&& (GetMiddle(x, y) == target)
		&& (GetMiddle(x, y) > 1)
		);
}

bool Terrain::IsCellVisible(int x, int y)
{
	return IsPointVisible(x, y);
/*	return(IsPointVisible(x, y)
		|| IsPointVisible(x + 1, y)
		|| IsPointVisible(x + 1, y + 1)
		|| IsPointVisible(x, y + 1)
		);*/
}

bool Terrain::IsPointVisible(int x, int y)
{
	//  Create vertex.

	D3DXVECTOR3 originalPoint = D3DXVECTOR3(x + .5, GetMiddle(x, y), y + .5);

	//  Apply camera transform to it.

	D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
	D3DXMATRIX V;
	V = gp_Display->m_CurrentCamera;

	D3DXMATRIX W;
	D3DXMatrixIdentity(&W);

	D3DXMATRIX P;
	gp_Display->m_D3DDevice.GetTransform(D3DTS_PROJECTION, &P);

	D3DVIEWPORT9 VP;
	gp_Display->m_D3DDevice.GetViewport(&VP);

	D3DXVec3Project(&originalPoint, &originalPoint, &VP, &P, &V, &W);

	if(originalPoint.x <= (gp_Display->m_HRes + (gp_Display->m_HRes * .2f))
		&& originalPoint.x >= (0 - (gp_Display->m_HRes * .2f))
		&& originalPoint.y <= (gp_Display->m_VRes + (gp_Display->m_VRes * .2f))
		&& originalPoint.y >= (0 - (gp_Display->m_VRes * .2f)))
		return true;
	else
		return false;
}

void Terrain::ScrubTerrainCell(int i, int j)
{
	for(int x = i - 1; x <= i + 1; ++x)
	{
		for(int y = j - 1; y <= j + 1; ++y)
		{
			if(x >= 0 && x < m_CellWidth && y >= 0 && y < m_CellHeight)
			{
				if(GetTerrainType(x, y) != TT_FARMLAND && GetTerrainType(x, y) != TT_HOUSE)
					SetTerrainType(x, y, FindTerrainType(x, y));
			}
		}
	}
}

bool Terrain::IsWaterCell(int i, int j)
{
	if(i >= 0 && i < m_CellWidth && j >= 0 && j < m_CellHeight)
	{
		if(GetValue(i, j) < 0.2f
			|| GetValue(i + 1, j) < 0.2f
			|| GetValue(i + 1, j + 1) < 0.2f
			|| GetValue(i, j + 1) < 0.2f)
		{
			return true;
		}
	}
	return false;
}

void Terrain::DrawHighlightMarker()
{
	if(m_ShowTerrainHit)
	{
        gp_Display->m_D3DDevice.SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, F2DW(-.001f) );
        gp_Display->m_D3DDevice.SetRenderState( D3DRS_DEPTHBIAS, F2DW(-.001f) );


		gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHABLENDENABLE, true);
        gp_Display->m_D3DDevice.SetRenderState(D3DRS_ZENABLE, false);

		gp_Display->m_D3DDevice.SetRenderState(D3DRS_TEXTUREFACTOR, m_TerrainHitColor);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

		gp_Display->m_D3DDevice.SetTexture(0, m_TerrainHighlightRing->m_Bitmap);

		gp_Display->m_D3DDevice.SetStreamSource( 0, m_HighlightVertexBuffer, 0, sizeof(PlanitiaVertex));
		gp_Display->m_D3DDevice.DrawPrimitive(D3DPT_TRIANGLELIST, 0, 4);

		gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHABLENDENABLE, false);

		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        
        gp_Display->m_D3DDevice.SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, F2DW(0.0f) );
        gp_Display->m_D3DDevice.SetRenderState( D3DRS_DEPTHBIAS, F2DW(0.0) );
        gp_Display->m_D3DDevice.SetRenderState(D3DRS_ZENABLE, true);

	}

    for( int i = 0; i < 4; ++i )
    {
        if( g_Players[i] != NULL )
        {
            if( g_Players[i]->m_General != NULL )
            {
                if( g_Players[i]->m_General->m_Selected )
                {

                    gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHABLENDENABLE, true);

                    gp_Display->m_D3DDevice.SetRenderState(D3DRS_TEXTUREFACTOR, m_TerrainHitColor);
                    gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                    gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);

                    gp_Display->m_D3DDevice.SetTexture(0, m_TerrainHighlightRing->m_Bitmap);

                    gp_Display->m_D3DDevice.SetStreamSource( 0, m_ColoredHighlightVertexBuffer[i], 0, sizeof(PlanitiaVertex));
                    gp_Display->m_D3DDevice.DrawPrimitive(D3DPT_TRIANGLELIST, 0, 4);

                    gp_Display->m_D3DDevice.SetRenderState(D3DRS_ALPHABLENDENABLE, false);

                    gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    gp_Display->m_D3DDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                }
            }
        }
    }
}

void Terrain::RuinAllLand()
{
	for(int i = 0; i < m_CellWidth; ++i)
	{
		for(int j = 0; j < m_CellHeight; ++j)
		{
			if(GetTerrainType(i, j) == TT_GRASS)
			{
				SetTerrainType(i, j, TT_RUINEDLAND);
			}
		}
	}
}