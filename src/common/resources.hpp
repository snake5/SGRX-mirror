

#pragma once
#include <engine.hpp>
#include <enganim.hpp>
#include <pathfinding.hpp>
#include <sound.hpp>
#include <physics.hpp>
#include <script.hpp>

#include "level.hpp"
#include "systems.hpp"



#define GO_RSRC_MESH 1

EXP_STRUCT MeshResource : GOResource
{
	SGS_OBJECT_INHERIT( GOResource );
	ENT_SGS_IMPLEMENT;
	
	GFW_EXPORT MeshResource( GameObject* obj );
	GFW_EXPORT ~MeshResource();
	GFW_EXPORT virtual void OnTransformUpdate();
	GFW_EXPORT virtual void EditorDrawWorld();
	GFW_EXPORT virtual void EditUI( EditorUIHelper* uih );
	GFW_EXPORT void _UpdateLighting();
	GFW_EXPORT void _UpdateMatrix();
	
	FINLINE void _UpEv(){ }//Game_FireEvent( EID_MeshUpdate, this ); }
	
	bool IsStatic() const { return m_isStatic; }
	void SetStatic( bool v ){ m_isStatic = v; }
	bool IsVisible() const { return m_isVisible; }
	void SetVisible( bool v ){ m_isVisible = v; m_meshInst->enabled = v; _UpEv(); }
	MeshHandle GetMeshData() const { return m_mesh; }
	GFW_EXPORT void SetMeshData( MeshHandle mesh );
	StringView GetMeshPath() const { return m_mesh ? SV(m_mesh->m_key) : SV(); }
	void SetMeshPath( StringView path ){ SetMeshData( GR_GetMesh( path ) ); }
	Mat4 GetMatrix() const { return m_localMatrix; }
	void SetMatrix( Mat4 m ){ m_localMatrix = m; _UpdateMatrix(); }
	bool GetLightingMode() const { return m_lightingMode; }
	void SetLightingMode( int v ){ m_lightingMode = v;
		m_meshInst->SetLightingMode( (SGRX_LightingMode) v ); _UpdateLighting(); }
	
	SGS_PROPERTY_FUNC( READ SOURCE m_meshInst.item ) SGS_ALIAS( void* meshInst );
	SGS_PROPERTY_FUNC( READ IsStatic WRITE SetStatic VARNAME isStatic ) bool m_isStatic;
	SGS_PROPERTY_FUNC( READ IsVisible WRITE SetVisible VARNAME visible ) bool m_isVisible;
	SGS_PROPERTY_FUNC( READ GetMeshData WRITE SetMeshData VARNAME meshData ) MeshHandle m_mesh;
	SGS_PROPERTY_FUNC( READ GetMeshPath WRITE SetMeshPath ) SGS_ALIAS( StringView mesh );
	SGS_PROPERTY_FUNC( READ GetMatrix WRITE SetMatrix ) SGS_ALIAS( Mat4 matrix );
	SGS_PROPERTY_FUNC( READ GetLightingMode WRITE SetLightingMode VARNAME lightingMode ) int m_lightingMode;
	// editor-only static mesh parameters
	SGS_PROPERTY_FUNC( READ WRITE WRITE_CALLBACK _UpEv VARNAME lmQuality ) float m_lmQuality;
	SGS_PROPERTY_FUNC( READ WRITE WRITE_CALLBACK _UpEv VARNAME castLMS ) bool m_castLMS;
	
	GFW_EXPORT SGS_METHOD void SetShaderConst( int v, Vec4 var );
	
	MeshInstHandle m_meshInst;
	Mat4 m_localMatrix;
	uint32_t m_edLGCID;
};


