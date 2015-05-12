

#define MAPEDIT_DEFINE_GLOBALS
#include "mapedit.hpp"



EDGUIPropRsrc_PickParentGroup::EDGUIPropRsrc_PickParentGroup( int32_t id, EDGUIGroupPicker* gp, const StringView& value ) :
	EDGUIPropRsrc( gp, value ),
	m_id( id )
{
}

void EDGUIPropRsrc_PickParentGroup::OnReload( bool after )
{
	SGRX_CAST( EDGUIGroupPicker*, gp, m_rsrcPicker );
	gp->m_ignoreID = after ? -1 : m_id;
}

EdGroup::EdGroup( struct EdGroupManager* groupMgr, int32_t id, int32_t pid, const StringView& name ) :
	m_refcount( 0 ),
	m_groupMgr( groupMgr ),
	m_id( id ),
	m_parent_id( pid ),
	m_needsMtxUpdate( true ),
	m_mtxLocal( Mat4::Identity ),
	m_mtxCombined( Mat4::Identity ),
	m_needsPathUpdate( true ),
	m_group( true, "Group" ),
	m_ctlName( name ),
	m_ctlParent( id, &groupMgr->m_grpPicker, id ? groupMgr->GetPath( pid ) : StringView() ),
	m_ctlOrigin( V3(0), 2, V3(-8192), V3(8192) ),
	m_ctlPos( V3(0), 2, V3(-8192), V3(8192) ),
	m_ctlAngles( V3(0), 2, V3(0), V3(360) ),
	m_ctlScaleUni( 1, 2, 0.01f, 100.0f )
{
	m_ctlName.caption = "Name";
	m_ctlParent.caption = "Parent";
	m_ctlOrigin.caption = "Origin";
	m_ctlPos.caption = "Position";
	m_ctlAngles.caption = "Rotation";
	m_ctlScaleUni.caption = "Scale (uniform)";
	m_addChild.caption = "Add child";
	m_deleteDisownParent.caption = "Delete group and subobjects -> parent";
	m_deleteDisownRoot.caption = "Delete group and subobjects -> root";
	m_deleteRecursive.caption = "Delete group and destroy subobjects";
	m_recalcOrigin.caption = "Recalculate origin";
	m_cloneGroup.caption = "Clone group only";
	m_cloneGroupWithSub.caption = "Clone group with subobjects";
	m_exportObj.caption = "Export group as .obj";
	
	m_group.Add( &m_ctlName );
	if( m_id )
	{
		m_group.Add( &m_ctlParent );
	}
	m_group.Add( &m_ctlOrigin );
	m_group.Add( &m_ctlPos );
	m_group.Add( &m_ctlAngles );
	m_group.Add( &m_ctlScaleUni );
	m_group.Add( &m_addChild );
	if( m_id )
	{
		m_group.Add( &m_deleteDisownParent );
		m_group.Add( &m_deleteDisownRoot );
		m_group.Add( &m_deleteRecursive );
	}
	m_group.Add( &m_recalcOrigin );
	if( m_id )
	{
		m_group.Add( &m_cloneGroup );
		m_group.Add( &m_cloneGroupWithSub );
	}
	m_group.Add( &m_exportObj );
	
	Add( &m_group );
}

int EdGroup::OnEvent( EDGUIEvent* e )
{
	switch( e->type )
	{
	case EDGUI_EVENT_BTNCLICK:
		if( e->target == &m_addChild )
		{
			m_groupMgr->PrepareEditGroup( m_groupMgr->AddGroup( m_id ) );
		}
		if( e->target == &m_deleteDisownParent )
		{
			g_EdWorld->TransferObjectsToGroup( m_id, m_parent_id );
			m_groupMgr->TransferGroupsToGroup( m_id, m_parent_id );
			m_groupMgr->QueueDestroy( this );
		}
		if( e->target == &m_deleteDisownRoot )
		{
			g_EdWorld->TransferObjectsToGroup( m_id, 0 );
			m_groupMgr->TransferGroupsToGroup( m_id, 0 );
			m_groupMgr->QueueDestroy( this );
		}
		if( e->target == &m_deleteRecursive )
		{
			m_groupMgr->QueueDestroy( this );
		}
		if( e->target == &m_recalcOrigin )
		{
			m_ctlOrigin.SetValue( g_EdWorld->FindCenterOfGroup( m_id ) );
		}
		if( e->target == &m_cloneGroup )
		{
			EdGroup* grp = Clone();
			m_groupMgr->PrepareEditGroup( grp );
		}
		if( e->target == &m_cloneGroupWithSub )
		{
			EdGroup* grp = Clone();
			g_EdWorld->CopyObjectsToGroup( m_id, grp->m_id );
			m_groupMgr->PrepareEditGroup( grp );
		}
		if( e->target == &m_exportObj )
		{
			g_EdWorld->ExportGroupAsOBJ( m_id, "group.obj" );
		}
		break;
	case EDGUI_EVENT_PROPEDIT:
		if( e->target == &m_ctlOrigin ||
			e->target == &m_ctlPos ||
			e->target == &m_ctlAngles ||
			e->target == &m_ctlScaleUni ||
			e->target == &m_ctlParent )
		{
			m_groupMgr->MatrixInvalidate( m_id );
		}
		break;
	case EDGUI_EVENT_PROPCHANGE:
		if( e->target == &m_ctlParent )
		{
			m_parent_id = m_groupMgr->FindGroupByPath( m_ctlParent.m_value )->m_id;
		}
		if( e->target == &m_ctlName || e->target == &m_ctlParent )
		{
			m_groupMgr->PathInvalidate( m_id );
			m_groupMgr->m_editedGroup.SetValue( GetPath() );
		}
		break;
	}
	return EDGUILayoutRow::OnEvent( e );
}

Mat4 EdGroup::GetMatrix()
{
	if( m_needsMtxUpdate )
	{
		m_mtxCombined = m_mtxLocal = Mat4::CreateTranslation( -m_ctlOrigin.m_value ) *
			Mat4::CreateSRT( V3( m_ctlScaleUni.m_value ), DEG2RAD( m_ctlAngles.m_value ), m_ctlPos.m_value + m_ctlOrigin.m_value );
		if( m_id )
		{
			m_mtxCombined = m_mtxCombined * m_groupMgr->GetMatrix( m_parent_id );
		}
	}
	return m_mtxCombined;
}

StringView EdGroup::GetPath()
{
	if( m_needsPathUpdate )
	{
		m_path.clear();
		if( m_id )
		{
			m_path = m_groupMgr->GetPath( m_parent_id );
			m_path.append( "/" );
		}
		m_path.append( Name() );
	}
	return m_path;
}

EdGroup* EdGroup::Clone()
{
	EdGroup* grp = m_groupMgr->AddGroup( m_parent_id );
	grp->m_ctlOrigin.SetValue( m_ctlOrigin.m_value );
	grp->m_ctlPos.SetValue( m_ctlPos.m_value );
	grp->m_ctlAngles.SetValue( m_ctlAngles.m_value );
	grp->m_ctlScaleUni.SetValue( m_ctlScaleUni.m_value );
	return grp;
}


void EDGUIGroupPicker::Reload()
{
	m_options.clear();
	for( size_t i = 0; i < m_groupMgr->m_groups.size(); ++i )
	{
		EdGroup* grp = m_groupMgr->m_groups.item( i ).value;
		if( grp->m_id != m_ignoreID && m_groupMgr->GroupHasParent( grp->m_id, m_ignoreID ) == false )
			m_options.push_back( grp->GetPath() );
	}
	_Search( m_searchString );
}


EdGroupManager::EdGroupManager() :
	m_grpPicker( this ),
	m_lastGroupID(-1),
	m_editedGroup( &m_grpPicker )
{
	m_gotoRoot.caption = "Go to root";
	m_editedGroup.caption = "Edited group";
}

int EdGroupManager::OnEvent( EDGUIEvent* e )
{
	switch( e->type )
	{
	case EDGUI_EVENT_BTNCLICK:
		if( e->target == &m_gotoRoot )
		{
			PrepareEditGroup( FindGroupByID( 0 ) );
		}
		break;
	case EDGUI_EVENT_PROPCHANGE:
		if( e->target == &m_editedGroup )
		{
			PrepareCurrentEditGroup();
		}
		break;
	}
	return EDGUILayoutRow::OnEvent( e );
}

void EdGroupManager::DrawGroups()
{
	BatchRenderer& br = GR2D_GetBatchRenderer();
	
	EdGroup* grp = FindGroupByPath( m_editedGroup.m_value );
	while( grp )
	{
		Mat4 parent_mtx = grp->m_id ? GetMatrix( grp->m_parent_id ) : Mat4::Identity;
		
		br.Col( 0.8f, 0.05f, 0.01f );
		br.Tick( grp->m_ctlPos.m_value + grp->m_ctlOrigin.m_value, 0.1f, parent_mtx );
		br.Col( 0.05f, 0.8f, 0.01f );
		br.Tick( grp->m_ctlPos.m_value, 0.1f, parent_mtx );
		
		if( grp->m_id == 0 )
			break;
		grp = FindGroupByID( grp->m_parent_id );
	}
}

void EdGroupManager::AddRootGroup()
{
	_AddGroup( 0, 0, "root" );
}

Mat4 EdGroupManager::GetMatrix( int32_t id )
{
	EdGroup* grp = m_groups.getcopy( id );
	ASSERT( grp );
	return grp->GetMatrix();
}

StringView EdGroupManager::GetPath( int32_t id )
{
	EdGroup* grp = m_groups.getcopy( id );
	ASSERT( grp );
	return grp->GetPath();
}

EdGroup* EdGroupManager::AddGroup( int32_t parent_id, StringView name, int32_t id )
{
	char bfr[ 32 ];
	if( id < 0 )
		id = m_lastGroupID + 1;
	if( !name )
	{
		sgrx_snprintf( bfr, 32, "group%d", id );
		name = bfr;
	}
	return _AddGroup( id, parent_id, name );
}

EdGroup* EdGroupManager::_AddGroup( int32_t id, int32_t parent_id, StringView name )
{
	EdGroup* grp = new EdGroup( this, id, parent_id, name );
	m_groups[ id ] = grp;
	if( m_lastGroupID < id )
		m_lastGroupID = id;
	return grp;
}

EdGroup* EdGroupManager::FindGroupByID( int32_t id )
{
	return m_groups.getcopy( id );
}

EdGroup* EdGroupManager::FindGroupByPath( StringView path )
{
	for( size_t i = 0; i < m_groups.size(); ++i )
	{
		EdGroup* grp = m_groups.item( i ).value;
		if( grp->GetPath() == path )
			return grp;
	}
	return NULL;
}

bool EdGroupManager::GroupHasParent( int32_t id, int32_t parent_id )
{
	while( id != 0 )
	{
		id = FindGroupByID( id )->m_parent_id;
		if( id == parent_id )
			return true;
	}
	return false;
}

void EdGroupManager::PrepareEditGroup( EdGroup* grp )
{
	Clear();
	Add( &m_gotoRoot );
	Add( &m_editedGroup );
	if( grp )
	{
		m_editedGroup.SetValue( grp->GetPath() );
		Add( grp );
	}
	else
		m_editedGroup.SetValue( "" );
}

void EdGroupManager::PrepareCurrentEditGroup()
{
	PrepareEditGroup( FindGroupByPath( m_editedGroup.m_value ) );
}

void EdGroupManager::TransferGroupsToGroup( int32_t from, int32_t to )
{
	for( size_t i = 0; i < m_groups.size(); ++i )
	{
		EdGroup* grp = m_groups.item( i ).value;
		if( grp->m_parent_id == from )
		{
			grp->m_parent_id = to;
			PathInvalidate( grp->m_id );
		}
	}
}

void EdGroupManager::QueueDestroy( EdGroup* grp )
{
	m_destroyQueue.push_back( grp );
	m_groups.unset( grp->m_id );
}

void EdGroupManager::ProcessDestroyQueue()
{
	for( size_t i = 0; i < m_destroyQueue.size(); ++i )
	{
		g_EdWorld->DeleteObjectsInGroup( m_destroyQueue[ i ]->m_id );
		for( size_t j = 0; j < m_groups.size(); ++j )
		{
			EdGroup* grp = m_groups.item( j ).value;
			if( grp->m_parent_id == m_destroyQueue[ i ]->m_id )
				m_destroyQueue.push_back( grp );
		}
	}
	m_destroyQueue.clear();
}

void EdGroupManager::MatrixInvalidate( int32_t id )
{
	FindGroupByID( id )->m_needsMtxUpdate = true;
	g_EdWorld->FixTransformsOfGroup( id );
	for( size_t i = 0; i < m_groups.size(); ++i )
	{
		EdGroup* grp = m_groups.item( i ).value;
		if( grp->m_id && grp->m_parent_id == id )
			MatrixInvalidate( grp->m_id ); // TODO: might need to fix algorithm for performance?
	}
}

void EdGroupManager::PathInvalidate( int32_t id )
{
	FindGroupByID( id )->m_needsPathUpdate = true;
	for( size_t i = 0; i < m_groups.size(); ++i )
	{
		EdGroup* grp = m_groups.item( i ).value;
		if( grp->m_id && grp->m_parent_id == id )
			PathInvalidate( grp->m_id ); // TODO: might need to fix algorithm for performance?
	}
}



EdWorld::EdWorld() :
	m_ctlGroup( true, "Level properties" ),
	m_ctlAmbientColor( V3(0,0,0.1f), 2, V3(0), V3(1,1,100) ),
	m_ctlDirLightDir( V2(0,0), 2, V2(-8192), V2(8192) ),
	m_ctlDirLightColor( V3(0,0,0.0f), 2, V3(0), V3(1,1,100) ),
	m_ctlDirLightDivergence( 10, 2, 0, 180 ),
	m_ctlDirLightNumSamples( 15, 0, 256 ),
	m_ctlLightmapClearColor( V3(0,0,0), 2, V3(0), V3(1,1,100) ),
	m_ctlRADNumBounces( 2, 0, 256 ),
	m_ctlLightmapDetail( 1, 2, 0.01f, 10 ),
	m_ctlLightmapBlurSize( 1, 2, 0, 10 ),
	m_ctlAODistance( 2, 2, 0, 100 ),
	m_ctlAOMultiplier( 1, 2, 0, 2 ),
	m_ctlAOFalloff( 2, 2, 0.01f, 100.0f ),
	m_ctlAOEffect( 0, 2, -1, 1 ),
//	m_ctlAODivergence( 0, 2, 0, 1 ),
	m_ctlAOColor( V3(0,0,0), 2, V3(0), V3(1,1,100) ),
	m_ctlAONumSamples( 15, 0, 256 )
{
	tyname = "world";
	m_ctlAmbientColor.caption = "Ambient color";
	m_ctlDirLightDir.caption = "Dir.light direction (dX,dY)";
	m_ctlDirLightColor.caption = "Dir.light color (HSV)";
	m_ctlDirLightDivergence.caption = "Dir.light divergence";
	m_ctlDirLightNumSamples.caption = "Dir.light sample count";
	m_ctlLightmapClearColor.caption = "Lightmap clear color (HSV)";
	m_ctlRADNumBounces.caption = "Radiosity bounce count";
	m_ctlLightmapDetail.caption = "Lightmap detail";
	m_ctlLightmapBlurSize.caption = "Lightmap blur size";
	m_ctlAODistance.caption = "AO distance";
	m_ctlAOMultiplier.caption = "AO multiplier";
	m_ctlAOFalloff.caption = "AO falloff";
	m_ctlAOEffect.caption = "AO effect";
//	m_ctlAODivergence.caption = "AO divergence";
	m_ctlAOColor.caption = "AO color";
	m_ctlAONumSamples.caption = "AO sample count";
	m_vd = GR_GetVertexDecl( LCVertex_DECL );
	
	Add( &m_ctlGroup );
	m_ctlGroup.Add( &m_ctlAmbientColor );
	m_ctlGroup.Add( &m_ctlDirLightDir );
	m_ctlGroup.Add( &m_ctlDirLightColor );
	m_ctlGroup.Add( &m_ctlDirLightDivergence );
	m_ctlGroup.Add( &m_ctlDirLightNumSamples );
	m_ctlGroup.Add( &m_ctlLightmapClearColor );
	m_ctlGroup.Add( &m_ctlRADNumBounces );
	m_ctlGroup.Add( &m_ctlLightmapDetail );
	m_ctlGroup.Add( &m_ctlLightmapBlurSize );
	m_ctlGroup.Add( &m_ctlAODistance );
	m_ctlGroup.Add( &m_ctlAOMultiplier );
	m_ctlGroup.Add( &m_ctlAOFalloff );
	m_ctlGroup.Add( &m_ctlAOEffect );
//	m_ctlGroup.Add( &m_ctlAODivergence );
	m_ctlGroup.Add( &m_ctlAOColor );
	m_ctlGroup.Add( &m_ctlAONumSamples );
	
	TestData();
}

void EdWorld::Reset()
{
	m_blocks.clear();
	m_entities.clear();
}

void EdWorld::TestData()
{
	m_groupMgr.AddRootGroup();
	
	EdBlock b1;
	
	b1.position = V3(0);
	b1.z0 = 0;
	b1.z1 = 2;
	
	Vec3 poly[] = { {-2,-2,0}, {2,-2,0}, {2,2,0}, {-2,2,0} };
	b1.poly.assign( poly, 4 );
	
	EdSurface surf;
	surf.texname = "metal0";
	for( int i = 0; i < 6; ++i )
		b1.surfaces.push_back( surf );
	b1.subsel.resize( b1.GetNumElements() );
	b1.ClearSelection();
	
	m_blocks.push_back( b1 );
	b1.z1 = 1;
	b1.position = V3( 0.1f, 1, 0.5f );
	m_blocks.push_back( b1 );
	
	RegenerateMeshes();
}

void EdWorld::RegenerateMeshes()
{
	for( size_t i = 0; i < m_blocks.size(); ++i )
		m_blocks[ i ].RegenerateMesh();
}

void EdWorld::DrawWires_Blocks( int hlblock, int selblock )
{
	BatchRenderer& br = GR2D_GetBatchRenderer();
	
	br.SetPrimitiveType( PT_Lines ).UnsetTexture();
	for( size_t i = 0; i < m_blocks.size(); ++i )
	{
		const EdBlock& B = m_blocks[ i ];
		GR2D_SetWorldMatrix( m_groupMgr.GetMatrix( B.group ) );
		
		if( (int) i == selblock || B.selected )
		{
			if( (int) i == hlblock )
				br.Col( 0.9f, 0.2f, 0.1f, 1 );
			else
				br.Col( 0.9f, 0.5f, 0.1f, 1 );
		}
		else if( (int) i == hlblock )
			br.Col( 0.1f, 0.8f, 0.9f, 0.9f );
		else
			br.Col( 0.1f, 0.5f, 0.9f, 0.5f );
		
		for( size_t v = 0; v < B.poly.size(); ++v )
		{
			size_t v1 = ( v + 1 ) % B.poly.size();
			
			br.Pos( B.position.x + B.poly[ v ].x,  B.position.y + B.poly[ v ].y, B.z0 + B.position.z );
			br.Pos( B.position.x + B.poly[ v1 ].x, B.position.y + B.poly[ v1 ].y, B.z0 + B.position.z );
			
			br.Pos( B.position.x + B.poly[ v ].x,  B.position.y + B.poly[ v ].y, B.poly[ v ].z + B.z1 + B.position.z );
			br.Pos( B.position.x + B.poly[ v1 ].x, B.position.y + B.poly[ v1 ].y, B.poly[ v1 ].z + B.z1 + B.position.z );
			
			br.Prev( 3 ).Prev( 2 );
		}
	}
	
	br.Flush();
	GR2D_SetWorldMatrix( Mat4::Identity );
}

void EdWorld::DrawPoly_BlockSurf( int block, int surf, bool sel )
{
	BatchRenderer& br = GR2D_GetBatchRenderer();
	
	br.SetPrimitiveType( PT_TriangleStrip ).UnsetTexture();
	
	if( sel )
		br.Col( 0.9f, 0.5, 0.1f, 0.2f );
	else
		br.Col( 0.1f, 0.5, 0.9f, 0.1f );
	
	const EdBlock& B = m_blocks[ block ];
	GR2D_SetWorldMatrix( m_groupMgr.GetMatrix( B.group ) );
	if( surf == (int) B.poly.size() )
	{
		for( size_t i = 0; i < B.poly.size(); ++i )
		{
			size_t v;
			if( i % 2 == 0 )
				v = i / 2;
			else
				v = B.poly.size() - 1 - i / 2;
			br.Pos( B.poly[ v ].x + B.position.x, B.poly[ v ].y + B.position.y, B.poly[ v ].z + B.z1 + B.position.z );
		}
//		br.Prev( B.poly.size() - 1 );
	}
	else if( surf == (int) B.poly.size() + 1 )
	{
		for( size_t i = B.poly.size(); i > 0; )
		{
			--i;
			size_t v;
			if( i % 2 == 0 )
				v = i / 2;
			else
				v = B.poly.size() - 1 - i / 2;
			br.Pos( B.poly[ v ].x + B.position.x, B.poly[ v ].y + B.position.y, B.z0 + B.position.z );
		}
//		br.Prev( B.poly.size() - 1 );
	}
	else
	{
		size_t v = surf, v1 = ( surf + 1 ) % B.poly.size();
		br.Pos( B.position.x + B.poly[ v ].x,  B.position.y + B.poly[ v ].y, B.z0 + B.position.z );
		br.Pos( B.position.x + B.poly[ v1 ].x, B.position.y + B.poly[ v1 ].y, B.z0 + B.position.z );
		
		br.Pos( B.position.x + B.poly[ v ].x, B.position.y + B.poly[ v ].y, B.z1 + B.position.z );
		br.Pos( B.position.x + B.poly[ v1 ].x,  B.position.y + B.poly[ v1 ].y, B.z1 + B.position.z );
	}
	
	br.Flush();
	GR2D_SetWorldMatrix( Mat4::Identity );
}

void EdWorld::DrawPoly_BlockVertex( int block, int vert, bool sel )
{
	BatchRenderer& br = GR2D_GetBatchRenderer();
	
	br.SetPrimitiveType( PT_Lines ).UnsetTexture();
	
	if( sel )
		br.Col( 0.9f, 0.5, 0.1f, 0.9f );
	else
		br.Col( 0.1f, 0.5, 0.9f, 0.5f );
	
	const EdBlock& B = m_blocks[ block ];
	GR2D_SetWorldMatrix( m_groupMgr.GetMatrix( B.group ) );
	Vec3 P = V3( B.poly[ vert ].x + B.position.x, B.poly[ vert ].y + B.position.y, B.z0 + B.position.z );
	
	float s = 0.5f;
	br.Pos( P - V3(s,0,0) ).Pos( P + V3(0,0,s) ).Prev(0).Pos( P + V3(s,0,0) ).Prev(0).Pos( P - V3(0,0,s) ).Prev(0).Prev(6);
	br.Pos( P - V3(0,s,0) ).Pos( P + V3(0,0,s) ).Prev(0).Pos( P + V3(0,s,0) ).Prev(0).Pos( P - V3(0,0,s) ).Prev(0).Prev(6);
	br.Pos( P - V3(s,0,0) ).Pos( P + V3(0,s,0) ).Prev(0).Pos( P + V3(s,0,0) ).Prev(0).Pos( P - V3(0,s,0) ).Prev(0).Prev(6);
}

void EdWorld::DrawWires_Entities( int hlmesh, int selmesh )
{
	BatchRenderer& br = GR2D_GetBatchRenderer().Reset();
	
	br.SetPrimitiveType( PT_Lines ).UnsetTexture();
	for( size_t i = 0; i < m_entities.size(); ++i )
	{
		if( (int) i == selmesh )
			br.Col( 0.9f, 0.5, 0.1f, 0.9f );
		else if( (int) i == hlmesh )
			br.Col( 0.1f, 0.5, 0.9f, 0.7f );
		else
			br.Col( 0.1f, 0.5, 0.9f, 0.25f );
		
		float q = 0.2f;
		Vec3 P = m_entities[ i ]->Pos();
		br.Pos( P - V3(q,0,0) ).Pos( P + V3(0,0,q) ).Prev(0).Pos( P + V3(q,0,0) ).Prev(0).Pos( P - V3(0,0,q) ).Prev(0).Prev(6);
		br.Pos( P - V3(0,q,0) ).Pos( P + V3(0,0,q) ).Prev(0).Pos( P + V3(0,q,0) ).Prev(0).Pos( P - V3(0,0,q) ).Prev(0).Prev(6);
		br.Pos( P - V3(q,0,0) ).Pos( P + V3(0,q,0) ).Prev(0).Pos( P + V3(q,0,0) ).Prev(0).Pos( P - V3(0,q,0) ).Prev(0).Prev(6);
	}
	
	Mat4& iv = g_EdScene->camera.mInvView;
	Vec3 axes[2] = { iv.TransformNormal( V3(1,0,0) ), iv.TransformNormal( V3(0,1,0) ) };
	
	br.Col( 1 );
	for( size_t i = 0; i < m_entities.size(); ++i )
	{
		br.SetTexture( m_entities[ i ]->m_iconTex );
		br.Sprite( m_entities[ i ]->Pos(), axes[0]*0.1f, axes[1]*0.1f );
	}
	
	// debug draw highlighted/selected entities
	if( selmesh >= 0 )
		m_entities[ selmesh ]->DebugDraw();
	if( hlmesh >= 0 && hlmesh != selmesh )
		m_entities[ hlmesh ]->DebugDraw();
	
	br.Flush();
}

bool EdWorld::RayBlocksIntersect( const Vec3& pos, const Vec3& dir, int searchfrom, float outdst[1], int outblock[1] )
{
	float ndst[1], mindst = FLT_MAX;
	int curblk = -1;
	if( searchfrom < 0 )
		searchfrom = m_blocks.size();
	for( int i = searchfrom - 1; i >= 0; --i )
	{
		if( m_blocks[ i ].RayIntersect( pos, dir, ndst ) && ndst[0] < mindst )
		{
			curblk = i;
			mindst = ndst[0];
		}
	}
	for( int i = m_blocks.size() - 1; i >= searchfrom; --i )
	{
		if( m_blocks[ i ].RayIntersect( pos, dir, ndst ) && ndst[0] < mindst )
		{
			curblk = i;
			mindst = ndst[0];
		}
	}
	if( outdst ) outdst[0] = mindst;
	if( outblock ) outblock[0] = curblk;
	return curblk != -1;
}

bool EdWorld::RayEntitiesIntersect( const Vec3& pos, const Vec3& dir, int searchfrom, float outdst[1], int outent[1] )
{
	float ndst[1], mindst = FLT_MAX;
	int curent = -1;
	if( searchfrom < 0 )
		searchfrom = m_entities.size();
	for( int i = searchfrom - 1; i >= 0; --i )
	{
		if( m_entities[ i ]->RayIntersect( pos, dir, ndst ) && ndst[0] < mindst )
		{
			curent = i;
			mindst = ndst[0];
		}
	}
	for( int i = m_entities.size() - 1; i >= searchfrom; --i )
	{
		if( m_entities[ i ]->RayIntersect( pos, dir, ndst ) && ndst[0] < mindst )
		{
			curent = i;
			mindst = ndst[0];
		}
	}
	outdst[0] = mindst;
	outent[0] = curent;
	return curent != -1;
}

void EdWorld::DeleteSelectedBlocks()
{
	size_t i = m_blocks.size();
	while( i > 0 )
	{
		i--;
		if( m_blocks[ i ].selected )
			m_blocks.uerase( i );
	}
}

bool EdWorld::DuplicateSelectedBlocksAndMoveSelection()
{
	size_t sz = m_blocks.size();
	for( size_t i = 0; i < sz; ++i )
	{
		if( m_blocks[ i ].selected )
		{
			EdBlock B = m_blocks[ i ];
			m_blocks[ i ].selected = false;
			m_blocks.push_back( B );
		}
	}
	return m_blocks.size() != sz;
}

int EdWorld::GetNumSelectedBlocks()
{
	// no specific perf requirements currently
	int ns = 0;
	for( size_t i = 0; i < m_blocks.size(); ++i )
		if( m_blocks[ i ].selected )
			ns++;
	return ns;
}

int EdWorld::GetOnlySelectedBlock()
{
	int sb = -1;
	int ns = 0;
	for( size_t i = 0; i < m_blocks.size(); ++i )
	{
		if( m_blocks[ i ].selected )
		{
			if( ns++ )
				return -1;
			sb = i;
		}
	}
	return sb;
}

bool EdWorld::GetSelectedBlockAABB( Vec3 outaabb[2] )
{
	bool ret = false;
	outaabb[0] = V3(FLT_MAX);
	outaabb[1] = V3(-FLT_MAX);
	for( size_t i = 0; i < m_blocks.size(); ++i )
	{
		EdBlock& B = m_blocks[ i ];
		if( B.selected == false )
			continue;
		Mat4 gwm = m_groupMgr.GetMatrix( B.group );
		for( int v = 0; v < B.GetNumVerts(); ++v )
		{
			Vec3 p = gwm.TransformPos( B.GetLocalVertex( v ) );
			outaabb[0] = Vec3::Min( outaabb[0], p );
			outaabb[1] = Vec3::Max( outaabb[1], p );
		}
		ret = B.GetNumVerts() != 0;
	}
	return ret;
}

void EdWorld::SelectBlock( int block, bool mod )
{
	if( mod )
	{
		if( block != -1 )
			m_blocks[ block ].selected = !m_blocks[ block ].selected;
	}
	else
	{
		for( size_t i = 0; i < m_blocks.size(); ++i )
			m_blocks[ i ].selected = false;
		if( block != -1 )
			m_blocks[ block ].selected = true;
	}
}

Vec3 EdWorld::FindCenterOfGroup( int32_t grp )
{
	Vec3 cp = V3(0);
	int count = 0;
	for( size_t i = 0; i < m_blocks.size(); ++i )
	{
		if( m_blocks[ i ].group == grp )
		{
			cp += m_blocks[ i ].FindCenter();
			count++;
		}
	}
	/*TODO ENTITIES*/
	if( count )
		cp /= count;
	return cp;
}

void EdWorld::FixTransformsOfGroup( int32_t grp )
{
	for( size_t i = 0; i < m_blocks.size(); ++i )
	{
		if( m_blocks[ i ].group == grp )
			m_blocks[ i ].RegenerateMesh();
	}
	/*TODO ENTITIES*/
}

void EdWorld::CopyObjectsToGroup( int32_t grpfrom, int32_t grpto )
{
	size_t oldsize = m_blocks.size();
	for( size_t i = 0; i < oldsize; ++i )
	{
		if( m_blocks[ i ].group == grpfrom )
		{
			EdBlock blk = m_blocks[ i ];
			blk.group = grpto;
			blk.cached_mesh = NULL;
			blk.cached_meshinst = NULL;
			blk.RegenerateMesh();
			m_blocks.push_back( blk );
		}
	}
	/*TODO ENTITIES*/
}

void EdWorld::TransferObjectsToGroup( int32_t grpfrom, int32_t grpto )
{
	for( size_t i = 0; i < m_blocks.size(); ++i )
	{
		if( m_blocks[ i ].group == grpfrom )
		{
			m_blocks[ i ].group = grpto;
			m_blocks[ i ].RegenerateMesh();
		}
	}
	/*TODO ENTITIES*/
}

void EdWorld::DeleteObjectsInGroup( int32_t grp )
{
	for( size_t i = 0; i < m_blocks.size(); ++i )
	{
		if( m_blocks[ i ].group == grp )
		{
			m_blocks.uerase( i-- );
		}
	}
	/*TODO ENTITIES*/
}

void EdWorld::ExportGroupAsOBJ( int32_t grp, const StringView& name )
{
	OBJExporter objex;
	for( size_t i = 0; i < m_blocks.size(); ++i )
	{
		if( m_blocks[ i ].group == grp )
		{
			m_blocks[ i ].Export( objex );
		}
	}
	objex.Save( name, "Exported from SGRX editor" );
}



EDGUIMainFrame::EDGUIMainFrame() :
	m_editTF( NULL ),
	m_editMode( NULL ),
	m_keyMod( 0 ),
	m_UIMenuSplit( true, 26, 0 ),
	m_UIParamSplit( false, 0, 0.7f ),
	m_UIRenderView( g_EdScene, this )
{
	tyname = "mainframe";
	
	Add( &m_UIMenuSplit );
	m_UIMenuSplit.SetFirstPane( &m_UIMenuButtons );
	m_UIMenuSplit.SetSecondPane( &m_UIParamSplit );
	m_UIParamSplit.SetFirstPane( &m_UIRenderView );
	m_UIParamSplit.SetSecondPane( &m_UIParamList );
	
	// menu
	m_MB_Cat0.caption = "File:";
	m_MBNew.caption = "New";
	m_MBOpen.caption = "Open";
	m_MBSave.caption = "Save";
	m_MBSaveAs.caption = "Save As";
	m_MBCompile.caption = "Compile";
	m_MB_Cat1.caption = "Edit:";
	m_MBDrawBlock.caption = "Draw Block";
	m_MBEditBlock.caption = "Edit Block";
	m_MBPaintSurfs.caption = "Paint surfaces";
	m_MBAddEntity.caption = "Add Entity";
	m_MBEditEntity.caption = "Edit Entity";
	m_MBEditGroups.caption = "Edit groups";
	m_MBLevelInfo.caption = "Level Info";
	m_UIMenuButtons.Add( &m_MB_Cat0 );
	m_UIMenuButtons.Add( &m_MBNew );
	m_UIMenuButtons.Add( &m_MBOpen );
	m_UIMenuButtons.Add( &m_MBSave );
	m_UIMenuButtons.Add( &m_MBSaveAs );
	m_UIMenuButtons.Add( &m_MBCompile );
	m_UIMenuButtons.Add( &m_MB_Cat1 );
	m_UIMenuButtons.Add( &m_MBDrawBlock );
	m_UIMenuButtons.Add( &m_MBEditBlock );
	m_UIMenuButtons.Add( &m_MBPaintSurfs );
	m_UIMenuButtons.Add( &m_MBAddEntity );
	m_UIMenuButtons.Add( &m_MBEditEntity );
	m_UIMenuButtons.Add( &m_MBEditGroups );
	m_UIMenuButtons.Add( &m_MBLevelInfo );
	
	m_txMarker = GR_GetTexture( "editor/marker.png" );
}

void EDGUIMainFrame::PostInit()
{
	SetEditMode( &m_emEditLevel );
}

int EDGUIMainFrame::OnEvent( EDGUIEvent* e )
{
	switch( e->type )
	{
	case EDGUI_EVENT_BTNCLICK:
		if(0);
		
		else if( e->target == &m_MBNew ) Level_New();
		else if( e->target == &m_MBOpen ) Level_Open();
		else if( e->target == &m_MBSave ) Level_Save();
		else if( e->target == &m_MBSaveAs ) Level_SaveAs();
		else if( e->target == &m_MBCompile ) Level_Compile();
		
		else if( e->target == &m_MBDrawBlock ) SetEditMode( &m_emDrawBlock );
		else if( e->target == &m_MBEditBlock ) SetEditMode( &m_emEditBlock );
		else if( e->target == &m_MBPaintSurfs ) SetEditMode( &m_emPaintSurfs );
		else if( e->target == &m_MBAddEntity ) SetEditMode( &m_emAddEntity );
		else if( e->target == &m_MBEditEntity ) SetEditMode( &m_emEditEntity );
		else if( e->target == &m_MBEditGroups ) SetEditMode( &m_emEditGroup );
		else if( e->target == &m_MBLevelInfo ) SetEditMode( &m_emEditLevel );
		
		return 1;
		
	case EDGUI_EVENT_PROPCHANGE:
		if( e->target == g_UILevelOpenPicker )
		{
			Level_Real_Open( g_UILevelOpenPicker->GetValue() );
		}
		if( e->target == g_UILevelSavePicker )
		{
			Level_Real_Save( g_UILevelSavePicker->GetValue() );
		}
		return 1;
	}
	if( m_editMode )
	{
		if( m_editMode->OnUIEvent( e ) )
			return 1;
	}
	return EDGUIFrame::OnEvent( e );
}

bool EDGUIMainFrame::ViewEvent( EDGUIEvent* e )
{
	if( e->type == EDGUI_EVENT_KEYDOWN || e->type == EDGUI_EVENT_KEYUP )
	{
		int mod = 0;
		switch( e->key.engkey )
		{
		case SDLK_LSHIFT: mod = KMOD_LSHIFT; break;
		case SDLK_RSHIFT: mod = KMOD_RSHIFT; break;
		case SDLK_LCTRL: mod = KMOD_LCTRL; break;
		case SDLK_RCTRL: mod = KMOD_RCTRL; break;
		case SDLK_LALT: mod = KMOD_LALT; break;
		case SDLK_RALT: mod = KMOD_RALT; break;
		case SDLK_LGUI: mod = KMOD_LGUI; break;
		case SDLK_RGUI: mod = KMOD_RGUI; break;
		}
		if( e->type == EDGUI_EVENT_KEYDOWN )
			m_keyMod |= mod;
		else
			m_keyMod &= ~mod;
	}
	
	if( m_editTF )
	{
		if( m_editTF->OnViewEvent( e ) )
			return false;
	}
	
	if( m_editMode )
		m_editMode->OnViewEvent( e );
	
	return true;
}

void EDGUIMainFrame::_DrawCursor( bool drawimg, float height )
{
	BatchRenderer& br = GR2D_GetBatchRenderer();
	br.UnsetTexture();
	if( IsCursorAiming() )
	{
		Vec2 pos = GetCursorPlanePos();
		if( drawimg )
		{
			br.SetTexture( m_txMarker ).Col( 0.9f, 0.1f, 0, 0.9f );
			br.Box( pos.x, pos.y, 1, 1, height );
		}
		br.UnsetTexture().SetPrimitiveType( PT_Lines );
		// up
		br.Col( 0, 0.1f, 0.9f, 0.9f ).Pos( pos.x, pos.y, height );
		br.Col( 0, 0.1f, 0.9f, 0.0f ).Pos( pos.x, pos.y, 4 + height );
		// -X
		br.Col( 0.5f, 0.1f, 0, 0.9f ).Pos( pos.x, pos.y, height );
		br.Col( 0.5f, 0.1f, 0, 0.0f ).Pos( pos.x - 4, pos.y, height );
		// +X
		br.Col( 0.9f, 0.1f, 0, 0.9f ).Pos( pos.x, pos.y, height );
		br.Col( 0.9f, 0.1f, 0, 0.0f ).Pos( pos.x + 4, pos.y, height );
		// -Y
		br.Col( 0.1f, 0.5f, 0, 0.9f ).Pos( pos.x, pos.y, height );
		br.Col( 0.1f, 0.5f, 0, 0.0f ).Pos( pos.x, pos.y - 4, height );
		// +Y
		br.Col( 0.1f, 0.9f, 0, 0.9f ).Pos( pos.x, pos.y, height );
		br.Col( 0.1f, 0.9f, 0, 0.0f ).Pos( pos.x, pos.y + 4, height );
	}
}

void EDGUIMainFrame::DrawCursor( bool drawimg )
{
	_DrawCursor( drawimg, 0 );
	if( m_UIRenderView.crplaneheight )
		_DrawCursor( drawimg, m_UIRenderView.crplaneheight );
}

void EDGUIMainFrame::DebugDraw()
{
	if( m_editMode )
		m_editMode->Draw();
	if( m_editTF )
		m_editTF->Draw();
}


void EDGUIMainFrame::AddToParamList( EDGUIItem* item )
{
	m_UIParamList.Add( item );
}

void EDGUIMainFrame::ClearParamList()
{
	while( m_UIParamList.m_subitems.size() )
		m_UIParamList.Remove( m_UIParamList.m_subitems.last() );
}

void EDGUIMainFrame::RefreshMouse()
{
	m_frame->_HandleMouseMove( false );
}

Vec3 EDGUIMainFrame::GetCursorRayPos()
{
	return m_UIRenderView.crpos;
}

Vec3 EDGUIMainFrame::GetCursorRayDir()
{
	return m_UIRenderView.crdir;
}

Vec2 EDGUIMainFrame::GetCursorPlanePos()
{
	return Snapped( m_UIRenderView.cursor_hpos );
}

float EDGUIMainFrame::GetCursorPlaneHeight()
{
	return m_UIRenderView.crplaneheight;
}

void EDGUIMainFrame::SetCursorPlaneHeight( float z )
{
	m_UIRenderView.crplaneheight = z;
}

bool EDGUIMainFrame::IsCursorAiming()
{
	return m_UIRenderView.cursor_aim;
}

void EDGUIMainFrame::Snap( Vec2& v )
{
	m_snapProps.Snap( v );
}

void EDGUIMainFrame::Snap( Vec3& v )
{
	m_snapProps.Snap( v );
}

Vec2 EDGUIMainFrame::Snapped( const Vec2& v )
{
	Vec2 o = v;
	Snap( o );
	return o;
}

Vec3 EDGUIMainFrame::Snapped( const Vec3& v )
{
	Vec3 o = v;
	Snap( o );
	return o;
}


void EDGUIMainFrame::ResetEditorState()
{
	PostInit();
}

void EDGUIMainFrame::Level_New()
{
	g_UIScrFnPicker->m_levelName = m_fileName = "";
	g_EdWorld->Reset();
	ResetEditorState();
}

void EDGUIMainFrame::Level_Open()
{
	g_UILevelOpenPicker->Reload();
	g_UILevelOpenPicker->Open( this, "" );
	m_frame->Add( g_UILevelOpenPicker );
}

void EDGUIMainFrame::Level_Save()
{
	if( m_fileName.size() )
	{
		Level_Real_Save( m_fileName );
	}
	else
	{
		Level_SaveAs();
	}
}

void EDGUIMainFrame::Level_SaveAs()
{
	g_UILevelSavePicker->Reload();
	g_UILevelSavePicker->Open( this, "" );
	m_frame->Add( g_UILevelSavePicker );
}

void EDGUIMainFrame::Level_Compile()
{
	if( m_fileName.size() )
	{
		Level_Real_Compile();
	}
	else
	{
		Level_SaveAs();
	}
}

void EDGUIMainFrame::Level_Real_Open( const String& str )
{
	LOG << "Trying to open level: " << str;
	
	char bfr[ 256 ];
	snprintf( bfr, sizeof(bfr), "levels/%.*s.tle", TMIN( (int) str.size(), 200 ), str.data() );
	String data;
	if( !FS_LoadTextFile( bfr, data ) )
	{
		LOG_ERROR << "FAILED TO LOAD LEVEL FILE: " << bfr;
		return;
	}
	
	ResetEditorState();
	
	TextReader tr( &data );
	g_EdWorld->Serialize( tr );
	if( tr.error )
	{
		LOG_ERROR << "FAILED TO READ LEVEL FILE (at " << (int) tr.pos << "): " << bfr;
		return;
	}
	
	g_UIScrFnPicker->m_levelName = m_fileName = str;
}

void EDGUIMainFrame::Level_Real_Save( const String& str )
{
	LOG << "Trying to save level: " << str;
	String data;
	TextWriter arch( &data );
	
	arch << *g_EdWorld;
	
	char bfr[ 256 ];
	snprintf( bfr, sizeof(bfr), "levels/%.*s.tle", TMIN( (int) str.size(), 200 ), str.data() );
	if( !FS_SaveTextFile( bfr, data ) )
	{
		LOG_ERROR << "FAILED TO SAVE LEVEL FILE: " << bfr;
		return;
	}
	
	g_UIScrFnPicker->m_levelName = m_fileName = str;
}

void EDGUIMainFrame::Level_Real_Compile()
{
	LOG << "Compiling level";
	LevelCache lcache;
	
	lcache.AmbientColor = HSV( g_EdWorld->m_ctlAmbientColor.m_value );
	lcache.LightmapClearColor = HSV( g_EdWorld->m_ctlLightmapClearColor.m_value );
	lcache.RADNumBounces = g_EdWorld->m_ctlRADNumBounces.m_value;
	lcache.LightmapDetail = g_EdWorld->m_ctlLightmapDetail.m_value;
	lcache.LightmapBlurSize = g_EdWorld->m_ctlLightmapBlurSize.m_value;
	lcache.AODistance = g_EdWorld->m_ctlAODistance.m_value;
	lcache.AOMultiplier = g_EdWorld->m_ctlAOMultiplier.m_value;
	lcache.AOFalloff = g_EdWorld->m_ctlAOFalloff.m_value;
	lcache.AOEffect = g_EdWorld->m_ctlAOEffect.m_value;
//	lcache.AODivergence = g_EdWorld->m_ctlAODivergence.m_value;
	lcache.AOColor = HSV( g_EdWorld->m_ctlAOColor.m_value );
	lcache.AONumSamples = g_EdWorld->m_ctlAONumSamples.m_value;
	
	// DIRECTIONAL LIGHT
	if( g_EdWorld->m_ctlDirLightColor.m_value.z && g_EdWorld->m_ctlDirLightNumSamples.m_value )
	{
		LC_Light L;
		L.type = LM_LIGHT_DIRECT;
		L.range = 1024;
		Vec2 dir = g_EdWorld->m_ctlDirLightDir.m_value;
		L.dir = -V3( dir.x, dir.y, -1 ).Normalized();
		L.color = HSV( g_EdWorld->m_ctlDirLightColor.m_value );
		L.light_radius = g_EdWorld->m_ctlDirLightDivergence.m_value / 180.0f;
		L.num_shadow_samples = g_EdWorld->m_ctlDirLightNumSamples.m_value;
		lcache.AddLight( L );
	}
	
	for( size_t i = 0; i < g_EdWorld->m_blocks.size(); ++i )
		g_EdWorld->m_blocks[ i ].GenerateMesh( lcache );
	
	for( size_t i = 0; i < g_EdWorld->m_entities.size(); ++i )
		g_EdWorld->m_entities[ i ]->UpdateCache( lcache );
	
	char bfr[ 256 ];
	snprintf( bfr, sizeof(bfr), "levels/%.*s", TMIN( (int) m_fileName.size(), 200 ), m_fileName.data() );
	
	if( !lcache.SaveCache( bfr ) )
		LOG_ERROR << "FAILED TO SAVE CACHE";
	else
		LOG << "Level is compiled";
}

void EDGUIMainFrame::SetEditMode( EdEditMode* em )
{
	if( m_editMode )
		m_editMode->OnExit();
	m_editMode = em;
	ClearParamList();
	em->OnEnter();
}

void EDGUIMainFrame::SetEditTransform( EdEditTransform* et )
{
	if( m_editTF )
	{
		m_editTF->OnExit();
		if( m_editMode )
			m_editMode->OnTransformEnd();
	}
	m_editTF = et && et->OnEnter() ? et : NULL;
}

void EDGUIMainFrame::SetModeHighlight( EDGUIButton* mybtn )
{
	m_MBDrawBlock.SetHighlight( mybtn == &m_MBDrawBlock );
	m_MBEditBlock.SetHighlight( mybtn == &m_MBEditBlock );
	m_MBPaintSurfs.SetHighlight( mybtn == &m_MBPaintSurfs );
	m_MBAddEntity.SetHighlight( mybtn == &m_MBAddEntity );
	m_MBEditEntity.SetHighlight( mybtn == &m_MBEditEntity );
	m_MBEditGroups.SetHighlight( mybtn == &m_MBEditGroups );
	m_MBLevelInfo.SetHighlight( mybtn == &m_MBLevelInfo );
}


//
// EDITOR ENTRY POINT
//

SGRX_RenderPass g_RenderPasses_Main[] =
{
	{ RPT_OBJECT, RPF_MTL_SOLID | RPF_OBJ_STATIC | RPF_ENABLED, 1, 0, 0, "base" },
	{ RPT_OBJECT, RPF_MTL_SOLID | RPF_OBJ_DYNAMIC | RPF_ENABLED | RPF_CALC_DIRAMB, 1, 0, 0, "base" },
	{ RPT_OBJECT, RPF_MTL_TRANSPARENT | RPF_OBJ_STATIC | RPF_ENABLED, 1, 0, 0, "base" },
	{ RPT_OBJECT, RPF_MTL_TRANSPARENT | RPF_OBJ_DYNAMIC | RPF_ENABLED | RPF_CALC_DIRAMB, 1, 0, 0, "base" },
};

struct TACStrikeEditor : IGame
{
	void OnInitialize()
	{
		GR_SetRenderPasses( g_RenderPasses_Main, SGRX_ARRAY_SIZE( g_RenderPasses_Main ) );
		
		GR2D_SetFont( "fonts/lato-regular.ttf", 12 );
		
		g_ScriptCtx = new ScriptContext;
		g_ScriptCtx->RegisterBatchRenderer();
		sgs_RegFuncConsts( g_ScriptCtx->C, g_ent_scripted_rfc, -1 );
		
		LOG << "\n\nLoading scripted entities:\n\n";
		g_ScriptCtx->ExecFile( "editor/entities.sgs" );
		LOG << "\nLoading completed\n\n";
		
		g_UISurfTexPicker = new EDGUISurfTexPicker;
		g_UIMeshPicker = new EDGUIMeshPicker;
		g_UIPartSysPicker = new EDGUIPartSysPicker;
		g_UISoundPicker = new EDGUISoundPicker;
		g_UIScrFnPicker = new EDGUIScrFnPicker( g_ScriptCtx );
		g_UILevelOpenPicker = new EDGUILevelOpenPicker;
		g_UILevelSavePicker = new EDGUILevelSavePicker;
		
		// core layout
		g_EdScene = GR_CreateScene();
		g_EdScene->camera.position = Vec3::Create(3,3,3);
		g_EdScene->camera.UpdateMatrices();
		g_EdWorld = new EdWorld();
		g_EdWorld->RegenerateMeshes();
		g_UIFrame = new EDGUIMainFrame();
		g_UIFrame->PostInit();
		g_UIFrame->Resize( GR_GetWidth(), GR_GetHeight() );
		
		// param area
		g_UIFrame->AddToParamList( g_EdWorld );
	}
	void OnDestroy()
	{
		delete g_UILevelSavePicker;
		g_UILevelSavePicker = NULL;
		delete g_UILevelOpenPicker;
		g_UILevelOpenPicker = NULL;
		delete g_UIScrFnPicker;
		g_UIScrFnPicker = NULL;
		delete g_UIPartSysPicker;
		g_UIPartSysPicker = NULL;
		delete g_UISoundPicker;
		g_UISoundPicker = NULL;
		delete g_UIMeshPicker;
		g_UIMeshPicker = NULL;
		delete g_UISurfTexPicker;
		g_UISurfTexPicker = NULL;
		delete g_UIFrame;
		g_UIFrame = NULL;
		delete g_EdWorld;
		g_EdWorld = NULL;
		g_EdScene = NULL;
		delete g_ScriptCtx;
		g_ScriptCtx = NULL;
	}
	void OnEvent( const Event& e )
	{
		g_UIFrame->EngineEvent( &e );
		g_EdWorld->m_groupMgr.ProcessDestroyQueue();
	}
	void OnTick( float dt, uint32_t gametime )
	{
		GR2D_SetViewMatrix( Mat4::CreateUI( 0, 0, GR_GetWidth(), GR_GetHeight() ) );
		g_UIFrame->m_UIRenderView.UpdateCamera( dt );
		g_UIFrame->Draw();
	}
}
g_Game;


extern "C" EXPORT IGame* CreateGame()
{
	return &g_Game;
}
