

#define MAPEDIT_DEFINE_GLOBALS
#include "mapedit.hpp"


bool g_DrawCovers = false;
int g_mode = LevelInfo;



EdGroup::EdGroup( struct EdGroupManager* groupMgr, int32_t id, int32_t pid, const StringView& name ) :
	m_groupMgr( groupMgr ),
	m_id( id ),
	m_parent_id( pid ),
	m_needsMtxUpdate( true ),
	m_mtxLocal( Mat4::Identity ),
	m_mtxCombined( Mat4::Identity ),
	m_needsPathUpdate( true ),
	m_name( name.str() ),
	m_parent( ( id ? groupMgr->GetPath( pid ) : StringView() ).str() ),
	m_origin( V3(0) ),
	m_position( V3(0) ),
	m_angles( V3(0) ),
	m_scaleUni( 1 )
{
}

Mat4 EdGroup::GetMatrix()
{
	if( m_needsMtxUpdate )
	{
		m_mtxCombined = m_mtxLocal = Mat4::CreateTranslation( -m_origin ) *
			Mat4::CreateSRT( V3( m_scaleUni ), DEG2RAD( m_angles ), m_position + m_origin );
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
	grp->m_origin = m_origin;
	grp->m_position = m_position;
	grp->m_angles = m_angles;
	grp->m_scaleUni = m_scaleUni;
	return grp;
}

void EdGroup::EditUI()
{
	// name/parent
	bool chgname = IMGUIEditString( "Name", m_name, 256 );
	bool chgprt = false;
	
	ImGui::BeginChangeCheck();
	if( m_id )
	{
		m_groupMgr->m_grpPicker.m_ignoreID = m_id;
		chgprt = m_groupMgr->m_grpPicker.Property( "Select parent group", "Parent", m_parent );
		if( chgprt )
		{
			m_parent_id = m_groupMgr->FindGroupByPath( m_parent )->m_id;
		}
	}
	
	if( chgname || chgprt )
	{
		m_groupMgr->PathInvalidate( m_id );
		m_groupMgr->m_editedGroup = GetPath();
	}
	
	// transform
	IMGUIEditVec3( "Origin", m_origin, -8192, 8192 );
	IMGUIEditVec3( "Position", m_position, -8192, 8192 );
	IMGUIEditVec3( "Rotation (XYZ)", m_angles, 0, 360 );
	IMGUIEditFloat( "Scale (uniform)", m_scaleUni, 0.01f, 100.0f );
	
	if( ImGui::EndChangeCheck() )
	{
		m_groupMgr->MatrixInvalidate( m_id );
	}
	
	// actions
	if( ImGui::Button( "Add child" ) )
	{
		m_groupMgr->PrepareEditGroup( m_groupMgr->AddGroup( m_id ) );
	}
	if( m_id )
	{
		if( ImGui::Button( "Delete group and subobjects -> parent" ) )
		{
			g_EdWorld->TransferObjectsToGroup( m_id, m_parent_id );
			m_groupMgr->TransferGroupsToGroup( m_id, m_parent_id );
			m_groupMgr->QueueDestroy( this );
		}
		if( ImGui::Button( "Delete group and subobjects -> root" ) )
		{
			g_EdWorld->TransferObjectsToGroup( m_id, 0 );
			m_groupMgr->TransferGroupsToGroup( m_id, 0 );
			m_groupMgr->QueueDestroy( this );
		}
		if( ImGui::Button( "Delete group and destroy subobjects" ) )
		{
			m_groupMgr->QueueDestroy( this );
		}
	}
	if( ImGui::Button( "Recalculate origin" ) )
	{
		m_origin = g_EdWorld->FindCenterOfGroup( m_id );
	}
	if( m_id )
	{
		if( ImGui::Button( "Clone group only" ) )
		{
			EdGroup* grp = Clone();
			m_groupMgr->PrepareEditGroup( grp );
		}
		if( ImGui::Button( "Clone group with subobjects" ) )
		{
			EdGroup* grp = Clone();
			g_EdWorld->CopyObjectsToGroup( m_id, grp->m_id );
			m_groupMgr->PrepareEditGroup( grp );
		}
	}
	if( ImGui::Button( "Export group as .obj" ) )
	{
		g_EdWorld->ExportGroupAsOBJ( m_id, "group.obj" );
	}
}


void IMGUIGroupPicker::Reload()
{
	m_entries.clear();
	for( size_t i = 0; i < m_groupMgr->m_groups.size(); ++i )
	{
		EdGroup* grp = m_groupMgr->m_groups.item( i ).value;
		if( grp->m_id != m_ignoreID && m_groupMgr->GroupHasParent( grp->m_id, m_ignoreID ) == false )
			m_entries.push_back( grp->GetPath() );
	}
	_Search( m_searchString );
}


EdGroupManager::EdGroupManager() :
	m_lastGroupID(-1)
{
	m_grpPicker.m_groupMgr = this;
}

void EdGroupManager::DrawGroups()
{
	BatchRenderer& br = GR2D_GetBatchRenderer();
	
	EdGroup* grp = FindGroupByPath( m_editedGroup );
	while( grp )
	{
		Mat4 parent_mtx = grp->m_id ? GetMatrix( grp->m_parent_id ) : Mat4::Identity;
		
		br.Col( 0.8f, 0.05f, 0.01f );
		br.Tick( grp->m_position + grp->m_origin, 0.1f, parent_mtx );
		br.Col( 0.05f, 0.8f, 0.01f );
		br.Tick( grp->m_position, 0.1f, parent_mtx );
		
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
	m_editedGroup = grp ? grp->GetPath() : "";
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

void EdGroupManager::EditUI()
{
	if( ImGui::Button( "Go to root" ) )
	{
		PrepareEditGroup( FindGroupByID( 0 ) );
	}
	m_grpPicker.m_ignoreID = -1;
	m_grpPicker.Property( "Select edited group", "Edited group", m_editedGroup );
	EdGroup* grp = FindGroupByPath( m_editedGroup );
	if( grp )
	{
		grp->EditUI();
	}
}

void EdGroupManager::GroupProperty( const char* label, int& group )
{
	EdGroup* grp = FindGroupByID( group );
	String gname = grp ? grp->m_name : "";
	m_grpPicker.m_ignoreID = -1;
	m_grpPicker.Property( "Select group", label, gname );
	grp = FindGroupByPath( gname );
	if( grp )
		group = grp->m_id;
}



void EdObject::UISelectElement( int i, bool mod )
{
	if( i >= 0 && i < GetNumElements() )
	{
		if( mod )
			SelectElement( i, !IsElementSelected( i ) );
		else
		{
			ClearSelection();
			SelectElement( i, true );
		}
		
	}
	else if( mod == false )
		ClearSelection();
}

void EdObject::ProjectSelectedVertices()
{
	Vec3 origin = g_EdScene->camera.position;
	int numverts = GetNumVerts();
	EdObjIdx skiplist[2] = { this, EdObjIdx() };
	for( int i = 0; i < numverts; ++i )
	{
		if( IsVertexSelected( i ) == false )
			continue;
		
		Vec3 pos = GetLocalVertex( i );
		
		Vec3 dir = ( pos - origin ).Normalized();
		float dst = FLT_MAX;
		float ndst = 0;
		int mask = SelMask_Blocks | SelMask_Patches;
		if( g_EdWorld->RayObjectsIntersect( origin, dir, EdObjIdx(), &ndst, NULL, skiplist, mask ) && ndst < dst )
			dst = ndst;
		
		if( dst < FLT_MAX )
		{
			dst -= g_UIFrame->m_snapProps.projDist;
			SetLocalVertex( i, origin + dir * dst );
		}
	}
}


EdPaintProps::EdPaintProps() :
	layerNum( 0 ),
	paintPos( false ),
	paintColor( false ),
	paintAlpha( true ),
	radius( 1 ),
	falloff( 1 ),
	sculptSpeed( 2 ),
	paintSpeed( 2 ),
	colorHSV( V3(0,0,1) ),
	alpha( 1 )
{
	_UpdateColor();
}

void EdPaintProps::EditUI()
{
	IMGUI_GROUP_BEGIN( "Painting properties", true )
	{
		ImGui::RadioButton( "0", &layerNum, 0 );
		ImGui::SameLine();
		ImGui::RadioButton( "1", &layerNum, 1 );
		ImGui::SameLine();
		ImGui::RadioButton( "2", &layerNum, 2 );
		ImGui::SameLine();
		ImGui::RadioButton( "3", &layerNum, 3 );
		ImGui::SameLine();
		ImGui::Text( "Layer" );
		
		ImGui::BeginChangeMask( 1 );
		
		if( ImGui::RadioButton( "##sculpt", paintPos ) ){
			paintPos = true; paintColor = false; paintAlpha = false; }
		ImGui::SameLine();
		IMGUIEditBool( "Sculpt", paintPos );
		
		if( ImGui::RadioButton( "##paintcolor", paintColor ) ){
			paintPos = false; paintColor = true; paintAlpha = false; }
		ImGui::SameLine();
		IMGUIEditBool( "Paint color", paintColor );
		
		if( ImGui::RadioButton( "##paintalpha", paintAlpha ) ){
			paintPos = false; paintColor = false; paintAlpha = true; }
		ImGui::SameLine();
		IMGUIEditBool( "Paint alpha", paintAlpha );
		
		_UpdateColor();
		
		ImGui::Separator();
		
		IMGUIEditFloatSlider( "Radius", radius, 0, 64, 2 );
		IMGUIEditFloatSlider( "Falloff", falloff, 0.01f, 100.0f, 10 );
		IMGUIEditFloatSlider( "Sculpt speed", sculptSpeed, 0.01f, 100.0f, 10 );
		IMGUIEditFloatSlider( "Paint speed", paintSpeed, 0.01f, 100.0f, 10 );
		IMGUIEditColorHSVHDR( "Color", colorHSV, 1 );
		IMGUIEditFloatSlider( "Alpha", alpha, 0, 1 );
		
		ImGui::EndChangeMask();
	}
	IMGUI_GROUP_END;
}

float EdPaintProps::GetDistanceFactor( const Vec3& vpos, const Vec3& bpos )
{
	float dist = ( vpos - bpos ).Length();
	return powf( 1 - clamp( dist / radius, 0, 1 ), falloff );
}

void EdPaintProps::Paint( Vec3& vpos, const Vec3& nrm, Vec4& vcol, float factor )
{
	if( paintPos )
	{
		vpos += nrm * factor * sculptSpeed * ( ImGui::GetIO().KeyAlt ? -1 : 1 );
	}
	if( paintColor || paintAlpha )
	{
		Vec4 cf = V4( V3( paintColor ), paintAlpha ) * clamp( paintSpeed * factor, 0, 1 );
		vcol = TLERP( vcol, ImGui::GetIO().KeyAlt ? V4(1) - m_tgtColor : m_tgtColor, cf );
	}
}

void EdPaintProps::_UpdateColor()
{
	m_tgtColor = V4( HSV( colorHSV ), alpha );
}



void ReconfigureEntities( StringView levname )
{
	g_Level->GetScriptCtx().Include( SGRXPATH_SRC_LEVELS "/_template" );
	if( levname )
	{
		char bfr[ 256 ];
		sgrx_snprintf( bfr, sizeof(bfr), SGRXPATH_SRC_LEVELS "/%s", StackString<240>(levname).str );
		g_Level->GetScriptCtx().Include( bfr );
	}
}


void EdWorldBasicInfo::FLoad( sgsVariable data )
{
	prefabMode = FLoadProp( data, "prefabMode", false );
}

sgsVariable EdWorldBasicInfo::FSave()
{
	sgsVariable data = FNewDict();
	FSaveProp( data, "prefabMode", prefabMode );
	return data;
}

void EdWorldBasicInfo::EditUI()
{
	IMGUI_GROUP( "Basic info", true,
	{
		IMGUIEditBool( "Prefab mode", prefabMode );
	});
}

void EdWorldLightingInfo::FLoad( sgsVariable lighting )
{
	ambientColor = FLoadProp( lighting, "ambientColor", V3( 0, 0, 0.1f ) );
	dirLightDir = FLoadProp( lighting, "dirLightDir", V2(0) );
	dirLightColor = FLoadProp( lighting, "dirLightColor", V3(0) );
	dirLightDvg = FLoadProp( lighting, "dirLightDvg", 10.0f );
	dirLightNumSamples = FLoadProp( lighting, "dirLightNumSamples", 15 );
	lightmapClearColor = FLoadProp( lighting, "lightmapClearColor", V3(0) );
	lightmapDetail = FLoadProp( lighting, "lightmapDetail", 1.0f );
	lightmapBlurSize = FLoadProp( lighting, "lightmapBlurSize", 1.0f );
	aoDist = FLoadProp( lighting, "aoDist", 2.0f );
	aoMult = FLoadProp( lighting, "aoMult", 1.0f );
	aoFalloff = FLoadProp( lighting, "aoFalloff", 2.0f );
	aoEffect = FLoadProp( lighting, "aoEffect", 0.0f );
	aoColor = FLoadProp( lighting, "aoColor", V3(0) );
	aoNumSamples = FLoadProp( lighting, "aoNumSamples", 15 );
	sampleDensity = FLoadProp( lighting, "sampleDensity", 1.0f );
	skyboxTexture = FLoadProp( lighting, "skyboxTexture", SV() );
	clutTexture = FLoadProp( lighting, "clutTexture", SV() );
}

sgsVariable EdWorldLightingInfo::FSave()
{
	sgsVariable lighting = FNewDict();
	{
		FSaveProp( lighting, "ambientColor", ambientColor );
		FSaveProp( lighting, "dirLightDir", dirLightDir );
		FSaveProp( lighting, "dirLightColor", dirLightColor );
		FSaveProp( lighting, "dirLightDvg", dirLightDvg );
		FSaveProp( lighting, "dirLightNumSamples", dirLightNumSamples );
		FSaveProp( lighting, "lightmapClearColor", lightmapClearColor );
		FSaveProp( lighting, "lightmapDetail", lightmapDetail );
		FSaveProp( lighting, "lightmapBlurSize", lightmapBlurSize );
		FSaveProp( lighting, "aoDist", aoDist );
		FSaveProp( lighting, "aoMult", aoMult );
		FSaveProp( lighting, "aoFalloff", aoFalloff );
		FSaveProp( lighting, "aoEffect", aoEffect );
		FSaveProp( lighting, "aoColor", aoColor );
		FSaveProp( lighting, "aoNumSamples", aoNumSamples );
		FSaveProp( lighting, "sampleDensity", sampleDensity );
		FSaveProp( lighting, "skyboxTexture", skyboxTexture );
		FSaveProp( lighting, "clutTexture", clutTexture );
	}
	return lighting;
}

void EdWorldLightingInfo::EditUI()
{
	IMGUI_GROUP( "Lighting info", true,
	{
		IMGUIEditColorHSVHDR( "Ambient color", ambientColor, 100 );
		IMGUIEditVec2( "Dir.light direction (dX,dY)", dirLightDir, -8192, 8192 );
		IMGUIEditColorHSVHDR( "Dir.light color", dirLightColor, 100 );
		IMGUIEditFloat( "Dir.light divergence", dirLightDvg, 0, 180 );
		IMGUIEditInt( "Dir.light sample count", dirLightNumSamples, 0, 256 );
		IMGUIEditColorHSVHDR( "Lightmap clear color", lightmapClearColor, 100 );
	//	IMGUIEditInt( "Radiosity bounce count", radNumBounces, 0, 256 );
		IMGUIEditFloat( "Lightmap detail", lightmapDetail, 0.01f, 16 );
		IMGUIEditFloat( "Lightmap blur size", lightmapBlurSize, 0, 10 );
		IMGUI_GROUP( "Ambient occlusion", true,
		{
			IMGUIEditFloat( "Distance", aoDist, 0, 100 );
			IMGUIEditFloat( "Multiplier", aoMult, 0, 2 );
			IMGUIEditFloat( "Falloff", aoFalloff, 0.01f, 100 );
			IMGUIEditFloat( "Effect", aoEffect, -1, 1 );
		//	IMGUIEditFloat( "Divergence", aoDivergence, 0, 1 );
			IMGUIEditColorHSVHDR( "Color", aoColor, 100 );
			IMGUIEditInt( "Sample count", aoNumSamples, 0, 256 );
		});
		IMGUIEditFloat( "Sample density", sampleDensity, 0.01f, 100 );
		if( g_NUITexturePicker->Property( "Pick a skybox texture", "Skybox texture", skyboxTexture ) )
			g_EdWorld->ReloadSkybox();
		if( g_NUITexturePicker->Property( "Pick default post-process cLUT", "Default post-process cLUT", clutTexture ) )
			g_EdWorld->ReloadCLUT();
	});
}

void EdWorld::EditUI()
{
	m_info.EditUI();
	SystemsParamsUI();
	m_lighting.EditUI();
}

void EdWorld::SystemsParamsUI()
{
	sgsVariable gui = g_Level->GetScriptCtx().GetGlobal( "ED_IMGUI" );
	IMGUI_GROUP_BEGIN( "Systems", true )
	{
		ScriptVarIterator it( g_Level->GetScriptCtx().GetGlobal( "ED_SYSTEM_TYPES" ) );
		while( it.Advance() )
		{
			sgsVariable key = it.GetKey();
			
			// data
			sgsVariable data = m_systemsParams.getprop( key );
			if( data.not_null() == false )
			{
				data = FNewDict();
				m_systemsParams.setprop( key, data );
			}
			
			sgsString name = key.get_string();
			sgsVariable func = it.GetValue();
			IMGUI_GROUP_BEGIN( name.c_str(), false )
			{
				gui.push( g_Level->GetSGSC() );
				data.thiscall( g_Level->GetSGSC(), func, 1 );
			}
			IMGUI_GROUP_END;
		}
	}
	IMGUI_GROUP_END;
}


EdWorld::EdWorld() :
	m_nextID( 0 )
{
	m_vd = GR_GetVertexDecl( LCVertex_DECL );
	
	ReconfigureEntities( "" );
	Reset();
	TestData();
}

EdWorld::~EdWorld()
{
	Reset();
}

void EdWorld::FLoad( sgsVariable obj )
{
	LOG_FUNCTION_ARG("EdWorld");
	
	Reset();
	
	g_Level->GetScriptCtx().GetGlobal( "ED_ILOAD" ).
		thiscall( g_Level->GetSGSC(), "_Restart" );
	
	int version = FLoadProp( obj, "version", 0 );
	m_nextID = FLoadProp( obj, "id", 0 );
	
	m_info.FLoad( obj.getprop("info") );
	m_lighting.FLoad( obj.getprop("lighting") );
	m_systemsParams = obj.getprop("systems");
	if( m_systemsParams.not_null() == false )
		m_systemsParams = FNewDict();
	
	sgsVariable objects = obj.getprop("objects");
	{
		ScriptVarIterator it( objects );
		while( it.Advance() )
		{
			sgsVariable object = it.GetValue();
			
			// 2nd upgrade
			g_Level->GetScriptCtx().Push( object );
			g_Level->GetScriptCtx().GlobalCall( "OBJ_UPG2", 1, 1 );
			object = sgsVariable( g_Level->GetScriptCtx().C, sgsVariable::PickAndPop );
			if( object.not_null() == false )
				continue; // already parsed some other way
			
			int type = object.getprop("type").get<int>();
			EdObject* obj = NULL;
			switch( type )
			{
			case ObjType_Block: obj = new EdBlock; break;
			case ObjType_Patch: obj = new EdPatch; break;
			case ObjType_MeshPath: obj = new EdMeshPath; break;
			case ObjType_Entity:
				obj = new EdEntity( object.getprop( "entity_type" ).get_string(), false );
				break;
			case ObjType_GameObject:
				EDGO_FLoad( object );
				goto NotObject;
			default:
				LOG_ERROR << "Failed to load World!";
				continue;
			}
			obj->FLoad( object, version );
			AddObject( obj, false );
NotObject:;
		}
		RegenerateMeshes();
	}
	
	g_Level->GetScriptCtx().GetGlobal( "ED_ILOAD" ).
		thiscall( g_Level->GetSGSC(), "_ResolveLinks" );
}

sgsVariable EdWorld::FSave()
{
	LOG_FUNCTION_ARG("EdWorld");
	
	int version = MAP_FILE_VERSION;
	
	sgsVariable objects = FNewArray();
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		EdObject* obj = m_edobjs[ i ];
		sgsVariable objdata = obj->FSave( version );
		FSaveProp( objdata, "type", obj->m_type );
		FArrayAppend( objects, objdata );
	}
	for( size_t i = 0; i < g_Level->m_gameObjects.size(); ++i )
	{
		GameObject* obj = g_Level->m_gameObjects[ i ];
		sgsVariable objdata = EDGO_FSave( obj );
		FSaveProp( objdata, "type", (int) ObjType_GameObject );
		FArrayAppend( objects, objdata );
	}
	
	sgsVariable out = FNewDict();
	FSaveProp( out, "version", version );
	FSaveProp( out, "id", m_nextID );
	out.setprop( "info", m_info.FSave() );
	out.setprop( "lighting", m_lighting.FSave() );
	out.setprop( "systems", m_systemsParams );
	out.setprop( "objects", objects );
	
	return out;
}

void EdWorld::Reset()
{
	while( m_edobjs.size() )
		DeleteObject( m_edobjs.last().item, false );
	while( g_Level->m_gameObjects.size() )
		DeleteObject( g_Level->m_gameObjects.last(), false );
	m_edobjs.clear();
	m_selection.clear();
	g_EdLGCont->Reset();
	m_nextID = 0;
	m_info = EdWorldBasicInfo();
	m_lighting = EdWorldLightingInfo();
	m_systemsParams = FNewDict();
	if( g_UIFrame )
		g_UIFrame->OnDeleteObjects();
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
	surf.texname = "null";
	for( int i = 0; i < 6; ++i )
		b1.surfaces.push_back( new EdSurface( surf ) );
	b1.subsel.resize( b1.GetNumElements() );
	b1.ClearSelection();
	
	AddObject( b1.Clone() );
	b1.z1 = 1;
	b1.position = V3( 0.1f, 1, 0.5f );
	AddObject( b1.Clone() );
	
	RegenerateMeshes();
}


void EdWorld::GetAllObjects( EdObjIdxArray& out )
{
	out.reserve( m_edobjs.size() + g_Level->m_gameObjects.size() );
	out.clear();
	for( size_t i = 0; i < m_edobjs.size(); ++i )
		out.push_back( m_edobjs[ i ].item );
	for( size_t i = 0; i < g_Level->m_gameObjects.size(); ++i )
		out.push_back( g_Level->m_gameObjects[ i ] );
}

void EdWorld::RegenerateMeshes()
{
	for( size_t i = 0; i < m_edobjs.size(); ++i )
		m_edobjs[ i ]->RegenerateMesh();
	// GameObject does not need this
}

void EdWorld::ReloadSkybox()
{
	g_Level->GetScene()->skyTexture = GR_GetTexture( m_lighting.skyboxTexture );
}

void EdWorld::ReloadCLUT()
{
	TextureHandle t = GR_GetTexture( m_lighting.clutTexture );
	if( !t )
		t = GR_GetTexture( "sys:lut_default" );
	g_Level->GetScene()->clutTexture = t;
}

void EdWorld::DrawWires_Objects( const EdObjIdx& hl, bool tonedown )
{
	DrawWires_Blocks( hl );
	DrawWires_Patches( hl, tonedown );
	DrawWires_Entities( hl );
	DrawWires_MeshPaths( hl );
	DrawWires_GameObjects( hl );
}

void EdWorld::DrawWires_Blocks( const EdObjIdx& hl )
{
	BatchRenderer& br = GR2D_GetBatchRenderer();
	
	br.SetPrimitiveType( PT_Lines ).UnsetTexture();
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->m_type != ObjType_Block )
			continue;
		SGRX_CAST( EdBlock*, pB, m_edobjs[ i ].item );
		EdBlock& B = *pB;
		GR2D_SetWorldMatrix( GetGroupMatrix( B.group ) );
		
		if( B.selected )
		{
			if( EdObjIdx(pB) == hl )
				br.Col( 0.9f, 0.2f, 0.1f, 1 );
			else
				br.Col( 0.9f, 0.5f, 0.1f, 1 );
		}
		else if( EdObjIdx(pB) == hl )
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

void EdWorld::DrawPoly_BlockSurf( const EdObjIdx& block, int surf, bool sel )
{
	if( block.type != ObjType_Block )
		return;
	const EdBlock& B = *(EdBlock*)block.GetEdObject();
	
	BatchRenderer& br = GR2D_GetBatchRenderer();
	
	br.SetPrimitiveType( PT_TriangleStrip ).UnsetTexture();
	
	if( sel )
		br.Col( 0.9f, 0.5, 0.1f, 0.2f );
	else
		br.Col( 0.1f, 0.5, 0.9f, 0.1f );
	
	GR2D_SetWorldMatrix( GetGroupMatrix( B.group ) );
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

void EdWorld::DrawPoly_BlockVertex( const EdObjIdx& block, int vert, bool sel )
{
	if( block.type != ObjType_Block )
		return;
	const EdBlock& B = *(EdBlock*)block.GetEdObject();
	
	BatchRenderer& br = GR2D_GetBatchRenderer();
	
	br.SetPrimitiveType( PT_Lines ).UnsetTexture();
	
	if( sel )
		br.Col( 0.9f, 0.5, 0.1f, 0.9f );
	else
		br.Col( 0.1f, 0.5, 0.9f, 0.5f );
	
	GR2D_SetWorldMatrix( GetGroupMatrix( B.group ) );
	Vec3 P = V3( B.poly[ vert ].x + B.position.x, B.poly[ vert ].y + B.position.y, B.z0 + B.position.z );
	
	float s = 0.5f;
	br.Pos( P - V3(s,0,0) ).Pos( P + V3(0,0,s) ).Prev(0).Pos( P + V3(s,0,0) ).Prev(0).Pos( P - V3(0,0,s) ).Prev(0).Prev(6);
	br.Pos( P - V3(0,s,0) ).Pos( P + V3(0,0,s) ).Prev(0).Pos( P + V3(0,s,0) ).Prev(0).Pos( P - V3(0,0,s) ).Prev(0).Prev(6);
	br.Pos( P - V3(s,0,0) ).Pos( P + V3(0,s,0) ).Prev(0).Pos( P + V3(s,0,0) ).Prev(0).Pos( P - V3(0,s,0) ).Prev(0).Prev(6);
}

void EdWorld::DrawWires_Patches( const EdObjIdx& hl, bool tonedown )
{
	float ga = tonedown ? 0.5f : 1;
	BatchRenderer& br = GR2D_GetBatchRenderer().Reset();
	
	br.SetPrimitiveType( PT_Lines ).UnsetTexture();
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->m_type != ObjType_Patch )
			continue;
		SGRX_CAST( EdPatch*, ptc, m_edobjs[ i ].item );
		
		GR2D_SetWorldMatrix( GetGroupMatrix( ptc->group ) );
		
		if( m_selection.isset( ptc ) )
			br.Col( 0.9f, 0.5, 0.1f, 0.9f * ga );
		else if( EdObjIdx(ptc) == hl )
			br.Col( 0.1f, 0.5, 0.9f, 0.7f * ga );
		else
			br.Col( 0.1f, 0.5, 0.9f, 0.25f * ga );
		
		// grid lines
		for( int y = 0; y < ptc->ysize; ++y )
		{
			for( int x = 0; x < ptc->xsize - 1; ++x )
			{
				br.Pos( ptc->vertices[ x + y * MAX_PATCH_WIDTH ].pos + ptc->position );
				br.Pos( ptc->vertices[ x + 1 + y * MAX_PATCH_WIDTH ].pos + ptc->position );
			}
		}
		for( int x = 0; x < ptc->xsize; ++x )
		{
			for( int y = 0; y < ptc->ysize - 1; ++y )
			{
				br.Pos( ptc->vertices[ x + y * MAX_PATCH_WIDTH ].pos + ptc->position );
				br.Pos( ptc->vertices[ x + ( y + 1 ) * MAX_PATCH_WIDTH ].pos + ptc->position );
			}
		}
		// inner edges
		for( int y = 0; y < ptc->ysize - 1; ++y )
		{
			for( int x = 0; x < ptc->xsize - 1; ++x )
			{
				if( ptc->edgeflip[ y ] & ( 1 << x ) )
				{
					br.Pos( ptc->vertices[ x + ( y + 1 ) * MAX_PATCH_WIDTH ].pos + ptc->position );
					br.Pos( ptc->vertices[ x + 1 + y * MAX_PATCH_WIDTH ].pos + ptc->position );
				}
				else
				{
					br.Pos( ptc->vertices[ x + y * MAX_PATCH_WIDTH ].pos + ptc->position );
					br.Pos( ptc->vertices[ x + 1 + ( y + 1 ) * MAX_PATCH_WIDTH ].pos + ptc->position );
				}
			}
		}
	}
	
	br.Flush();
	GR2D_SetWorldMatrix( Mat4::Identity );
}

void EdWorld::DrawWires_Entities( const EdObjIdx& hl )
{
	BatchRenderer& br = GR2D_GetBatchRenderer().Reset();
	
	br.SetPrimitiveType( PT_Lines );
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->m_type != ObjType_Entity )
			continue;
		SGRX_CAST( EdEntity*, ent, m_edobjs[ i ].item );
		
		if( m_selection.isset( ent ) )
			br.Col( 0.9f, 0.5, 0.1f, 0.9f );
		else if( EdObjIdx(ent) == hl )
			br.Col( 0.1f, 0.5, 0.9f, 0.7f );
		else
			br.Col( 0.1f, 0.5, 0.9f, 0.25f );
		
		float q = 0.2f;
		Vec3 P = ent->Pos();
		br.Pos( P - V3(q,0,0) ).Pos( P + V3(0,0,q) ).Prev(0).Pos( P + V3(q,0,0) ).Prev(0).Pos( P - V3(0,0,q) ).Prev(0).Prev(6);
		br.Pos( P - V3(0,q,0) ).Pos( P + V3(0,0,q) ).Prev(0).Pos( P + V3(0,q,0) ).Prev(0).Pos( P - V3(0,0,q) ).Prev(0).Prev(6);
		br.Pos( P - V3(q,0,0) ).Pos( P + V3(0,q,0) ).Prev(0).Pos( P + V3(q,0,0) ).Prev(0).Pos( P - V3(0,q,0) ).Prev(0).Prev(6);
	}
	
	Mat4& iv = g_EdScene->camera.mInvView;
	Vec3 axes[2] = { iv.TransformNormal( V3(1,0,0) ), iv.TransformNormal( V3(0,1,0) ) };
	
	br.Col( 1 );
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->m_type != ObjType_Entity )
			continue;
		SGRX_CAST( EdEntity*, ent, m_edobjs[ i ].item );
		
		br.SetTexture( ent->m_iconTex );
		br.Sprite( ent->Pos(), axes[0]*0.1f, axes[1]*0.1f );
	}
	
	// debug draw highlighted/selected entities
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->m_type != ObjType_Entity )
			continue;
		SGRX_CAST( EdEntity*, ent, m_edobjs[ i ].item );
		
		if( m_selection.isset( ent ) )
			ent->DebugDraw();
	}
	if( hl.GetEdObject() &&
		hl.GetEdObject()->m_type == ObjType_Entity &&
		!m_selection.isset( hl ) )
		((EdEntity*)hl.GetEdObject())->DebugDraw();
	
	br.Flush();
}

void EdWorld::DrawWires_MeshPaths( const EdObjIdx& hl )
{
	BatchRenderer& br = GR2D_GetBatchRenderer().Reset();
	
	br.SetPrimitiveType( PT_Lines );
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->m_type != ObjType_MeshPath )
			continue;
		SGRX_CAST( EdMeshPath*, mp, m_edobjs[ i ].item );
		if( m_selection.isset( mp ) )
			br.Col( 0.9f, 0.5, 0.1f, 0.9f );
		else if( EdObjIdx(mp) == hl )
			br.Col( 0.1f, 0.5, 0.9f, 0.7f );
		else
			br.Col( 0.1f, 0.5, 0.9f, 0.25f );
		
		float q = 0.2f;
		Vec3 P = mp->m_position;
		br.Pos( P - V3(q,0,0) ).Pos( P + V3(0,0,q) ).Prev(0).Pos( P + V3(q,0,0) ).Prev(0).Pos( P - V3(0,0,q) ).Prev(0).Prev(6);
		br.Pos( P - V3(0,q,0) ).Pos( P + V3(0,0,q) ).Prev(0).Pos( P + V3(0,q,0) ).Prev(0).Pos( P - V3(0,0,q) ).Prev(0).Prev(6);
		br.Pos( P - V3(q,0,0) ).Pos( P + V3(0,q,0) ).Prev(0).Pos( P + V3(q,0,0) ).Prev(0).Pos( P - V3(0,q,0) ).Prev(0).Prev(6);
		
		for( size_t p = 1; p < mp->m_points.size(); ++p )
		{
			br.Pos( mp->m_points[ p - 1 ].pos + P );
			br.Pos( mp->m_points[ p ].pos + P );
		}
	}
}

void EdWorld::DrawWires_GameObjects( const EdObjIdx& hl )
{
	BatchRenderer& br = GR2D_GetBatchRenderer().Reset();
	for( size_t i = 0; i < g_Level->m_gameObjects.size(); ++i )
	{
		GameObject* obj = g_Level->m_gameObjects[ i ];
		bool ishl = EdObjIdx(obj) == hl;
		bool issel = m_selection.isset( obj );
		if( ishl && issel )
			br.Col( 0.9f, 0.5, 0.9f, 0.7f );
		else if( ishl )
			br.Col( 0.1f, 0.5, 0.9f, 0.7f );
		else if( issel )
			br.Col( 0.9f, 0.5, 0.1f, 0.9f );
		else
			br.Col( 0.1f, 0.5, 0.9f, 0.25f );
		br.SphereOutline( obj->GetWorldPosition(), 0.2f, 32 );
	}
	
	for( size_t i = 0; i < g_Level->m_gameObjects.size(); ++i )
	{
		GameObject* obj = g_Level->m_gameObjects[ i ];
		if( m_selection.isset( obj ) )
			obj->EditorDrawWorld();
		else if( EdObjIdx(obj) == hl )
			obj->EditorDrawWorld();
	}
	
	// draw resource info
	Mat4& iv = g_EdScene->camera.mInvView;
	Vec3 axes[2] = { iv.TransformNormal( V3(1,0,0) ), iv.TransformNormal( V3(0,1,0) ) };
	
	br.Col( 1 );
	for( size_t i = 0; i < g_Level->m_gameObjects.size(); ++i )
	{
		GameObject* obj = g_Level->m_gameObjects[ i ];
		
		for( size_t j = 0; j < obj->m_resources.size(); ++j )
		{
			GOResource* rsrc = obj->m_resources[ j ];
			
			// icon
			sgsVariable texpathvar = rsrc->GetScriptedObject().getprop( "ED_Icon" );
			StringView texpath = texpathvar.getdef( SV( SGRXPATH_SRC_EDITOR "/icons/default.png" ) );
			TextureHandle th = GR_GetTexture( texpath );
			GR_PreserveResource( th );
			
			br.SetTexture( th );
			br.Sprite( rsrc->EditorIconPos(), axes[0]*0.1f, axes[1]*0.1f );
		}
	}
}


static bool ObjInArray( const EdObjIdx& obj, EdObjIdx* list )
{
	while( list->Valid() )
	{
		if( obj == *list )
			return true;
		list++;
	}
	return false;
}
bool _RayIntersect( const EdObjIdx& item, const Vec3& pos, const Vec3& dir, float outdst[1] )
{
	if( item.GetGameObject() )
		return RaySphereIntersect( pos, dir, item.GetGameObject()->GetWorldPosition(), 0.2f, outdst );
	else if( item.GetEdObject() )
		return item.GetEdObject()->RayIntersect( pos, dir, outdst );
	return false;
}
int Obj2Mask( const EdObjIdx& idx )
{
	if( idx.type == ObjType_Block ) return SelMask_Blocks;
	if( idx.type == ObjType_Patch ) return SelMask_Patches;
	if( idx.type == ObjType_MeshPath ) return SelMask_MeshPaths;
	if( idx.type == ObjType_Entity ) return SelMask_Entities;
	if( idx.type == ObjType_GameObject ) return SelMask_GameObjects;
	return 0;
}

bool EdWorld::RayObjectsIntersect( const Vec3& pos, const Vec3& dir,
	EdObjIdx searchfrom, float outdst[1], EdObjIdx outobj[1], EdObjIdx* skip, int mask )
{
	EdObjIdxArray items;
	GetAllObjects( items );
	
	float ndst[1], mindst = FLT_MAX;
	int curblk = -1;
	
	int searchpoint = 0;
	if( searchfrom.Valid() )
	{
		for( int i = 0; i < (int) items.size(); ++i )
		{
			if( items[ i ] == searchfrom )
			{
				searchpoint = i;
				break;
			}
		}
	}
	
	// search after starting point in reverse
	for( int i = searchpoint - 1; i >= 0; --i )
	{
		if( skip && ObjInArray( items[ i ], skip ) )
			continue;
		if( ( Obj2Mask( items[ i ] ) & mask ) == 0 )
			continue;
		if( _RayIntersect( items[ i ], pos, dir, ndst ) && ndst[0] < mindst )
		{
			curblk = i;
			mindst = ndst[0];
		}
	}
	
	// continue search from other end until starting point in reverse
	for( int i = items.size() - 1; i >= searchpoint; --i )
	{
		if( skip && ObjInArray( items[ i ], skip ) )
			continue;
		if( ( Obj2Mask( items[ i ] ) & mask ) == 0 )
			continue;
		if( _RayIntersect( items[ i ], pos, dir, ndst ) && ndst[0] < mindst )
		{
			curblk = i;
			mindst = ndst[0];
		}
	}
	
	if( outdst ) outdst[0] = mindst;
	if( outobj ) outobj[0] = curblk >= 0 ? items[ curblk ] : EdObjIdx();
	return curblk != -1;
}

void EdWorld::AddObject( EdObject* obj, bool regen )
{
	m_edobjs.push_back( obj );
	if( regen )
		obj->RegenerateMesh();
}

void EdWorld::DeleteObject( EdObjIdx idx, bool update )
{
	if( idx.type == ObjType_NONE )
		return;
	
	if( idx.type == ObjType_GameObject )
	{
		m_selection.unset( idx.gameobj );
		g_Level->DestroyGameObject( idx.gameobj );
	}
	else
	{
		m_selection.unset( idx.edobj );
		m_edobjs.remove_first( idx.edobj );
	}
	
	if( g_UIFrame )
	{
		if( g_UIFrame->m_emEditObjs.m_hlObj == idx )
			g_UIFrame->m_emEditObjs.m_hlObj.Unset();
		if( g_UIFrame->m_emEditObjs.m_curObj == idx )
			g_UIFrame->m_emEditObjs.m_curObj.Unset();
		if( update )
			g_UIFrame->OnDeleteObjects();
	}
}

void EdWorld::DeleteSelectedObjects()
{
	Array< EdObjIdx > sel;
	sel.reserve( m_selection.size() );
	for( size_t i = 0; i < m_selection.size(); ++i )
		sel.push_back( m_selection.item( i ).key );
	for( size_t i = 0; i < sel.size(); ++i )
		DeleteObject( sel[ i ], false );
	
	if( g_UIFrame )
		g_UIFrame->OnDeleteObjects();
}

bool EdWorld::DuplicateSelectedObjectsAndMoveSelection()
{
	Array< EdObjIdx > newsel;
	for( size_t i = 0; i < m_selection.size(); ++i )
	{
		EdObjIdx idx = m_selection.item( i ).key;
		if( idx.type == ObjType_GameObject )
		{
			GameObject* obj = EDGO_Clone( idx.GetGameObject() );
			newsel.push_back( obj );
		}
		else
		{
			EdObject* obj = idx.GetEdObject()->Clone();
			newsel.push_back( obj );
			AddObject( obj );
		}
	}
	
	m_selection.clear();
	for( size_t i = 0; i < newsel.size(); ++i )
		m_selection.set( newsel[ i ], NoValue() );
	
	return newsel.size() != 0;
}

int EdWorld::GetNumSelectedObjects()
{
	return m_selection.size();
}

EdObjIdx EdWorld::GetOnlySelectedObject()
{
	if( m_selection.size() != 1 )
		return EdObjIdx();
	return m_selection.item( 0 ).key;
}

bool EdWorld::GetSelectedObjectAABB( Vec3 outaabb[2] )
{
	bool ret = false;
	outaabb[0] = V3(FLT_MAX);
	outaabb[1] = V3(-FLT_MAX);
	for( size_t i = 0; i < m_selection.size(); ++i )
	{
		EdObjIdx idx = m_selection.item( i ).key;
		if( idx.type == ObjType_GameObject )
		{
			Vec3 p = idx.GetGameObject()->GetWorldPosition();
			outaabb[0] = Vec3::Min( outaabb[0], p );
			outaabb[1] = Vec3::Max( outaabb[1], p );
		}
		else
		{
			EdObject* obj = idx.GetEdObject();
			if( obj )
			{
				Mat4 gwm = GetGroupMatrix( obj->group );
				for( int v = 0; v < obj->GetNumVerts(); ++v )
				{
					Vec3 p = gwm.TransformPos( obj->GetLocalVertex( v ) );
					outaabb[0] = Vec3::Min( outaabb[0], p );
					outaabb[1] = Vec3::Max( outaabb[1], p );
				}
				ret = obj->GetNumVerts() != 0;
			}
		}
	}
	return ret;
}

void EdWorld::SelectObject( EdObjIdx idx, int mod )
{
	if( mod == SELOBJ_ONLY )
	{
		m_selection.clear();
		if( idx.Valid() )
			m_selection.set( idx, NoValue() );
	}
	else
	{
		if( idx.Valid() )
		{
			bool sel;
			if( mod == SELOBJ_TOGGLE )
				sel = !m_selection.isset( idx );
			else
				sel = !!mod;
			
			if( sel )
				m_selection.set( idx, NoValue() );
			else
				m_selection.unset( idx );
		}
	}
}

Vec3 EdWorld::FindCenterOfGroup( int32_t grp )
{
	Vec3 cp = V3(0);
	int count = 0;
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->group == grp )
		{
			cp += m_edobjs[ i ]->FindCenter();
			count++;
		}
	}
	if( count )
		cp /= count;
	return cp;
}

void EdWorld::FixTransformsOfGroup( int32_t grp )
{
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->group == grp )
			m_edobjs[ i ]->RegenerateMesh();
	}
}

void EdWorld::CopyObjectsToGroup( int32_t grpfrom, int32_t grpto )
{
	size_t oldsize = m_edobjs.size();
	for( size_t i = 0; i < oldsize; ++i )
	{
		if( m_edobjs[ i ]->group == grpfrom )
		{
			EdObject* obj = m_edobjs[ i ]->Clone();
			obj->group = grpto;
			obj->RegenerateMesh();
			m_edobjs.push_back( obj );
		}
	}
}

void EdWorld::TransferObjectsToGroup( int32_t grpfrom, int32_t grpto )
{
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->group == grpfrom )
		{
			m_edobjs[ i ]->group = grpto;
			m_edobjs[ i ]->RegenerateMesh();
		}
	}
}

void EdWorld::DeleteObjectsInGroup( int32_t grp )
{
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->group == grp )
		{
			m_edobjs.uerase( i-- );
		}
	}
}

void EdWorld::ExportGroupAsOBJ( int32_t grp, const StringView& name )
{
	OBJExporter objex;
	for( size_t i = 0; i < m_edobjs.size(); ++i )
	{
		if( m_edobjs[ i ]->group == grp )
		{
			m_edobjs[ i ]->Export( objex );
		}
	}
	objex.Save( name, "Exported from SGRX editor" );
}

LC_Light EdWorld::GetDirLightInfo()
{
	LC_Light L;
	L.type = LM_LIGHT_DIRECT;
	L.range = 1024;
	Vec2 dir = g_EdWorld->m_lighting.dirLightDir;
	L.dir = -V3( dir.x, dir.y, -1 ).Normalized();
	L.color = HSV( g_EdWorld->m_lighting.dirLightColor );
	L.light_radius = g_EdWorld->m_lighting.dirLightDvg / 180.0f;
	L.num_shadow_samples = g_EdWorld->m_lighting.dirLightNumSamples;
	return L;
}

void EdWorld::SetEntityID( EdEntity* e )
{
	int32_t id = m_nextID++;
	char bfr[ 32 ];
	sgrx_snprintf( bfr, 32, "ent%d", (int) id );
	e->SetID( bfr );
}



EdMultiObjectProps::EdMultiObjectProps() :
	m_selsurf( false )
{
}

void EdMultiObjectProps::Prepare( bool selsurf )
{
	m_selsurf = selsurf;
	String tex;
	for( size_t i = 0; i < g_EdWorld->m_selection.size(); ++i )
	{
		EdObjIdx idx = g_EdWorld->m_selection.item( i ).key;
		if( idx.type == ObjType_Block )
		{
			SGRX_CAST( EdBlock*, B, idx.edobj );
			for( size_t s = 0; s < B->surfaces.size(); ++s )
			{
				if( selsurf && B->IsSurfaceSelected( s ) == false )
					continue;
				StringView tt = B->surfaces[ s ]->texname;
				if( tt && tex != tt )
				{
					if( tex.size() )
					{
						m_mtl = "";
						return;
					}
					tex = tt;
				}
			}
		}
		else if( idx.type == ObjType_Patch )
		{
			SGRX_CAST( EdPatch*, P, idx.edobj );
			StringView tt = P->layers[0].texname;
			if( tt && tex != tt )
			{
				if( tex.size() )
				{
					m_mtl = "";
					return;
				}
				tex = tt;
			}
		}
		else if( idx.type == ObjType_MeshPath )
		{
			SGRX_CAST( EdMeshPath*, MP, idx.edobj );
			StringView tt = MP->m_parts[0].texname;
			if( tt && tex != tt )
			{
				if( tex.size() )
				{
					m_mtl = "";
					return;
				}
				tex = tt;
			}
		}
	}
	m_mtl = tex;
}

void EdMultiObjectProps::OnSetMtl( StringView name )
{
	for( size_t i = 0; i < g_EdWorld->m_selection.size(); ++i )
	{
		EdObjIdx idx = g_EdWorld->m_selection.item( i ).key;
		if( idx.type == ObjType_Block )
		{
			SGRX_CAST( EdBlock*, B, idx.edobj );
			for( size_t s = 0; s < B->surfaces.size(); ++s )
			{
				if( m_selsurf && B->IsSurfaceSelected( s ) == false )
					continue;
				B->surfaces[ s ]->texname = name;
			}
			B->RegenerateMesh();
		}
		else if( idx.type == ObjType_Patch )
		{
			SGRX_CAST( EdPatch*, P, idx.edobj );
			P->layers[0].texname = name;
			P->RegenerateMesh();
		}
		else if( idx.type == ObjType_MeshPath )
		{
			SGRX_CAST( EdMeshPath*, MP, idx.edobj );
			MP->m_parts[0].texname = name;
			MP->RegenerateMesh();
		}
	}
//	g_UIFrame->SetEditMode( g_UIFrame->m_editMode );
}

void EdMultiObjectProps::EditUI()
{
	IMGUI_GROUP( "Multiple objects", true,
	{
		if( g_NUISurfMtlPicker->Property( "Pick surface material", "Material", m_mtl ) )
			OnSetMtl( m_mtl );
	});
}


void MapEditorRenderView::DebugDraw()
{
	g_UIFrame->DebugDraw();
	
	if( g_DrawCovers )
	{
		g_EdMDCont->DebugDrawCovers();
	}
}


EdMainFrame::EdMainFrame() :
	m_editTF( NULL ),
	m_editMode( NULL )
{
	m_txMarker = GR_GetTexture( SGRXPATH_SRC_EDITOR "/marker.png" );
}

void EdMainFrame::EditUI()
{
	if( m_editMode )
		m_editMode->EditUI();
}

bool EdMainFrame::ViewUI()
{
	m_NUIRenderView.Process( ImGui::GetIO().DeltaTime, m_editTF == NULL );
	
	if( m_editTF )
	{
		if( m_editTF->ViewUI() )
			return false;
	}
	
	if( m_editMode )
		m_editMode->ViewUI();
	
	char bfr[ 1024 ];
	int x1 = g_UIFrame->m_NUIRenderView.m_vp.x1;
	int y1 = g_UIFrame->m_NUIRenderView.m_vp.y1;
	
	ImDrawList* idl = ImGui::GetWindowDrawList();
	idl->PushClipRectFullScreen();
	int vsz = g_EdLGCont->m_lmRenderer ? 32 : 16;
	idl->AddRectFilled( ImVec2( x1 - 200, y1 - vsz ), ImVec2( x1, y1 ), ImColor(0.f,0.f,0.f,0.5f), 8.0f, 0x1 );
	
	sgrx_snprintf( bfr, 1024, "%d outdated lightmaps", g_EdLGCont->GetInvalidItemCount() );
	ImGui::SetCursorScreenPos( ImVec2( x1 - 200 + 4, y1 - 16 - 1 ) );
	ImGui::Text( bfr );
	
	if( g_EdLGCont->m_lmRenderer )
	{
		sgrx_snprintf( bfr, 1024, "rendering lightmaps (%s: %d%%)",
			StackString<128>(g_EdLGCont->m_lmRenderer->stage).str,
			int(g_EdLGCont->m_lmRenderer->completion * 100) );
		ImGui::SetCursorScreenPos( ImVec2( x1 - 200 + 4, y1 - 32 - 1 ) );
		ImGui::Text( bfr );
	}
	
	idl->PopClipRect();
	
	return true;
}

void EdMainFrame::_DrawCursor( bool drawimg, float height )
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

void EdMainFrame::DrawCursor( bool drawimg )
{
	_DrawCursor( drawimg, 0 );
	if( m_NUIRenderView.crplaneheight )
		_DrawCursor( drawimg, m_NUIRenderView.crplaneheight );
}

void EdMainFrame::DebugDraw()
{
	if( m_editMode )
		m_editMode->Draw();
	if( m_editTF )
		m_editTF->Draw();
}

void EdMainFrame::OnDeleteObjects()
{
	if( m_editMode )
		m_editMode->OnEnter();
}


Vec3 EdMainFrame::GetCursorRayPos()
{
	return m_NUIRenderView.crpos;
}

Vec3 EdMainFrame::GetCursorRayDir()
{
	return m_NUIRenderView.crdir;
}

Vec3 EdMainFrame::GetCursorPos()
{
	return V3( Snapped( m_NUIRenderView.cursor_hpos ), m_NUIRenderView.crplaneheight );
}

Vec2 EdMainFrame::GetCursorPlanePos()
{
	return Snapped( m_NUIRenderView.cursor_hpos );
}

float EdMainFrame::GetCursorPlaneHeight()
{
	return m_NUIRenderView.crplaneheight;
}

void EdMainFrame::SetCursorPlaneHeight( float z )
{
	m_NUIRenderView.crplaneheight = z;
}

bool EdMainFrame::IsCursorAiming()
{
	return m_NUIRenderView.cursor_aim;
}

void EdMainFrame::Snap( Vec2& v )
{
	if( ImGui::GetIO().KeyAlt )
		return;
	m_snapProps.Snap( v );
}

void EdMainFrame::Snap( Vec3& v )
{
	if( ImGui::GetIO().KeyAlt )
		return;
	m_snapProps.Snap( v );
}

Vec2 EdMainFrame::Snapped( const Vec2& v )
{
	Vec2 o = v;
	Snap( o );
	return o;
}

Vec3 EdMainFrame::Snapped( const Vec3& v )
{
	Vec3 o = v;
	Snap( o );
	return o;
}


static StringView LevelPathToName( StringView path )
{
	if( path.starts_with( SGRXPATH_SRC_LEVELS "/" ) && path.ends_with( ".tle" ) )
		return path.part( STRLIT_LEN( SGRXPATH_SRC_LEVELS "/" ), path.size() - STRLIT_LEN( SGRXPATH_SRC_LEVELS "/.tle" ) );
	return path;
}


void EdMainFrame::Level_New()
{
	ReconfigureEntities( "" );
	g_EdWorld->Reset();
	g_EdLGCont->Reset();
	g_EdMDCont->Reset();
}

bool EdMainFrame::Level_Real_Open( const StringView& str )
{
	LOG_FUNCTION;
	
	LOG << "Trying to open level: " << str;
	
	String data;
	if( !FS_LoadTextFile( str, data ) )
	{
		LOG_ERROR << "FAILED TO LOAD LEVEL FILE: " << str;
		return false;
	}
	
	ReconfigureEntities( LevelPathToName( str ) );
	
	if( SV(data).part( 0, 5 ) == "WORLD" )
	{
		TextReader tr( &data );
		g_EdWorld->Serialize( tr );
		if( tr.error )
		{
			LOG_ERROR << "FAILED TO READ LEVEL FILE (at " << (int) tr.pos << "): " << str;
			return false;
		}
	}
	else if( SV(data).part( 0, 1 ) == "{" )
	{
		sgsVariable parsed = g_Level->GetScriptCtx().ParseSGSON( data );
		if( !parsed.not_null() )
		{
			LOG_ERROR << "FAILED TO READ LEVEL FILE: " << str;
			return false;
		}
		g_EdWorld->FLoad( parsed );
	}
	else
	{
		LOG_ERROR << "UNKNOWN LEVEL FILE FORMAT: " << str;
		return false;
	}
	
	g_EdLGCont->LoadLightmaps( LevelPathToName( str ) );
	g_EdMDCont->LoadCache( LevelPathToName( str ) );
	g_EdWorld->ReloadSkybox();
	g_EdWorld->ReloadCLUT();
	
	m_fileName = str;
	return true;
}

bool EdMainFrame::Level_Real_Save( const StringView& str )
{
	LOG_FUNCTION;
	
	LOG << "Trying to save level: " << str;
	String data;
	
#if 1
	data = g_Level->GetScriptCtx().ToSGSON( g_EdWorld->FSave() );
#else
	TextWriter arch( &data );
	
	arch << *g_EdWorld;
#endif
	
	if( !FS_SaveTextFile( str, data ) )
	{
		LOG_ERROR << "FAILED TO SAVE LEVEL FILE: " << str;
		return false;
	}
	
	g_EdLGCont->SaveLightmaps( LevelPathToName( str ) );
	g_EdMDCont->SaveCache( LevelPathToName( str ) );
	
	m_fileName = str;
	ReconfigureEntities( LevelPathToName( str ) );
	return true;
}

void EdMainFrame::Level_Real_Compile()
{
	if( g_EdWorld->m_info.prefabMode )
		Level_Real_Compile_Prefabs();
	else
		Level_Real_Compile_Default();
}

LevelCache* EdMainFrame::CreateCache()
{
	LevelCache* lcache = new LevelCache( &g_EdLGCont->m_lightEnv );
	lcache->m_skyTexture = g_EdWorld->m_lighting.skyboxTexture;
	lcache->m_clutTexture = g_EdWorld->m_lighting.clutTexture;
	
	g_EdLGCont->UpdateCache( *lcache );
	
	// gather system compilers
	Array< IEditorSystemCompiler* > ESCs;
	g_Level->GetEditorCompilers( ESCs );
	
	// compile entities
	for( size_t i = 0; i < g_EdWorld->m_edobjs.size(); ++i )
	{
		EdObject* O = g_EdWorld->m_edobjs[ i ];
		if( O->m_type != ObjType_Entity )
			continue;
		SGRX_CAST( EdEntity*, E, O );
		EditorEntity EE =
		{
			// sys. params
			g_EdWorld->m_systemsParams,
			// type
			StringView( E->m_entityType.c_str(), E->m_entityType.size() ),
			// props
			E->m_data,
			
		};
		
		// HARDCODED :(
		if( EE.type == "SolidBox" )
		{
			LC_SolidBox sb =
			{
				EE.props["position"].get<Vec3>(),
				Quat::CreateFromXYZ( EE.props["rotationXYZ"].get<Vec3>() ),
				EE.props["scale"].get<Vec3>(),
			};
			lcache->m_solidBoxes.push_back( sb );
		}
		if( EE.type == "MapLayer" )
		{
			LC_Map_Layer ml = { 0, 0, EE.props["position"].get<Vec3>().z };
			lcache->m_mapLayers.push_back( ml );
		}
		if( EE.type == "AIPathArea" )
		{
			Mat4 xf = Mat4::CreateSRT(
				EE.props["scale"].get<Vec3>(),
				EE.props["rotationXYZ"].get<Vec3>(),
				EE.props["position"].get<Vec3>() );
			
			LevelCache::PathArea PA;
			PA.push_back( xf.TransformPos( V3(-1,1,0) ).ToVec2() );
			PA.push_back( xf.TransformPos( V3(1,1,0) ).ToVec2() );
			PA.push_back( xf.TransformPos( V3(1,-1,0) ).ToVec2() );
			PA.push_back( xf.TransformPos( V3(-1,-1,0) ).ToVec2() );
			PA.z0 = xf.TransformPos( V3(0,0,-1) ).z;
			PA.z1 = xf.TransformPos( V3(0,0,1) ).z;
			
			lcache->m_pathAreas.push_back( PA );
		}
		
		for( size_t i = 0; i < ESCs.size(); ++i )
		{
			ESCs[ i ]->ProcessEntity( EE );
		}
	}
	
	// compile game objects
	g_Level->GetScriptCtx().GetGlobal( "ED_ILCSV" ).
		thiscall( g_Level->GetSGSC(), "_Restart" );
	lcache->m_gobj.gameObjects.resize( g_Level->m_gameObjects.size() );
	for( size_t i = 0; i < g_Level->m_gameObjects.size(); ++i )
	{
		GameObject* obj = g_Level->m_gameObjects[ i ];
		EDGO_LCSave( obj, &lcache->m_gobj.gameObjects[ i ] );
		if( obj->GetParent() )
		{
			LC_GOLink L =
			{
				obj->m_src_guid,
				obj->GetParent()->m_src_guid,
				"parent",
			};
			lcache->m_gobj.links.push_back( L );
		}
	}
	// links
	{
		ScriptVarIterator svi(
			g_Level->GetScriptCtx().GetGlobal( "ED_ILCSV"
				).getprop( "links" ) );
		while( svi.Advance() )
		{
			sgsVariable lv = svi.GetValue();
			LC_GOLink L =
			{
				SGRX_GUID::ParseString( lv.getprop( "src" ).get_string().c_str() ),
				SGRX_GUID::ParseString( lv.getprop( "dst" ).get_string().c_str() ),
				lv.getprop( "prop" ).get_string().c_str()
			};
			lcache->m_gobj.links.push_back( L );
		}
	}
	
	// generate system chunks
	for( size_t i = 0; i < ESCs.size(); ++i )
	{
		ByteArray chunk;
		if( ESCs[ i ]->GenerateChunk( chunk, g_EdWorld->m_systemsParams ) &&
			chunk.size() )
			lcache->m_chunkData.append( chunk );
		
		SAFE_DELETE( ESCs[ i ] );
	}
	
	return lcache;
}

void EdMainFrame::Level_Real_Compile_Default()
{
	LOG_FUNCTION;
	
	LOG << "Compiling level";
	LevelCache* lcache = CreateCache();
	
	char bfr[ 256 ];
	StringView lname = LevelPathToName( m_fileName );
	sgrx_snprintf( bfr, sizeof(bfr), SGRXPATH_CACHE_LEVELS "/%.*s" SGRX_LEVEL_DIR_SFX, TMIN( (int) lname.size(), 200 ), lname.data() );
	
	// extra chunks
	ByteArray ba_mapl, ba_cov2;
	LC_Chunk chunk;
	Array< LC_Chunk > xchunks;
	// - map
	if( g_EdMDCont->m_mapLines.size() )
	{
		LC_Chunk_Mapl ch_mapl = { &g_EdMDCont->m_mapLines, &g_EdMDCont->m_mapLayers };
		ByteWriter( &ba_mapl ) << ch_mapl;
		memcpy( chunk.sys_id, LC_FILE_MAPL_NAME, sizeof(chunk.sys_id) );
		chunk.ptr = ba_mapl.data();
		chunk.size = ba_mapl.size();
		xchunks.push_back( chunk );
	}
	// - navigation data
	if( g_EdMDCont->m_navMeshData.size() )
	{
		memcpy( chunk.sys_id, LC_FILE_PFND_NAME, sizeof(chunk.sys_id) );
		chunk.ptr = g_EdMDCont->m_navMeshData.data();
		chunk.size = g_EdMDCont->m_navMeshData.size();
		xchunks.push_back( chunk );
	}
	// - cover data
	if( g_EdMDCont->m_coverData.size() )
	{
		LC_Chunk_COV2 ch_cov2 = { &g_EdMDCont->m_coverData };
		ByteWriter( &ba_cov2 ) << ch_cov2;
		memcpy( chunk.sys_id, LC_FILE_COV2_NAME, sizeof(chunk.sys_id) );
		chunk.ptr = ba_cov2.data();
		chunk.size = ba_cov2.size();
		xchunks.push_back( chunk );
	}
	
	if( !lcache->SaveCache( g_NUISurfMtlPicker->m_materials, bfr, xchunks ) )
		LOG_ERROR << "FAILED TO SAVE CACHE";
	else
		LOG << "Level is compiled";
	
	delete lcache;
}

static void AppendNameSafeGUID( String& out, const SGRX_GUID& guid )
{
	char bfr[ 33 ];
	guid.ToCharArray( bfr, true, true, 0 );
	out.append( bfr );
}

void EdMainFrame::Level_Real_Compile_Prefabs()
{
	LOG_FUNCTION;
	
	String data;
	ScriptContext& sctx = g_Level->GetScriptCtx();
	
	data.append( "\nglobal PREFABS;\n" );
	data.append( "if( !@PREFABS ) PREFABS = {};\n\n" );
	
	for( size_t i = 0; i < g_Level->m_gameObjects.size(); ++i )
	{
		sctx.GetGlobal( "ED_ILCSV" ).thiscall( sctx.C, "_Restart" );
		
		GameObject* obj = g_Level->m_gameObjects[ i ];
		if( obj->GetParent() )
			continue; // has parent, so part of another object
		if( obj->GetName() == "" )
			continue; // no name, cannot create a function
		
		data.append( "function PREFABS." );
		data.append( obj->GetName() );
		data.append( "( level )\n{\n" );
		
		data.append( "\titem_" );
		AppendNameSafeGUID( data, obj->m_src_guid );
		data.append( " = obj = level.CreateGameObject().\n" );
		data.append( "\t{\n" );
		data.append( "\t\tname = " );
		data.append( sctx.ToSGSON( obj->m_name ) );
		data.append( ",\n\t\tid = " );
		data.append( sctx.ToSGSON( obj->m_id ) );
		data.append( ",\n\t\tlocalPosition = vec3(0,0,0)" );
		{
			char bfr[ 1024 ];
			Vec3 tgt = obj->GetInfoTarget();
			sgrx_snprintf( bfr, 1024, ",\n\t\tinfoMask = %u,\n\t\tlocalInfoTarget = vec3(%g,%g,%g)",
				(unsigned) obj->GetInfoMask(), tgt.x, tgt.y, tgt.z );
			data.append( bfr );
		}
		data.append( ",\n\t};\n" );
		
		for( size_t i = 0; i < obj->m_resources.size(); ++i )
		{
			GOResource* rsrc = obj->m_resources[ i ];
			
			data.append( "\titem_" );
			AppendNameSafeGUID( data, rsrc->m_src_guid );
			char bfr[ 48 ];
			sgrx_snprintf( bfr, 48, " = obj.RequireResource( %u, true ).\n\t{\n",
				(unsigned) rsrc->m_rsrcType );
			data.append( bfr );
			// resource properties
			sgsVariable rsrcdata = EDGO_RSRC_LCSave( rsrc );
			ScriptVarIterator it( rsrcdata );
			while( it.Advance() )
			{
				data.append( "\t\t" );
				data.append( sctx.ToSGSON( it.GetKey(), NULL ) );
				data.append( " = " );
				data.append( sctx.ToSGSON( it.GetValue(), NULL ) );
				data.append( ",\n" );
			}
			// ---
			data.append( "\t};\n" );
		}
		
		for( size_t i = 0; i < obj->m_behaviors.size(); ++i )
		{
			GOBehavior* bhvr = obj->m_behaviors[ i ];
			
			data.append( "\titem_" );
			AppendNameSafeGUID( data, bhvr->m_src_guid );
			data.append( " = obj.RequireBehavior( " ); // TODO FIX MULTIPLE BEHAVIORS
			data.append( sctx.ToSGSON( bhvr->m_type ) );
			data.append( ", true ).\n\t{\n" );
			// behavior properties
			sgsVariable bhvrdata = EDGO_BHVR_LCSave( bhvr );
			ScriptVarIterator it( bhvrdata );
			while( it.Advance() )
			{
				data.append( "\t\t" );
				data.append( sctx.ToSGSON( it.GetKey(), NULL ) );
				data.append( " = " );
				data.append( sctx.ToSGSON( it.GetValue(), NULL ) );
				data.append( ",\n" );
			}
			// ---
			data.append( "\t};\n" );
		}
		
		// handle links
		ScriptVarIterator it( sctx.GetGlobal( "ED_ILCSV" ).getprop( "links" ) );
		while( it.Advance() )
		{
			sgsVariable link = it.GetValue();
			data.append( "\titem_" );
			AppendNameSafeGUID( data, SGRX_GUID::ParseString(
				link.getprop( "src" ).getdef(SV()) ) );
			data.append( "." );
			data.append( link.getprop( "prop" ).getdef(SV()) );
			data.append( " = item_" );
			AppendNameSafeGUID( data, SGRX_GUID::ParseString(
				link.getprop( "dst" ).getdef(SV()) ) );
			data.append( ";\n" );
		}
		
		data.append( "\treturn obj;\n" );
		
		data.append( "}\n\n" );
	}
	
	sctx.GetGlobal( "ED_ILCSV" ).thiscall( sctx.C, "_Restart" );
	
	// save the file
	char bfr[ 256 ];
	StringView lname = LevelPathToName( m_fileName );
	sgrx_snprintf( bfr, sizeof(bfr), SGRXPATH_SRC_LEVELS "/%.*s.pfb.sgs", TMIN( (int) lname.size(), 200 ), lname.data() );
	if( !FS_SaveTextFile( bfr, data ) )
		LOG_ERROR << "FAILED TO SAVE PREFAB";
	else
		LOG << "Prefab script is compiled";
}

void EdMainFrame::SetEditMode( EdEditMode* em )
{
	if( em == m_editMode )
		return;
	if( m_editMode )
		m_editMode->OnExit();
	m_editMode = em;
	if( em )
		em->OnEnter();
}

void EdMainFrame::SetEditTransform( EdEditTransform* et )
{
	if( m_editTF )
	{
		m_editTF->OnExit();
		if( m_editMode )
			m_editMode->OnTransformEnd();
	}
	m_editTF = et && et->OnEnter() ? et : NULL;
}


static GameObject* hoveredObj = NULL;
static void AfterObjChanges()
{
	if( g_mode == EditObjects )
		g_UIFrame->m_emEditObjs.RecheckSelectionUI();
}
static void DrawHierarchyItem( GameObject* obj )
{
	bool open = false;
	ImGui::PushID( obj );
	if( obj->GetChildCount() )
	{
		ImGui::SetNextTreeNodeOpened( true, ImGuiSetCond_FirstUseEver );
		open = ImGui::TreeNode( "" );
		ImGui::SameLine();
	}
	if( ImGui::Selectable(
		obj->m_name.size() ? obj->m_name.c_str() : "<unnamed>",
		g_EdWorld->IsObjectSelected( obj ) ) )
	{
		g_EdWorld->SelectObject( obj,
			ImGui::IsKeyDown( SDL_SCANCODE_LCTRL ) ? SELOBJ_TOGGLE : SELOBJ_ONLY );
		AfterObjChanges();
	}
	if( ImGui::IsMouseDragging() )
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		if( obj == hoveredObj )
		{
			dl->AddRectFilled( ImGui::GetItemRectMin(),
				ImGui::GetItemRectMax(),
				ImColor(200,100,100,100), 4 );
		}
		else if( ImGui::IsItemHoveredRect() )
		{
			ImVec2 mpos = ImGui::GetMousePos(),
				rmin = ImGui::GetItemRectMin(),
				rmax = ImGui::GetItemRectMax();
			if( mpos.x < ( rmin.x + rmax.x ) * 0.5f )
			{
				dl->AddRectFilled( rmin,
					ImVec2( ( rmin.x + rmax.x ) * 0.5f, rmax.y ),
					ImColor(200,150,100,100), 4 );
			}
			else
			{
				dl->AddRectFilled(
					ImVec2( ( rmin.x + rmax.x ) * 0.5f, rmin.y ),
					rmax, ImColor(200,150,100,100), 4 );
			}
		}
	}
	if( ImGui::IsItemHovered() && !ImGui::IsMouseDragging() && !ImGui::IsMouseReleased(0) )
	{
		hoveredObj = obj;
	}
	if( ImGui::IsItemHovered() && ImGui::IsMouseClicked(1) )
	{
	//	menuObj = obj;
		ImGui::OpenPopup( "object_h_menu" );
	}
	if( ImGui::BeginPopup( "object_h_menu" ) )
	{
		ImGui::Text( "%s", obj->m_name.size() ? obj->m_name.c_str() : "<unnamed>" );
		if( obj->GetParent() )
		{
			ImGui::Separator();
			if( ImGui::Selectable( "Move up (keep world xform)" ) )
			{
				obj->SetParent( obj->GetParent()->GetParent(), true );
				AfterObjChanges();
			}
			if( ImGui::Selectable( "Move up (keep local xform)" ) )
			{
				obj->SetParent( obj->GetParent()->GetParent(), false );
				AfterObjChanges();
			}
			if( ImGui::Selectable( "Move to top (keep world xform)" ) )
			{
				obj->SetParent( NULL, true );
				AfterObjChanges();
			}
			if( ImGui::Selectable( "Move to top (keep local xform)" ) )
			{
				obj->SetParent( NULL, false );
				AfterObjChanges();
			}
		}
		ImGui::Separator();
		if( ImGui::Selectable( "Delete" ) )
		{
			g_Level->DestroyGameObject( obj, false );
			AfterObjChanges();
		}
		if( ImGui::Selectable( "Delete (with children)" ) )
		{
			g_Level->DestroyGameObject( obj, true );
			AfterObjChanges();
		}
		if( ImGui::Selectable( "Duplicate" ) )
		{
			EDGO_Clone( obj );
			AfterObjChanges();
		}
		ImGui::EndPopup();
	}
	if( ImGui::IsItemHoveredRect() && ImGui::IsMouseReleased(0) && hoveredObj )
	{
		if( obj != hoveredObj )
		{
			ImVec2 mpos = ImGui::GetMousePos(),
				rmin = ImGui::GetItemRectMin(),
				rmax = ImGui::GetItemRectMax();
			if( mpos.x < ( rmin.x + rmax.x ) * 0.5f )
			{
				hoveredObj->SetParent( obj->GetParent(), ImGui::GetIO().KeyAlt == false );
				AfterObjChanges();
			}
			else
			{
				hoveredObj->SetParent( obj, ImGui::GetIO().KeyAlt == false );
				AfterObjChanges();
			}
		}
		hoveredObj = NULL;
	}
	if( open )
	{
		for( size_t i = 0; i < obj->GetChildCount(); ++i )
		{
			DrawHierarchyItem( obj->GetChild( i ) );
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}
static void DrawObjectHierarchy()
{
	for( size_t i = 0; i < g_Level->m_gameObjects.size(); ++i )
	{
		if( g_Level->m_gameObjects[ i ]->GetParent() == NULL )
			DrawHierarchyItem( g_Level->m_gameObjects[ i ] );
	}
	if( ImGui::IsMouseReleased(0) )
	{
		hoveredObj = NULL;
	}
}


//
// EDITOR ENTRY POINT
//

bool MapEditor::OnInitialize()
{
	LOG_FUNCTION_ARG( "MapEditor" );
	
	Window_EnableResizing( true );
	GR2D_LoadFont( "core", "fonts/lato-regular.ttf" );
	GR2D_SetFont( "core", 12 );
	
	SGRX_IMGUI_Init();
	
	g_NUILevelPicker = new IMGUIFilePicker( SGRXPATH_SRC_LEVELS, ".tle" );
	g_NUIMeshPicker = new IMGUIMeshPicker();
	g_NUITexturePicker = new IMGUITexturePicker();
	g_NUICharPicker = new IMGUICharPicker();
	g_NUIPartSysPicker = new IMGUIFilePicker( SGRXPATH__PARTSYS, ".psy", false );
	g_NUISurfMtlPicker = new IMGUISurfMtlPicker();
	g_NUISoundPicker = new IMGUISoundPicker();
	g_NUIShaderPicker = new IMGUIShaderPicker();
	
	g_BaseGame->ParseConfigFile( true );
	g_Level = g_BaseGame->CreateLevel();
	g_Level->m_editorMode = true;
	
	g_Level->GetScriptCtx().ExecFile( SGRXPATH_SRC_EDITOR "/ext.sgs" );
	
	sgs_RegIntConsts( g_Level->GetSGSC(), g_ent_scripted_ric, -1 );
	sgs_RegFuncConsts( g_Level->GetSGSC(), g_ent_scripted_rfc, -1 );
	sgs_StoreFuncConsts( g_Level->GetSGSC(),
		g_Level->GetScriptCtx().GetGlobal( "ED_IMGUI" ).var, g_imgui_rfc, -1, "ED_IMGUI." );
	
	g_Level->GetScriptCtx().ExecFile( SGRXPATH_SRC_LEVELS "/upgrade1.sgs" );
	
	g_NUISoundPicker->sys = g_Level->GetSoundSys();
	g_NUISoundPicker->Reload();
	
	// core layout
	g_EdLGCont = new EdLevelGraphicsCont();
	g_EdMDCont = new EdMetaDataCont();
	g_EdScene = g_Level->GetScene();
	g_EdScene->camera.position = Vec3::Create(3,3,3);
	g_EdScene->camera.UpdateMatrices();
	g_EdWorld = new EdWorld();
	g_EdWorld->RegenerateMeshes();
	g_UIFrame = new EdMainFrame();
	g_UIFrame->SetEditMode( &g_UIFrame->m_emMeasure );
	
	return true;
}

void MapEditor::OnDestroy()
{
	LOG_FUNCTION_ARG( "MapEditor" );
	
	delete g_UIFrame;
	g_UIFrame = NULL;
	delete g_EdWorld;
	g_EdWorld = NULL;
	g_EdScene = NULL;
	delete g_EdMDCont;
	g_EdMDCont = NULL;
	delete g_EdLGCont;
	g_EdLGCont = NULL;
	delete g_Level;
	
	delete g_BaseGame;
	
	delete g_NUIShaderPicker;
	delete g_NUISoundPicker;
	delete g_NUISurfMtlPicker;
	delete g_NUIPartSysPicker;
	delete g_NUICharPicker;
	delete g_NUITexturePicker;
	delete g_NUIMeshPicker;
	delete g_NUILevelPicker;
	
	SGRX_IMGUI_Free();
}

void MapEditor::OnEvent( const Event& e )
{
	LOG_FUNCTION_ARG( "MapEditor" );
	SGRX_IMGUI_Event( e );
}

static bool ModeRB( const char* label, int mode, int key )
{
	if( ImGui::RadioButton( label, &g_mode, mode ) )
		return true;
	if( ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed( key, false ) )
	{
		g_mode = mode;
		return true;
	}
	return false;
}

void MapEditor::OnTick( float dt, uint32_t gametime )
{
	g_EdWorld->m_groupMgr.ProcessDestroyQueue();
	GR2D_SetViewMatrix( Mat4::CreateUI( 0, 0, GR_GetWidth(), GR_GetHeight() ) );
	g_EdLGCont->ApplyInvalidation();
	g_EdLGCont->ILMCheck();
	
	SGRX_IMGUI_NewFrame( dt );
	
	IMGUI_MAIN_WINDOW_BEGIN
	{
		bool needOpen = false;
		bool needSave = false;
		bool needSaveAs = false;
		
		if( ImGui::BeginMenuBar() )
		{
			if( ImGui::BeginMenu( "File" ) )
			{
				if( ImGui::MenuItem( "New" ) ) g_UIFrame->Level_New();
				if( ImGui::MenuItem( "Open" ) ) needOpen = true;
				if( ImGui::MenuItem( "Save" ) ) needSave = true;
				if( ImGui::MenuItem( "Save As" ) ) needSaveAs = true;
				ImGui::Separator();
				if( ImGui::MenuItem( "Exit" ) ){ Game_End(); }
				ImGui::EndMenu();
			}
			
			ImGui::SameLine();
			if( g_UIFrame->m_fileName.size() )
			{
				if( ImGui::Button( "Compile" ) )
					g_UIFrame->Level_Real_Compile();
			}
			else ImGui::Button( "Save before compiling" );
			
			ImGui::SameLine();
			if( ImGui::BeginMenu( "Prefabs" ) )
			{
				sgsVariable pfb = g_Level->GetScriptCtx().GetGlobal( "PREFABS" );
				if( pfb.not_null() )
				{
					ScriptVarIterator it( pfb );
					while( it.Advance() )
					{
						if( ImGui::MenuItem( it.GetKey().get_string().c_str() ) )
						{
							g_Level->GetScriptedObject().push( g_Level->GetSGSC() );
							it.GetValue().call( g_Level->GetSGSC(), 1, 1 );
							sgsVariable obj( g_Level->GetSGSC(), sgsVariable::PickAndPop );
							SGRX_Camera& cam = g_EdScene->camera;
							Vec3 pos = cam.position + cam.direction.Normalized();
							obj.setprop( "position", g_Level->GetScriptCtx().CreateVec3( pos ) );
						}
					}
				}
				else
				{
					ImGui::Text( "No prefabs loaded" );
				}
				ImGui::EndMenu();
			}
			
			if( ImGui::IsKeyPressed( SDL_SCANCODE_F2, false ) )
			{
				g_Level->GetScene()->director->SetMode( 0 );
			}
			if( ImGui::IsKeyPressed( SDL_SCANCODE_F3, false ) )
			{
				g_Level->GetScene()->director->SetMode( 1 );
			}
			if( ImGui::IsKeyPressed( SDL_SCANCODE_F4, false ) )
			{
				g_Level->GetScene()->director->SetMode( 2 );
			}
			
			ImGui::SameLine();
			if( ImGui::Button( "Rebuild..." ) ||
				ImGui::IsKeyPressed( SDL_SCANCODE_F5, false ) )
				ImGui::OpenPopup( "Cache rebuild/info options" );
			
			if( ImGui::BeginPopupModal( "Cache rebuild/info options" ) )
			{
				if( ImGui::IsKeyPressed( SDLK_ESCAPE, false ) )
					ImGui::CloseCurrentPopup();
				IMGUI_GROUP( "Lightmap", true,
				{
					if( ImGui::Button( "Rebuild all lightmaps [1]" ) || ImGui::IsKeyPressed( SDLK_1, false ) )
					{
						if( g_EdLGCont->m_lmRenderer == NULL )
						{
							g_EdLGCont->InvalidateAll();
							g_EdLGCont->ILMBeginRender();
						}
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if( ImGui::Button( "Rebuild outdated lightmaps [2]" ) || ImGui::IsKeyPressed( SDLK_2, false ) )
					{
						g_EdLGCont->ILMBeginRender();
						ImGui::CloseCurrentPopup();
					}
					ImGui::SameLine();
					if( ImGui::Button( "Stop [3]" ) || ImGui::IsKeyPressed( SDLK_3, false ) )
					{
						g_EdLGCont->ILMAbort();
						ImGui::CloseCurrentPopup();
					}
					if( ImGui::Button( "Regenerate samples [4]" ) || ImGui::IsKeyPressed( SDLK_4, false ) )
					{
						g_EdLGCont->STRegenerate();
						ImGui::CloseCurrentPopup();
					}
					if( ImGui::Button( "Dump lightmap info" ) )
					{
						g_EdLGCont->DumpLightmapInfo();
						ImGui::CloseCurrentPopup();
					}
				});
				IMGUI_GROUP( "Metadata", true,
				{
					if( ImGui::Button( "Rebuild navigation data [5]" ) || ImGui::IsKeyPressed( SDLK_5, false ) )
					{
						g_EdMDCont->RebuildNavMesh();
						ImGui::CloseCurrentPopup();
					}
					if( ImGui::Button( "Rebuild cover data [6]" ) || ImGui::IsKeyPressed( SDLK_6, false ) )
					{
						g_EdMDCont->RebuildCovers();
						ImGui::CloseCurrentPopup();
					}
					if( ImGui::Button( "Rebuild map data [7]" ) || ImGui::IsKeyPressed( SDLK_7, false ) )
					{
						g_EdMDCont->RebuildMap();
						ImGui::CloseCurrentPopup();
					}
				});
				ImGui::EndPopup();
			}
			
			ImGui::SameLine( 0, 50 );
			ImGui::Text( "Edit mode:" );
			ImGui::SameLine();
			if( ModeRB( "Create objects", CreateObjs, SDLK_1 ) )
				g_UIFrame->SetEditMode( &g_UIFrame->m_emDrawBlock );
			ImGui::SameLine();
			if( ModeRB( "Edit objects", EditObjects, SDLK_2 ) )
				g_UIFrame->SetEditMode( &g_UIFrame->m_emEditObjs );
			ImGui::SameLine();
			if( ModeRB( "Paint surfaces", PaintSurfs, SDLK_3 ) )
				g_UIFrame->SetEditMode( &g_UIFrame->m_emPaintSurfs );
			ImGui::SameLine();
			if( ModeRB( "Edit groups", EditGroups, SDLK_4 ) )
				g_UIFrame->SetEditMode( &g_UIFrame->m_emEditGroup );
			ImGui::SameLine();
			if( ModeRB( "Level info", LevelInfo, SDLK_5 ) )
				g_UIFrame->SetEditMode( &g_UIFrame->m_emMeasure );
			ImGui::SameLine();
			if( ModeRB( "Misc. settings", MiscProps, SDLK_6 ) )
				g_UIFrame->SetEditMode( NULL );
			
			ImGui::SameLine( 0, 50 );
			StringView lfn = g_UIFrame->m_fileName;
			ImGui::Text( "Level file: %s", lfn.size() ? StackPath(lfn).str : "<none>" );
			
			ImGui::EndMenuBar();
		}
		
		IMGUI_HSPLIT3( 0.2f, 0.7f,
		{
			ImGui::Text( "Objects" );
			ImGui::Separator();
			DrawObjectHierarchy();
		},
		{
			g_UIFrame->ViewUI();
		},
		{
			g_UIFrame->EditUI();
			
			if( g_mode == LevelInfo )
			{
				ImGui::Text( "Level info" );
				ImGui::Separator();
				g_EdWorld->EditUI();
			}
			else if( g_mode == MiscProps )
			{
				ImGui::Text( "Misc. settings" );
				ImGui::Separator();
				IMGUI_GROUP( "Rendering mode", true,
				{
					int mode = g_Level->GetScene()->director->GetMode();
					if( ImGui::RadioButton( "Normal", mode == 0 ) )
						g_Level->GetScene()->director->SetMode( 0 );
					if( ImGui::RadioButton( "Unlit", mode == 1 ) )
						g_Level->GetScene()->director->SetMode( 1 );
					if( ImGui::RadioButton( "Lighting (no diffuse color)", mode == 2 ) )
						g_Level->GetScene()->director->SetMode( 2 );
				});
				IMGUI_GROUP( "Display metadata", true,
				{
					IMGUIEditBool( "Covers", g_DrawCovers );
				});
				g_UIFrame->m_NUIRenderView.EditCameraParams();
			}
		});
		
		//
		// OPEN
		//
		String fn;
#define OPEN_CAPTION "Open level (.tle) file"
		if( needOpen )
			g_NUILevelPicker->OpenPopup( OPEN_CAPTION );
		if( g_NUILevelPicker->Popup( OPEN_CAPTION, fn, false ) )
		{
			if( !g_UIFrame->Level_Real_Open( fn ) )
			{
				IMGUIError( "Cannot open file: %s", StackPath(fn).str );
			}
		}
		
		//
		// SAVE
		//
		fn = g_UIFrame->m_fileName;
#define SAVE_CAPTION "Save level (.tle) file"
		if( needSaveAs || ( needSave && g_UIFrame->m_fileName.size() == 0 ) )
			g_NUILevelPicker->OpenPopup( SAVE_CAPTION );
		
		bool canSave = needSave && g_UIFrame->m_fileName.size();
		if( g_NUILevelPicker->Popup( SAVE_CAPTION, fn, true ) )
			canSave = fn.size();
		if( canSave )
		{
			if( !g_UIFrame->Level_Real_Save( fn ) )
			{
				IMGUIError( "Cannot save file: %s", StackPath(fn).str );
			}
		}
	}
	IMGUI_MAIN_WINDOW_END;
	
	SGRX_IMGUI_Render();
	SGRX_IMGUI_ClearEvents();
}

void MapEditor::SetBaseGame( BaseGame* game )
{
	g_BaseGame = game;
}


MapEditor* g_Game;

extern "C" EXPORT IGame* CreateGame()
{
	g_Game = new MapEditor;
	return g_Game;
}

extern "C" EXPORT void SetBaseGame( BaseGame* game )
{
	g_Game->SetBaseGame( game );
}

