

#define USE_HASHTABLE
#include "renderer.hpp"



void IRenderer::_RS_Cull_Camera_Prepare( SGRX_Scene* scene )
{
	SGRX_Cull_Camera_Prepare( scene );
}

uint32_t IRenderer::_RS_Cull_Camera_MeshList( SGRX_Scene* scene )
{
	m_visible_meshes.clear();
	return SGRX_Cull_Camera_MeshList( m_visible_meshes, m_scratchMem, scene );
}

uint32_t IRenderer::_RS_Cull_Camera_PointLightList( SGRX_Scene* scene )
{
	m_visible_point_lights.clear();
	return SGRX_Cull_Camera_PointLightList( m_visible_point_lights, m_scratchMem, scene );
}

uint32_t IRenderer::_RS_Cull_Camera_SpotLightList( SGRX_Scene* scene )
{
	m_visible_spot_lights.clear();
	return SGRX_Cull_Camera_SpotLightList( m_visible_spot_lights, m_scratchMem, scene );
}

uint32_t IRenderer::_RS_Cull_SpotLight_MeshList( SGRX_Scene* scene, SGRX_Light* L )
{
	SGRX_Cull_SpotLight_Prepare( scene, L );
	m_visible_spot_meshes.clear();
	return SGRX_Cull_SpotLight_MeshList( m_visible_spot_meshes, m_scratchMem, scene, L );
}

static int sort_meshinstlight_by_light( const void* p1, const void* p2 )
{
	SGRX_MeshInstLight* mil1 = (SGRX_MeshInstLight*) p1;
	SGRX_MeshInstLight* mil2 = (SGRX_MeshInstLight*) p2;
	return mil1->L == mil2->L ? 0 : ( mil1->L < mil2->L ? -1 : 1 );
}

void IRenderer::_RS_Compile_MeshLists( SGRX_Scene* scene )
{
	for( size_t inst_id = 0; inst_id < scene->m_meshInstances.size(); ++inst_id )
	{
		SGRX_MeshInstance* MI = scene->m_meshInstances.item( inst_id ).key;
		
		MI->_lightbuf_begin = NULL;
		MI->_lightbuf_end = NULL;
		
		if( !MI->mesh || !MI->enabled || MI->unlit )
			continue;
		MI->_lightbuf_begin = (SGRX_MeshInstLight*) m_inst_light_buf.size_bytes();
		// POINT LIGHTS
		for( size_t light_id = 0; light_id < m_visible_point_lights.size(); ++light_id )
		{
			SGRX_Light* L = m_visible_point_lights[ light_id ];
			SGRX_MeshInstLight mil = { MI, L };
			m_inst_light_buf.push_back( mil );
		}
		// SPOTLIGHTS
		for( size_t light_id = 0; light_id < m_visible_spot_lights.size(); ++light_id )
		{
			SGRX_Light* L = m_visible_spot_lights[ light_id ];
			SGRX_MeshInstLight mil = { MI, L };
			m_inst_light_buf.push_back( mil );
		}
		MI->_lightbuf_end = (SGRX_MeshInstLight*) m_inst_light_buf.size_bytes();
	}
	for( size_t inst_id = 0; inst_id < scene->m_meshInstances.size(); ++inst_id )
	{
		SGRX_MeshInstance* MI = scene->m_meshInstances.item( inst_id ).key;
		if( !MI->mesh || !MI->enabled || MI->unlit )
			continue;
		MI->_lightbuf_begin = (SGRX_MeshInstLight*)( (uintptr_t) MI->_lightbuf_begin + (uintptr_t) m_inst_light_buf.data() );
		MI->_lightbuf_end = (SGRX_MeshInstLight*)( (uintptr_t) MI->_lightbuf_end + (uintptr_t) m_inst_light_buf.data() );
	}
	
	/*  insts -> lights  TO  lights -> insts  */
	m_light_inst_buf = m_inst_light_buf;
	memcpy( m_light_inst_buf.data(), m_inst_light_buf.data(), m_inst_light_buf.size() );
	qsort( m_light_inst_buf.data(), m_light_inst_buf.size(), sizeof( SGRX_MeshInstLight ), sort_meshinstlight_by_light );
	
	for( size_t light_id = 0; light_id < scene->m_lights.size(); ++light_id )
	{
		SGRX_Light* L = scene->m_lights.item( light_id ).key;
		if( !L->enabled )
			continue;
		L->_mibuf_begin = NULL;
		L->_mibuf_end = NULL;
	}
	
	SGRX_MeshInstLight* pmil = m_light_inst_buf.data();
	SGRX_MeshInstLight* pmilend = pmil + m_light_inst_buf.size();
	
	while( pmil < pmilend )
	{
		if( !pmil->L->_mibuf_begin )
			pmil->L->_mibuf_begin = pmil;
		pmil->L->_mibuf_end = pmil + 1;
		pmil++;
	}
}
