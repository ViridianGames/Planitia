#include "PlanitiaGlobals.h"
#include "PlanitiaScene.h"

#include "../Geist/Source/Config.h"
#include "../Geist/Source/Globals.h"
#include "../Geist/Source/ResourceManager.h"
#include "PlanitiaMeshCache.h"
#include "PlanitiaTypes.h"
#include "PlanitiaInput.h"
#include "PlanitiaEngineAdapter.h"
#include "Terrain.h"

#include "PlanitiaProfile.h"

#include "raymath.h"
#include "rlgl.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>

using namespace std;

namespace {

Mesh BuildMeshFromVerticesAndIndices(const vector<Vertex>& vertices, const vector<unsigned short>& indices)
{
	Mesh mesh{};
	const int vertexCount = static_cast<int>(vertices.size());
	const int indexCount = static_cast<int>(indices.size());
	mesh.vertexCount = vertexCount;
	mesh.triangleCount = indexCount / 3;

	if (vertexCount > 0)
	{
		mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(vertexCount * 3 * sizeof(float))));
		mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(vertexCount * 2 * sizeof(float))));
		mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(vertexCount * 4)));

		for (int i = 0; i < vertexCount; ++i)
		{
			const Vertex& v = vertices[static_cast<size_t>(i)];
			mesh.vertices[i * 3 + 0] = v.x;
			mesh.vertices[i * 3 + 1] = v.y;
			mesh.vertices[i * 3 + 2] = v.z;
			mesh.texcoords[i * 2 + 0] = v.u;
			mesh.texcoords[i * 2 + 1] = v.v;
			mesh.colors[i * 4 + 0] = static_cast<unsigned char>(v.r);
			mesh.colors[i * 4 + 1] = static_cast<unsigned char>(v.g);
			mesh.colors[i * 4 + 2] = static_cast<unsigned char>(v.b);
			mesh.colors[i * 4 + 3] = static_cast<unsigned char>(v.a);
		}
	}

	if (indexCount > 0)
	{
		mesh.indices = static_cast<unsigned short*>(
			MemAlloc(static_cast<unsigned int>(indexCount * sizeof(unsigned short))));
		memcpy(mesh.indices, indices.data(), static_cast<size_t>(indexCount) * sizeof(unsigned short));
	}

	UploadMesh(&mesh, true);
	return mesh;
}

Mesh BuildMeshFromTriangleList(const vector<Vertex>& vertices)
{
	vector<unsigned short> indices(vertices.size());
	for (size_t i = 0; i < vertices.size(); ++i)
		indices[i] = static_cast<unsigned short>(i);
	return BuildMeshFromVerticesAndIndices(vertices, indices);
}

void DrawTerrainModel(TerrainDrawMesh& drawMesh, Texture* texture, Color tint = WHITE)
{
	if (!drawMesh.loaded || drawMesh.triangleCount <= 0 || !texture)
		return;

	SetMaterialTexture(&drawMesh.model.materials[0], MATERIAL_MAP_DIFFUSE, *texture);
	DrawModel(drawMesh.model, Vector3{0, 0, 0}, 1.0f, tint);
}

Model BuildBackgroundModel(const vector<Vertex>& vertices)
{
	vector<unsigned short> indices;
	if (vertices.size() >= 4)
	{
		indices = {0, 1, 2, 1, 2, 3};
	}
	else if (vertices.size() >= 3)
	{
		indices = {0, 1, 2};
	}

	Mesh mesh = BuildMeshFromVerticesAndIndices(vertices, indices);
	return LoadModelFromMesh(mesh);
}

} // namespace

void Terrain::UnloadDrawMesh(TerrainDrawMesh& drawMesh)
{
	if (drawMesh.loaded)
	{
		UnloadModel(drawMesh.model);
		drawMesh.loaded = false;
	}
	drawMesh.triangleCount = 0;
}

void Terrain::RebuildTypeMesh(int type, const vector<unsigned short>& indices)
{
	UnloadDrawMesh(m_TypeMeshes[type]);
	m_IndexBufferSizes[type] = static_cast<int>(indices.size());

	if (indices.empty())
		return;

	Mesh mesh = BuildMeshFromVerticesAndIndices(m_Vertices, indices);
	m_TypeMeshes[type].model = LoadModelFromMesh(mesh);
	m_TypeMeshes[type].loaded = true;
	m_TypeMeshes[type].triangleCount = static_cast<int>(indices.size()) / 3;
}

void Terrain::RebuildMeshFromVertices(TerrainDrawMesh& drawMesh, const vector<Vertex>& vertices)
{
	UnloadDrawMesh(drawMesh);
	if (vertices.empty())
		return;

	Mesh mesh = BuildMeshFromTriangleList(vertices);
	drawMesh.model = LoadModelFromMesh(mesh);
	drawMesh.loaded = true;
	drawMesh.triangleCount = static_cast<int>(vertices.size()) / 3;
}

Background::~Background()
{
   Shutdown();
}

void Background::Init(const std::string& configfile)
{
   (void)configfile;
   m_IsDead = false;
   LoadedMesh* meshData = GetLoadedMesh("Data/Meshes/standard.txt");
   m_Background = g_ResourceManager->GetTexture(NormalizePath("Images/clouds.png"));
   if (meshData && !meshData->m_VertexList.empty())
   {
      m_Model = BuildBackgroundModel(meshData->m_VertexList);
      m_ModelLoaded = true;
   }
}

void Background::Shutdown()
{
   if (m_ModelLoaded)
   {
      UnloadModel(m_Model);
      m_ModelLoaded = false;
   }
}

void Background::Update()
{

}

void Background::Draw()
{
   if (!m_ModelLoaded || !m_Background)
      return;

   SetMaterialTexture(&m_Model.materials[0], MATERIAL_MAP_DIFFUSE, *m_Background);
   DrawModel(m_Model, Vector3{0, 0, 0}, 1.0f, WHITE);
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

	m_UnitConfig.Load(configfile);

	m_IsDead = false;

	m_ShowTerrainHit = false;

	m_CellWidth = m_UnitConfig.GetNumber("width");
	m_CellHeight = m_UnitConfig.GetNumber("height");

	m_VertexWidth = m_CellWidth + 1;
	m_VertexHeight = m_CellHeight + 1;

	m_Values = new float[m_VertexWidth * m_VertexHeight];
	m_TerrainTypes = new TerrainCell[m_CellWidth * m_CellHeight];

	m_WaterHeight = 1.0f;

	ifstream instream;
	string filename = m_UnitConfig.GetString("map");

	

	char tempchar = 0;
	uint8_t actualvalue;

	m_DoWeNeedToRecreateVertexBuffers = true;
	m_DoWeNeedToRecreateIndexBuffers = true;

	memset(m_Values, 0, m_VertexWidth * m_VertexHeight * sizeof(float));

	InitializeMap(m_UnitConfig.GetNumber("seed"));
//	gp_Scene->m_Camera.m_LookAtPoint.x = m_VertexWidth / 2;
//	gp_Scene->m_Camera.m_LookAtPoint.z = m_VertexHeight / 2;
//	gp_Scene->m_Camera.m_LookAtPoint.y = GetHeight(m_VertexWidth / 2, m_VertexHeight / 2);

//	instream.close();

//	m_Texture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("texture")));

	m_ShallowWaterTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("shallowwatertexture")));

	m_DeepWaterTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("deepwatertexture")));

	m_SandTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("sandtexture")));

	m_GrassTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("grasstexture")));

	m_RuinTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("ruintexture")));

	m_MaskTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("masktexture")));

	m_BlessTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("blesstexture")));

	m_LavaTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("lavatexture")));

	m_FarmTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("farmtexture")));

	m_HouseTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("housetexture")));

	m_SwampTexture = g_ResourceManager->GetTexture(NormalizePath(m_UnitConfig.GetString("swamptexture")));

	m_TerrainHighlightRing = g_ResourceManager->GetTexture(NormalizePath("Images/TerrainHighlightRing.png"));

	//  Since we are constructing this one ourselves, we will "own" this bitmap rather than the
	//  resource manager (which only handles loaded files).

	
	
	m_MiniMapTexture = new Texture2D;
	Image minimapImage = GenImageColor(m_CellWidth, m_CellHeight, BLACK);
	*m_MiniMapTexture = LoadTextureFromImage(minimapImage);
	UnloadImage(minimapImage);
	
	FindTerrainTypes();
	UpdateMiniMap();

	m_TerrainViewRange = 25;


	if(gp_Scene->m_HardwareVertexProcessingSupported)
		m_VertexBufferSize = m_CellHeight * m_CellWidth * 5;
	else
		m_VertexBufferSize = m_CellHeight * m_CellWidth * 4;

	m_Vertices.resize(static_cast<size_t>(m_VertexBufferSize));

	m_TerrainSwitcher = true;

	m_NumberOfTrisDrawnThisFrame = 0;

	m_DidWeRecreateVertexBuffersLastFrame = false;

	g_NumberOfPlayers = m_UnitConfig.GetNumber("players");

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

	if(m_DoWeNeedToRecreateVertexBuffers || gp_Scene->m_Camera.m_DidCameraChangeThisFrame)
	{
		CreateVertexBuffers(m_DoWeNeedToRecreateVertexBuffers);
		m_DidWeRecreateVertexBuffersLastFrame = true;
	}
	else
	{
		m_DidWeRecreateVertexBuffersLastFrame = false;
	}

	CreateWaterVertexBuffer();
	CreateHighlightVertexBuffer();

	if(m_DoWeNeedToRecreateIndexBuffers || gp_Scene->m_Camera.m_DidCameraChangeThisFrame)
	{
		CreateIndexBuffers();
	}

	if(_DoWeNeedToUpdateMinimap)
	{
		UpdateMiniMap();
	}

   

   
}

void Terrain::Shutdown()
{
	delete [] m_Values;
	delete [] m_TerrainTypes;

	for (int i = 0; i < NUMBER_OF_VERTEX_BUFFERS; ++i)
		UnloadDrawMesh(m_TypeMeshes[i]);
	UnloadDrawMesh(m_WaterMesh);
	UnloadDrawMesh(m_HighlightMesh);
	for (int i = 0; i < 4; ++i)
		UnloadDrawMesh(m_ColoredHighlightMeshes[i]);
	m_Vertices.clear();
	m_WaterVertices.clear();
	if (m_MiniMapTexture)
	{
		UnloadTexture(*m_MiniMapTexture);
		delete m_MiniMapTexture;
		m_MiniMapTexture = nullptr;
	}
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

	vector<Vertex> highlightVertices(12);

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
    

	highlightVertices[0] =  MakeTerrainVertex( x     , GetHeight (x     , y      ), y     , lighting,  corners[0].x, corners[0].z  );
	highlightVertices[1] =  MakeTerrainVertex( x +  1, GetHeight (x +  1, y      ), y     , lighting,  corners[1].x, corners[1].z  );
	highlightVertices[2] =  MakeTerrainVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,                .5,        .5 );
	highlightVertices[3] =  MakeTerrainVertex( x +  1, GetHeight (x +  1, y      ), y     , lighting,  corners[1].x, corners[1].z  );
	highlightVertices[4] =  MakeTerrainVertex( x +  1, GetHeight (x +  1, y +  1 ), y +  1, lighting,  corners[2].x, corners[2].z  );
	highlightVertices[5] =  MakeTerrainVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,                .5, .5 );
	highlightVertices[6] =  MakeTerrainVertex( x +  1, GetHeight (x +  1, y +  1 ), y +  1, lighting,  corners[2].x, corners[2].z  );
	highlightVertices[7] =  MakeTerrainVertex( x     , GetHeight (x     , y +  1 ), y +  1, lighting,  corners[3].x, corners[3].z  );
	highlightVertices[8] =  MakeTerrainVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,  .5, .5 );
	highlightVertices[9] =  MakeTerrainVertex( x     , GetHeight (x     , y +  1 ), y +  1, lighting,  corners[3].x, corners[3].z  );
	highlightVertices[10] = MakeTerrainVertex( x     , GetHeight (x     , y      ), y     , lighting,  corners[0].x, corners[0].z  );
	highlightVertices[11] = MakeTerrainVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,  .5, .5 );

	RebuildMeshFromVertices(m_HighlightMesh, highlightVertices);

    for( int i = 0; i < 4; ++i )
    {
        if( g_Players[i] != NULL )
        {
            if( g_Players[i]->m_General != NULL )
            {
                vector<Vertex> playerHighlightVertices(12);

                float x = g_Players[i]->m_General->m_Pos.x - .5;
                float y = g_Players[i]->m_General->m_Pos.z - .5;

                DWORD lighting = D3DCOLOR_ARGB(255, 255, 255, 255);

                playerHighlightVertices[0] =  MakeTerrainVertex( x     , GetHeight (x     , y      ), y     , lighting,  0,         0         );
                playerHighlightVertices[1] =  MakeTerrainVertex( x +  1, GetHeight (x +  1, y      ), y     , lighting,  0,         1  );
                playerHighlightVertices[2] =  MakeTerrainVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,                .5,        .5 );
                playerHighlightVertices[3] =  MakeTerrainVertex( x +  1, GetHeight (x +  1, y      ), y     , lighting,  0,         1  );
                playerHighlightVertices[4] =  MakeTerrainVertex( x +  1, GetHeight (x +  1, y +  1 ), y +  1, lighting,  1 , 1  );
                playerHighlightVertices[5] =  MakeTerrainVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,                .5, .5 );
                playerHighlightVertices[6] =  MakeTerrainVertex( x +  1, GetHeight (x +  1, y +  1 ), y +  1, lighting,  1 , 1  );
                playerHighlightVertices[7] =  MakeTerrainVertex( x     , GetHeight (x     , y +  1 ), y +  1, lighting,  1 , 0         );
                playerHighlightVertices[8] =  MakeTerrainVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,  .5, .5 );
                playerHighlightVertices[9] =  MakeTerrainVertex( x     , GetHeight (x     , y +  1 ), y +  1, lighting,  1 , 0         );
                playerHighlightVertices[10] = MakeTerrainVertex( x     , GetHeight (x     , y      ), y     , lighting,  0       , 0         );
                playerHighlightVertices[11] = MakeTerrainVertex( x + .5, GetHeight (x + .5, y + .5 ), y + .5, lighting,  .5, .5 );

                RebuildMeshFromVertices(m_ColoredHighlightMeshes[i], playerHighlightVertices);
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
	int starti = gp_Scene->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
	if(starti < 0) starti = 0;
	int stopi = gp_Scene->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
	if(stopi > m_CellWidth - 1) stopi = m_CellWidth - 1;

	int startj = gp_Scene->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
	if(startj < 0) startj = 0;
	int stopj = gp_Scene->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
	if(stopj > m_CellHeight - 1) stopj = m_CellHeight - 1;

	for(int i = starti; i <= stopi; ++i)
	{
		for(int j = startj; j <= stopj; ++j)
		{
			if(GlobalIsDistanceLessThan(i, j, gp_Scene->m_Camera.m_LookAtPoint.x,
				gp_Scene->m_Camera.m_LookAtPoint.z, m_TerrainViewRange))
			{
				int TerrainType = GetTerrainType(i, j);

				if(TerrainType != -1) //  We ran off the grid somehow
				{
					if(gp_Scene->m_HardwareVertexProcessingSupported)
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
		vector<unsigned short> indices(_IndexBuckets[i].begin(), _IndexBuckets[i].end());
		RebuildTypeMesh(i, indices);
	}

	m_DoWeNeedToRecreateIndexBuffers = false;
}

void Terrain::CreateVertexBuffers(bool rebuildall)
{
	if (static_cast<int>(m_Vertices.size()) != m_VertexBufferSize)
		m_Vertices.resize(static_cast<size_t>(m_VertexBufferSize));

	Vertex* _Vertices = m_Vertices.data();

	int starti = gp_Scene->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
	if(starti < 0) starti = 0;
	int stopi = gp_Scene->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
	if(stopi > m_CellWidth - 1) stopi = m_CellWidth - 1;

	int startj = gp_Scene->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
	if(startj < 0) startj = 0;
	int stopj = gp_Scene->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
	if(stopj > m_CellWidth - 1) stopj = m_CellWidth - 1;

	for(int i = starti; i <= stopi; ++i)
	{
		for(int j = startj; j <= stopj; ++j)
		{
			if(rebuildall || GlobalIsDistanceLessThan(i, j, gp_Scene->m_Camera.m_LookAtPoint.x, gp_Scene->m_Camera.m_LookAtPoint.z, m_TerrainViewRange))
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

				if(gp_Scene->m_HardwareVertexProcessingSupported)
				{
					_Vertices[((i * m_CellWidth) + j) * 5 + 0] =  MakeTerrainVertex(i     , GetValue (i    , j    ), j     , D3DCOLOR_ARGB(alphaUL, lightingUL, lightingUL, lightingUL),      i     , j     , _u, _v );
					_Vertices[((i * m_CellWidth) + j) * 5 + 1] =  MakeTerrainVertex(i +  1, GetValue (i + 1, j    ), j     , D3DCOLOR_ARGB(alphaUR, lightingUR, lightingUR, lightingUR),      i +  1, j     , _u + .0625 , _v );
					_Vertices[((i * m_CellWidth) + j) * 5 + 2] =  MakeTerrainVertex(i +  1, GetValue (i + 1, j + 1), j +  1, D3DCOLOR_ARGB(alphaLR, lightingLR, lightingLR, lightingLR),      i +  1, j +  1, _u + .0625 , _v + 1 );
					_Vertices[((i * m_CellWidth) + j) * 5 + 3] =  MakeTerrainVertex(i     , GetValue (i    , j + 1), j +  1, D3DCOLOR_ARGB(alphaLL, lightingLL, lightingLL, lightingLL),      i     , j +  1, _u         , _v + 1 );
					_Vertices[((i * m_CellWidth) + j) * 5 + 4] =  MakeTerrainVertex(i + .5, GetMiddle(i    , j    ), j + .5, D3DCOLOR_ARGB(alphaMID, lightingMID, lightingMID, lightingMID),  i + .5, j + .5, _u + .03125, _v + .5 );
				}
				else
				{
					_Vertices[((i * m_CellWidth) + j) * 4 + 0] =  MakeTerrainVertex(i     , GetValue (i    , j    ), j     , D3DCOLOR_ARGB(alphaUL, lightingUL, lightingUL, lightingUL),  i    , j    , _u, _v);
					_Vertices[((i * m_CellWidth) + j) * 4 + 1] =  MakeTerrainVertex(i +  1, GetValue (i + 1, j    ), j     , D3DCOLOR_ARGB(alphaUR, lightingUR, lightingUR, lightingUR),  i + 1, j    , _u + .0625 , _v);
					_Vertices[((i * m_CellWidth) + j) * 4 + 2] =  MakeTerrainVertex(i +  1, GetValue (i + 1, j + 1), j +  1, D3DCOLOR_ARGB(alphaLR, lightingLR, lightingLR, lightingLR),  i + 1, j + 1, _u + .0625 , _v + 1);
					_Vertices[((i * m_CellWidth) + j) * 4 + 3] =  MakeTerrainVertex(i     , GetValue (i    , j + 1), j +  1, D3DCOLOR_ARGB(alphaLL, lightingLL, lightingLL, lightingLL),  i    , j + 1, _u         , _v + 1);
				}
			}
		}
	}

	m_DoWeNeedToRecreateVertexBuffers = false;
}

void Terrain::CreateWaterVertexBuffer()
{
	m_WaterVertices.clear();

	int starti = gp_Scene->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
	if(starti < 0) starti = 0;
	int stopi = gp_Scene->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
	if(stopi > m_CellWidth - 1) stopi = m_CellWidth - 1;

	int startj = gp_Scene->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
	if(startj < 0) startj = 0;
	int stopj = gp_Scene->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
	if(stopj > m_CellWidth - 1) stopj = m_CellWidth - 1;

	vector<unsigned short> waterIndices;

	for(float i = starti; i <= stopi; ++i)
	{
		for(float j = startj; j <= stopj; ++j)
		{
			if(GlobalIsDistanceLessThan(i, j, gp_Scene->m_Camera.m_LookAtPoint.x, gp_Scene->m_Camera.m_LookAtPoint.z, m_TerrainViewRange))
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

					float x = i;
					float y = j;
					float xplus1 = x + 1;
					float yplus1 = y + 1;

					if(x == 0) x -= .5f;
					if(y == 0) y -= .5f;
					if(xplus1 == m_CellWidth) xplus1 += .5f;
					if(yplus1 == m_CellHeight) yplus1 += .5f;

					const unsigned short base = static_cast<unsigned short>(m_WaterVertices.size());
					m_WaterVertices.push_back(MakeTerrainVertex(x     , .2f + UL, y     , D3DCOLOR_ARGB(alphaUL, lightingUL, lightingUL, lightingUL), ((i +     (gp_Engine->m_GameTimeInSeconds / 2)) * .25f) + (UL * 2) - (.1f * gp_Scene->m_Camera.m_LookAtPoint.x) + (2 * GetValue(i    , j    )), ((j +     (gp_Engine->m_GameTimeInSeconds / 2)) * .25f) + (UL * 2) - (.1f * gp_Scene->m_Camera.m_LookAtPoint.z) + (2 * GetValue(i    , j    ))));
					m_WaterVertices.push_back(MakeTerrainVertex(xplus1, .2f + UR, y     , D3DCOLOR_ARGB(alphaUR, lightingUR, lightingUR, lightingUR), ((i + 1 + (gp_Engine->m_GameTimeInSeconds / 2)) * .25f) + (UR * 2) - (.1f * gp_Scene->m_Camera.m_LookAtPoint.x) + (2 * GetValue(i + 1, j    )), ((j +     (gp_Engine->m_GameTimeInSeconds / 2)) * .25f) + (UR * 2) - (.1f * gp_Scene->m_Camera.m_LookAtPoint.z) + (2 * GetValue(i + 1, j    ))));
					m_WaterVertices.push_back(MakeTerrainVertex(xplus1, .2f + LR, yplus1, D3DCOLOR_ARGB(alphaUL, lightingLR, lightingLR, lightingLR), ((i + 1 + (gp_Engine->m_GameTimeInSeconds / 2)) * .25f) + (LR * 2) - (.1f * gp_Scene->m_Camera.m_LookAtPoint.x) + (2 * GetValue(i + 1, j + 1)), ((j + 1 + (gp_Engine->m_GameTimeInSeconds / 2)) * .25f) + (LR * 2) - (.1f * gp_Scene->m_Camera.m_LookAtPoint.z) + (2 * GetValue(i + 1, j + 1))));
					m_WaterVertices.push_back(MakeTerrainVertex(x     , .2f + LL, yplus1, D3DCOLOR_ARGB(alphaUL, lightingLL, lightingLL, lightingLL), ((i +     (gp_Engine->m_GameTimeInSeconds / 2)) * .25f) + (LL * 2) - (.1f * gp_Scene->m_Camera.m_LookAtPoint.x) + (2 * GetValue(i    , j + 1)), ((j + 1 + (gp_Engine->m_GameTimeInSeconds / 2)) * .25f) + (LL * 2) - (.1f * gp_Scene->m_Camera.m_LookAtPoint.z) + (2 * GetValue(i    , j + 1))));

					waterIndices.push_back(base + 0);
					waterIndices.push_back(base + 1);
					waterIndices.push_back(base + 3);
					waterIndices.push_back(base + 2);
					waterIndices.push_back(base + 3);
					waterIndices.push_back(base + 1);
				}
			}
		}
	}

	UnloadDrawMesh(m_WaterMesh);
	if (!m_WaterVertices.empty())
	{
		Mesh mesh = BuildMeshFromVerticesAndIndices(m_WaterVertices, waterIndices);
		m_WaterMesh.model = LoadModelFromMesh(mesh);
		m_WaterMesh.loaded = true;
		m_WaterMesh.triangleCount = static_cast<int>(waterIndices.size()) / 3;
	}
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

	struct result{int x; int y; double distance;};
	vector<result> results; 

	int starti = gp_Scene->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
	if(starti < 0) starti = 0;
	int stopi = gp_Scene->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
	if(stopi > m_CellWidth - 1) stopi = m_CellWidth - 1;

	int startj = gp_Scene->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
	if(startj < 0) startj = 0;
	int stopj = gp_Scene->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
	if(stopj > m_CellWidth - 1) stopj = m_CellWidth - 1;

	for(int i = starti; i <= stopi; ++i)
	{
		for(int j = startj; j < stopj; ++j)
		{
			double distance;
			if(gp_Scene->PickTriangle(
				D3DXVECTOR3(i    , GetValue(i    , j    ), j    ),
				D3DXVECTOR3(i + 1, GetValue(i + 1, j    ), j    ),
				D3DXVECTOR3(i    , GetValue(i    , j + 1), j + 1), distance) ||
				gp_Scene->PickTriangle(
				D3DXVECTOR3(i + 1, GetValue(i + 1, j + 1), j + 1),
				D3DXVECTOR3(i    , GetValue(i    , j + 1), j + 1),
				D3DXVECTOR3(i + 1, GetValue(i + 1, j    ), j    ), distance))
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
    struct result{double x; double y; double distance; double u; double v; int whichTriangle; };
    vector<result> results; 

    int starti = gp_Scene->m_Camera.m_LookAtPoint.x - m_TerrainViewRange;
    if(starti < 0) starti = 0;
    int stopi = gp_Scene->m_Camera.m_LookAtPoint.x + m_TerrainViewRange;
    if(stopi > m_CellWidth - 1) stopi = m_CellWidth - 1;

    int startj = gp_Scene->m_Camera.m_LookAtPoint.z - m_TerrainViewRange;
    if(startj < 0) startj = 0;
    int stopj = gp_Scene->m_Camera.m_LookAtPoint.z + m_TerrainViewRange;
    if(stopj > m_CellWidth - 1) stopj = m_CellWidth - 1;

    for(int i = starti; i <= stopi; ++i)
    {
        for(int j = startj; j < stopj; ++j)
        {
            double distance = 0;
            double triU = 0;
            double triV = 0;
            if(gp_Scene->PickTriangleUV(
                D3DXVECTOR3(i    , GetValue(i    , j    ), j    ),
                D3DXVECTOR3(i + 1, GetValue(i + 1, j    ), j    ),
                D3DXVECTOR3(i    , GetValue(i    , j + 1), j + 1), distance, triU, triV))
			{
				result temp = {i, j, distance, triU, triV, 0};
                results.push_back(temp);
			}
			else if(gp_Scene->PickTriangleUV(
                D3DXVECTOR3(i + 1, GetValue(i + 1, j + 1), j + 1),
                D3DXVECTOR3(i    , GetValue(i    , j + 1), j + 1),
                D3DXVECTOR3(i + 1, GetValue(i + 1, j    ), j    ), distance, triU, triV))
            {
				result temp = {i, j, distance, triU, triV, 1};
                results.push_back(temp);
            }
        }
    }

    if(results.size() == 0)
    {
        x = 0;
        y = 0;
        u = 0;
        v = 0;
		whichTriangle = 0;
        return;
    }

    double finaldistance = 99999;
    const result* best = &results.front();
    for(const result& hit : results)
    {
        if(hit.distance < finaldistance)
        {
            best = &hit;
            finaldistance = hit.distance;
        }
    }

    const Ray ray = gp_Scene->GetPickRay();
    x = ray.position.x + ray.direction.x * finaldistance;
    y = ray.position.z + ray.direction.z * finaldistance;
    u = best->u;
    v = best->v;
    whichTriangle = best->whichTriangle;
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
	BeginBlendMode(BLEND_ALPHA);

	for (int i = 0; i < NUMBER_OF_VERTEX_BUFFERS; ++i)
	{
		if (m_IndexBufferSizes[i] <= 0)
			continue;
		if (i == TT_HOUSE || i == TT_FARMLAND || i == TT_LAVA
			|| i == TT_BLESSEDLAND || i == TT_RUINEDLAND || i == TT_SWAMP)
			continue;

		Texture* texture = m_GrassTexture;
		if (i == TT_WATER || i == TT_SAND || i == TT_BEACH)
			texture = m_SandTexture;

		DrawTerrainModel(m_TypeMeshes[i], texture);
		m_NumberOfTrisDrawnThisFrame += m_TypeMeshes[i].triangleCount;
	}

	if (m_IndexBufferSizes[TT_BEACH] > 0)
	{
		DrawTerrainModel(m_TypeMeshes[TT_BEACH], m_GrassTexture);
		m_NumberOfTrisDrawnThisFrame += m_TypeMeshes[TT_BEACH].triangleCount;
	}

	if (m_IndexBufferSizes[TT_HOUSE] > 0)
	{
		DrawTerrainModel(m_TypeMeshes[TT_HOUSE], m_HouseTexture);
		m_NumberOfTrisDrawnThisFrame += m_TypeMeshes[TT_HOUSE].triangleCount;
	}

	if (m_IndexBufferSizes[TT_FARMLAND] > 0)
	{
		DrawTerrainModel(m_TypeMeshes[TT_FARMLAND], m_FarmTexture);
		m_NumberOfTrisDrawnThisFrame += m_TypeMeshes[TT_FARMLAND].triangleCount;
	}

	if (m_IndexBufferSizes[TT_LAVA] > 0)
	{
		int startingvalue = gp_Engine->m_GameTimeInMS % 1024;
		startingvalue /= 16;
		if (startingvalue > 32) startingvalue = 64 - startingvalue;
		startingvalue += 192;
		Color lavaTint = {static_cast<unsigned char>(startingvalue),
			static_cast<unsigned char>(startingvalue),
			static_cast<unsigned char>(startingvalue), 255};
		DrawTerrainModel(m_TypeMeshes[TT_LAVA], m_LavaTexture, lavaTint);
		m_NumberOfTrisDrawnThisFrame += m_TypeMeshes[TT_LAVA].triangleCount;
	}

	if (m_IndexBufferSizes[TT_BLESSEDLAND] > 0)
	{
		DrawTerrainModel(m_TypeMeshes[TT_BLESSEDLAND], m_BlessTexture);
		m_NumberOfTrisDrawnThisFrame += m_TypeMeshes[TT_BLESSEDLAND].triangleCount;
	}

	if (m_IndexBufferSizes[TT_RUINEDLAND] > 0)
	{
		DrawTerrainModel(m_TypeMeshes[TT_RUINEDLAND], m_RuinTexture);
		m_NumberOfTrisDrawnThisFrame += m_TypeMeshes[TT_RUINEDLAND].triangleCount;
	}

	if (m_IndexBufferSizes[TT_SWAMP] > 0)
	{
		DrawTerrainModel(m_TypeMeshes[TT_SWAMP], m_SwampTexture);
		m_NumberOfTrisDrawnThisFrame += m_TypeMeshes[TT_SWAMP].triangleCount;
	}

	EndBlendMode();
}

void Terrain::DrawWater()
{
	if (!m_WaterMesh.loaded || m_WaterMesh.triangleCount <= 0)
		return;

	BeginBlendMode(BLEND_ALPHA);
	DrawTerrainModel(m_WaterMesh, m_DeepWaterTexture);
	m_NumberOfTrisDrawnThisFrame += m_WaterMesh.triangleCount;
	EndBlendMode();
}

void Terrain::UpdateMiniMap()
{
	if (!m_MiniMapTexture) return;
	Image img = LoadImageFromTexture(*m_MiniMapTexture);
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
	UnloadTexture(*m_MiniMapTexture);
	*m_MiniMapTexture = updated;
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
	// Use the same view/projection path as the legacy camera — GetWorldToScreen()
	// compares against the full framebuffer size, not m_HRes/m_VRes, which culled
	// almost all cells after the BeginMode3D migration.
	D3DXVECTOR3 originalPoint = D3DXVECTOR3(x + .5f, GetMiddle(x, y), y + .5f);

	D3DXMATRIX V = gp_Scene->m_CurrentCamera;
	D3DXMATRIX W;
	D3DXMatrixIdentity(&W);

	D3DXMATRIX P;
	gp_Scene->m_D3DDevice.GetTransform(D3DTS_PROJECTION, &P);

	D3DVIEWPORT9 VP;
	gp_Scene->m_D3DDevice.GetViewport(&VP);

	D3DXVec3Project(&originalPoint, &originalPoint, &VP, &P, &V, &W);

	if(originalPoint.x <= (gp_Scene->m_HRes + (gp_Scene->m_HRes * .2f))
		&& originalPoint.x >= (0 - (gp_Scene->m_HRes * .2f))
		&& originalPoint.y <= (gp_Scene->m_VRes + (gp_Scene->m_VRes * .2f))
		&& originalPoint.y >= (0 - (gp_Scene->m_VRes * .2f)))
		return true;
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
	const Color hitTint = D3DColorToRaylib(m_TerrainHitColor);

	if (m_ShowTerrainHit && m_HighlightMesh.loaded)
	{
		BeginBlendMode(BLEND_ALPHA);
		rlDisableDepthTest();
		DrawTerrainModel(m_HighlightMesh, m_TerrainHighlightRing, hitTint);
		rlEnableDepthTest();
		EndBlendMode();
	}

	for (int i = 0; i < 4; ++i)
	{
		if (g_Players[i] != NULL && g_Players[i]->m_General != NULL && g_Players[i]->m_General->m_Selected
			&& m_ColoredHighlightMeshes[i].loaded)
		{
			BeginBlendMode(BLEND_ALPHA);
			DrawTerrainModel(m_ColoredHighlightMeshes[i], m_TerrainHighlightRing, hitTint);
			EndBlendMode();
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