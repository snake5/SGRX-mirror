

#include "resources.hpp"

#include "imgui.hpp"


MeshResource::MeshResource( GameObject* obj ) : GOResource( obj ),
	m_isStatic( true ),
	m_isVisible( true ),
	m_lightingMode( SGRX_LM_Dynamic ),
	m_lmQuality( 1 ),
	m_castLMS( true ),
	m_localMatrix( Mat4::Identity ),
	m_edLGCID( 0 )
{
	m_meshInst = m_level->GetScene()->CreateMeshInstance();
	m_meshInst->SetLightingMode( (SGRX_LightingMode) m_lightingMode );
	_UpdateMatrix();
}

MeshResource::~MeshResource()
{
}

void MeshResource::OnTransformUpdate()
{
	_UpdateMatrix();
}

void MeshResource::_UpdateMatrix()
{
	m_meshInst->SetTransform( m_localMatrix * m_obj->GetWorldMatrix() );
	_UpdateLighting();
	_UpEv();
}

void MeshResource::EditorDrawWorld()
{
	if( m_mesh )
	{
		BatchRenderer& br = GR2D_GetBatchRenderer();
		br.Reset();
		br.Col( 0.2f, 0.7f, 0.9f, 0.8f );
		br.AABB( m_mesh->m_boundsMin, m_mesh->m_boundsMax, m_meshInst->matrix );
	}
}

void MeshResource::EditUI( EditorUIHelper* uih )
{
	String path = GetMeshPath();
	if( uih->ResourcePicker( EditorUIHelper::PT_Mesh,
		"Select mesh", "Mesh", path ) )
		SetMeshPath( path );
	
	if( IMGUIComboBox( "Lighting mode", m_lightingMode,
		"Unlit\0Static\0Dynamic\0Decal\0" ) )
		SetLightingMode( m_lightingMode );
}

void MeshResource::_UpdateLighting()
{
	if( m_lightingMode == SGRX_LM_Dynamic )
	{
		m_level->LightMesh( m_meshInst );
	}
	_UpEv();
}

void MeshResource::SetMeshData( MeshHandle mesh )
{
	if( m_mesh == mesh )
		return;
	m_mesh = mesh;
	m_meshInst->SetMesh( mesh );
	// _UpEv already called
}

void MeshResource::SetShaderConst( int v, Vec4 var )
{
	if( v < 0 || v >= MAX_MI_CONSTANTS )
	{
		sgs_Msg( C, SGS_WARNING, "shader constant %d outside range [0;%d)", v, MAX_MI_CONSTANTS );
		return;
	}
	m_meshInst->constants[ v ] = var;
}

