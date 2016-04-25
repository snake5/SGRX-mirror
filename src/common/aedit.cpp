

#include "aedit.hpp"
#include <imgui.hpp>

#define MPD_IMPL
#include "aedit.mpd.hpp"


#define ASSET_SCRIPT_NAME "assets.txt"
#define ASSET_INFO_NAME "asset_revs.info"
#define OUTPUT_INFO_NAME "output_revs.info"


struct EDGUIMainFrame* g_UIFrame;
SceneHandle g_EdScene;
SGRX_AssetScript* g_EdAS;
EDGUIPickers* g_UIPickers;

EDGUIRsrcPicker TMPRSRC;


struct AssetRenderView* g_NUIRenderView;
struct IMGUIFilePicker* g_NUIAssetPicker;


#define SGRX_GET_FLAG( v, fl ) (((v) & (fl)) != 0)
#define SGRX_SET_FLAG( v, fl, b ) v = ((v) & ~(fl)) | ((b)?(fl):0)


void FC_EditMesh( size_t id ){ g_UIFrame->EditMesh( id ); }
void FC_EditMeshList(){ g_UIFrame->EditMeshList(); }
void FC_EditAnimBundle( size_t id ){ g_UIFrame->EditAnimBundle( id ); }
void FC_EditAnimBundleList(){ g_UIFrame->EditAnimBundleList(); }


void EDGUISmallEnumPicker::_OnChangeZoom()
{
	EDGUIRsrcPicker::_OnChangeZoom();
	m_itemHeight /= 4;
}

void EDGUILongEnumPicker::_OnChangeZoom()
{
	EDGUIRsrcPicker::_OnChangeZoom();
	m_itemWidth = 300;
	m_itemHeight = 16;
}

EDGUIMtlBlendMode::EDGUIMtlBlendMode()
{
	caption = "Pick the blending mode";
	m_options.push_back( "None" );
	m_options.push_back( "Basic" );
	m_options.push_back( "Additive" );
	m_options.push_back( "Multiply" );
	_Search( m_searchString );
}

EDGUICategoryPicker::EDGUICategoryPicker()
{
	m_looseSearch = true;
	Reload();
}
void EDGUICategoryPicker::Reload()
{
	m_options.clear();
	if( g_EdAS )
	{
		for( size_t i = 0; i < g_EdAS->categories.size(); ++i )
		{
			m_options.push_back( g_EdAS->categories.item( i ).key );
		}
	}
	_Search( m_searchString );
}

EDGUITextureAssetPicker::EDGUITextureAssetPicker()
{
	m_looseSearch = true;
	Reload();
}
void EDGUITextureAssetPicker::Reload()
{
	m_options.clear();
	if( g_EdAS )
	{
		for( size_t i = 0; i < g_EdAS->textureAssets.size(); ++i )
		{
			const SGRX_TextureAsset& TA = g_EdAS->textureAssets[ i ];
			String opt = TA.outputCategory;
			opt.append( "/" );
			opt.append( TA.outputName );
			m_options.push_back( opt );
		}
	}
	_Search( m_searchString );
}

EDGUIMeshAssetPicker::EDGUIMeshAssetPicker()
{
	m_looseSearch = true;
	Reload();
}
void EDGUIMeshAssetPicker::Reload()
{
	m_options.clear();
	if( g_EdAS )
	{
		for( size_t i = 0; i < g_EdAS->meshAssets.size(); ++i )
		{
			const SGRX_MeshAsset& TA = g_EdAS->meshAssets[ i ];
			String opt = TA.outputCategory;
			opt.append( "/" );
			opt.append( TA.outputName );
			m_options.push_back( opt );
		}
	}
	_Search( m_searchString );
}

EDGUIAssetPathPicker::EDGUIAssetPathPicker() : m_depth(0)
{
	m_looseSearch = true;
	Reload();
}
void EDGUIAssetPathPicker::AddOptionsFromDir( const StringView& path )
{
	if( m_depth > 32 )
		return;
	m_depth++;
	FS_IterateDirectory( path, this );
	m_depth--;
}
bool EDGUIAssetPathPicker::HandleDirEntry( const StringView& loc, const StringView& name, bool isdir )
{
	if( name == "." || name == ".." )
		return true;
	String path = loc;
	path.append( "/" );
	path.append( name );
	if( isdir )
	{
		AddOptionsFromDir( path );
	}
	else
	{
		m_options.push_back( path );
	}
	return true;
}
void EDGUIAssetPathPicker::Reload()
{
	m_options.clear();
	m_depth = 0;
	AddOptionsFromDir( "assets" );
	_Search( m_searchString );
}

EDGUIASMeshNamePicker::EDGUIASMeshNamePicker()
{
	m_looseSearch = true;
	Reload();
}
void EDGUIASMeshNamePicker::Reload()
{
	m_options.clear();
	if( m_scene )
	{
		m_scene->GetMeshList( m_options );
	}
	_Search( m_searchString );
}

EDGUIASAnimNamePicker::EDGUIASAnimNamePicker()
{
	m_looseSearch = true;
	Reload();
}
void EDGUIASAnimNamePicker::Reload()
{
	m_options.clear();
	for( size_t i = 0; i < m_scenes.size(); ++i )
	{
		if( !m_scenes[ i ] )
			continue;
		m_scenes[ i ]->GetAnimList( m_options );
	}
	_Search( m_searchString );
}


EDGUIAssetMesh::EDGUIAssetMesh() :
	m_group( true, "Mesh" ),
	m_sourceFile( &g_UIPickers->assetPath ),
	m_outputCategory( &g_UIPickers->category ),
	m_MPgroup( true, "Selected part" ),
	m_MPmeshName( &g_UIPickers->meshName ),
	m_MPshader( &g_UIPickers->shader ),
	m_MPtexture0( &g_UIPickers->textureAsset ),
	m_MPtexture1( &g_UIPickers->textureAsset ),
	m_MPtexture2( &g_UIPickers->textureAsset ),
	m_MPtexture3( &g_UIPickers->textureAsset ),
	m_MPtexture4( &g_UIPickers->textureAsset ),
	m_MPtexture5( &g_UIPickers->textureAsset ),
	m_MPtexture6( &g_UIPickers->textureAsset ),
	m_MPtexture7( &g_UIPickers->textureAsset ),
	m_MPmtlUnlit( false ),
	m_MPmtlNocull( false ),
	m_MPmtlDecal( false ),
	m_MPmtlBlendMode( &g_UIPickers->blendMode ),
	m_MPLgroup( true, "Parts" ),
	m_mid( NOT_FOUND ),
	m_pid( NOT_FOUND )
{
	m_outputCategory.m_requestReload = true;
	
	m_btnDuplicate.caption = "Duplicate";
	m_btnDelete.caption = "Delete";
	
	m_topCol.Add( &m_btnDuplicate );
	m_topCol.Add( &m_btnDelete );
	
	m_sourceFile.caption = "Source file";
	m_outputCategory.caption = "Output category";
	m_outputName.caption = "Output name";
	m_rotateY2Z.caption = "Rotate Y->Z";
	m_flipUVY.caption = "Flip UV/Y";
	m_transform.caption = "Transform";
	
	m_group.Add( &m_sourceFile );
	m_group.Add( &m_outputCategory );
	m_group.Add( &m_outputName );
	m_group.Add( &m_rotateY2Z );
	m_group.Add( &m_flipUVY );
	m_group.Add( &m_transform );
	
	m_MPLBtnAdd.caption = "Add part";
	m_MPLButtons.m_model = &m_meshPartModel;
	m_MPLButtons.Add( &m_MPLEditButton );
	m_MPLgroup.Add( &m_MPLBtnAdd );
	m_MPLgroup.Add( &m_MPLButtons );
	
	m_MPmeshName.caption = "Mesh name";
	m_MPshader.caption = "Shader";
	m_MPtexture0.caption = "Texture 0";
	m_MPtexture1.caption = "Texture 1";
	m_MPtexture2.caption = "Texture 2";
	m_MPtexture3.caption = "Texture 3";
	m_MPtexture4.caption = "Texture 4";
	m_MPtexture5.caption = "Texture 5";
	m_MPtexture6.caption = "Texture 6";
	m_MPtexture7.caption = "Texture 7";
	m_MPmtlUnlit.caption = "Unlit?";
	m_MPmtlNocull.caption = "Disable culling?";
	m_MPmtlDecal.caption = "Decal?";
	m_MPmtlBlendMode.caption = "Blend mode";
	m_MPoptTransform.caption = "Transform (override)?";
	
	m_MPmeshName.m_requestReload = true;
	
	m_MPart.Add( &m_MPmeshName );
	m_MPart.Add( &m_MPshader );
	m_MPart.Add( &m_MPtexture0 );
	m_MPart.Add( &m_MPtexture1 );
	m_MPart.Add( &m_MPtexture2 );
	m_MPart.Add( &m_MPtexture3 );
	m_MPart.Add( &m_MPtexture4 );
	m_MPart.Add( &m_MPtexture5 );
	m_MPart.Add( &m_MPtexture6 );
	m_MPart.Add( &m_MPtexture7 );
	m_MPart.Add( &m_MPmtlUnlit );
	m_MPart.Add( &m_MPmtlNocull );
	m_MPart.Add( &m_MPmtlDecal );
	m_MPart.Add( &m_MPmtlBlendMode );
	m_MPart.Add( &m_MPoptTransform );
	
	Add( &m_topCol );
	Add( &m_group );
	Add( &m_MPgroup );
	Add( &m_MPLgroup );
}

EDGUIAssetMesh::~EDGUIAssetMesh()
{
	m_MPLButtons.m_model = NULL;
}

void EDGUIAssetMesh::UpdatePreviewMesh()
{
	SGRX_MeshAsset& MA = g_EdAS->meshAssets[ m_mid ];
	MeshHandle mesh = SGRX_ProcessMeshAsset( g_EdAS, MA );
//	FC_SetMesh( mesh );
}

void EDGUIAssetMesh::ReloadPartList()
{
	SGRX_MeshAsset& MA = g_EdAS->meshAssets[ m_mid ];
	m_meshPartModel.meshAsset = &MA;
	m_MPLButtons.UpdateOptions();
	UpdatePreviewMesh();
}

void EDGUIAssetMesh::ReloadImpScene()
{
	g_UIPickers->SetMesh( new SGRX_Scene3D( m_sourceFile.m_value ) );
}

void EDGUIAssetMesh::Prepare( size_t mid )
{
	g_UIPickers->textureAsset.Reload();
	
	m_mid = mid;
	SGRX_MeshAsset& MA = g_EdAS->meshAssets[ mid ];
	
	m_sourceFile.SetValue( MA.sourceFile );
	m_outputCategory.SetValue( MA.outputCategory );
	m_outputName.SetValue( MA.outputName );
	m_rotateY2Z.SetValue( MA.rotateY2Z );
	m_flipUVY.SetValue( MA.flipUVY );
	m_transform.SetValue( MA.transform );
	
	ReloadPartList();
	PreparePart( NOT_FOUND );
	
	ReloadImpScene();
}

void EDGUIAssetMesh::PreparePart( size_t pid )
{
	m_pid = pid;
	SGRX_MeshAsset& MA = g_EdAS->meshAssets[ m_mid ];
	SGRX_MeshAssetPart* MP = m_pid != NOT_FOUND ? MA.parts[ m_pid ] : NULL;
	
	m_MPmeshName.SetValue( MP ? MP->meshName : "" );
	m_MPshader.SetValue( MP ? MP->shader : "" );
	m_MPtexture0.SetValue( MP ? MP->textures[ 0 ] : "" );
	m_MPtexture1.SetValue( MP ? MP->textures[ 1 ] : "" );
	m_MPtexture2.SetValue( MP ? MP->textures[ 2 ] : "" );
	m_MPtexture3.SetValue( MP ? MP->textures[ 3 ] : "" );
	m_MPtexture4.SetValue( MP ? MP->textures[ 4 ] : "" );
	m_MPtexture5.SetValue( MP ? MP->textures[ 5 ] : "" );
	m_MPtexture6.SetValue( MP ? MP->textures[ 6 ] : "" );
	m_MPtexture7.SetValue( MP ? MP->textures[ 7 ] : "" );
	m_MPmtlUnlit.SetValue( MP ? SGRX_GET_FLAG( MP->mtlFlags, SGRX_MtlFlag_Unlit ) : false );
	m_MPmtlNocull.SetValue( MP ? SGRX_GET_FLAG( MP->mtlFlags, SGRX_MtlFlag_Nocull ) : false );
	m_MPmtlDecal.SetValue( MP ? SGRX_GET_FLAG( MP->mtlFlags, SGRX_MtlFlag_Decal ) : false );
	m_MPmtlBlendMode.SetValue( SGRX_MtlBlend_ToString( MP ? MP->mtlBlendMode : 0 ) );
	m_MPoptTransform.SetValue( MP ? MP->optTransform : 0 );
	m_MPgroup.Clear();
	if( MP )
		m_MPgroup.Add( &m_MPart );
}

int EDGUIAssetMesh::OnEvent( EDGUIEvent* e )
{
	if( m_mid != NOT_FOUND )
	{
		SGRX_MeshAsset& MA = g_EdAS->meshAssets[ m_mid ];
		SGRX_MeshAssetPart* MP = m_pid != NOT_FOUND ? MA.parts[ m_pid ] : NULL;
		switch( e->type )
		{
		case EDGUI_EVENT_PROPEDIT:
			if( e->target == &m_sourceFile ){ MA.sourceFile = m_sourceFile.m_value; UpdatePreviewMesh(); }
			if( e->target == &m_outputCategory ){ MA.outputCategory = m_outputCategory.m_value; }
			if( e->target == &m_outputName ){ MA.outputName = m_outputName.m_value; }
			if( e->target == &m_rotateY2Z ){ MA.rotateY2Z = m_rotateY2Z.m_value; UpdatePreviewMesh(); }
			if( e->target == &m_flipUVY ){ MA.flipUVY = m_flipUVY.m_value; UpdatePreviewMesh(); }
			if( e->target == &m_transform ){ MA.transform = m_transform.m_value; UpdatePreviewMesh(); }
			if( MP )
			{
				bool ed = false;
				if( e->target == &m_MPmeshName ){ ed = true; MP->meshName = m_MPmeshName.m_value; }
				if( e->target == &m_MPshader ){ ed = true; MP->shader = m_MPshader.m_value; }
				if( e->target == &m_MPtexture0 ){ ed = true; MP->textures[ 0 ] = m_MPtexture0.m_value; }
				if( e->target == &m_MPtexture1 ){ ed = true; MP->textures[ 1 ] = m_MPtexture1.m_value; }
				if( e->target == &m_MPtexture2 ){ ed = true; MP->textures[ 2 ] = m_MPtexture2.m_value; }
				if( e->target == &m_MPtexture3 ){ ed = true; MP->textures[ 3 ] = m_MPtexture3.m_value; }
				if( e->target == &m_MPtexture4 ){ ed = true; MP->textures[ 4 ] = m_MPtexture4.m_value; }
				if( e->target == &m_MPtexture5 ){ ed = true; MP->textures[ 5 ] = m_MPtexture5.m_value; }
				if( e->target == &m_MPtexture6 ){ ed = true; MP->textures[ 6 ] = m_MPtexture6.m_value; }
				if( e->target == &m_MPtexture7 ){ ed = true; MP->textures[ 7 ] = m_MPtexture7.m_value; }
				if( e->target == &m_MPmtlUnlit ){ ed = true;
					SGRX_SET_FLAG( MP->mtlFlags, SGRX_MtlFlag_Unlit, m_MPmtlUnlit.m_value ); }
				if( e->target == &m_MPmtlNocull ){ ed = true;
					SGRX_SET_FLAG( MP->mtlFlags, SGRX_MtlFlag_Nocull, m_MPmtlNocull.m_value ); }
				if( e->target == &m_MPmtlDecal ){ ed = true;
					SGRX_SET_FLAG( MP->mtlFlags, SGRX_MtlFlag_Decal, m_MPmtlDecal.m_value ); }
				if( e->target == &m_MPmtlBlendMode ){ ed = true;
					MP->mtlBlendMode = SGRX_MtlBlend_FromString( m_MPmtlBlendMode.m_value ); }
				if( e->target == &m_MPoptTransform ){ ed = true;
					MP->optTransform = m_MPoptTransform.m_value; }
				if( ed )
				{
					ReloadPartList();
				}
			}
			break;
		case EDGUI_EVENT_PROPCHANGE:
			MA.ri.rev_asset++;
			if( e->target == &m_sourceFile ){ ReloadImpScene(); }
			break;
		case EDGUI_EVENT_BTNCLICK:
			if( e->target == &m_btnDuplicate )
			{
				SGRX_MeshAsset MAcopy;
				MAcopy.Clone( g_EdAS->meshAssets[ m_mid ] );
				m_mid = g_EdAS->meshAssets.size();
				MAcopy.outputName.append( " - Copy" );
				g_EdAS->meshAssets.push_back( MAcopy );
				Prepare( m_mid );
				return 1;
			}
			if( e->target == &m_btnDelete )
			{
				g_EdAS->meshAssets.erase( m_mid );
				m_mid = NOT_FOUND;
				m_meshPartModel.meshAsset = NULL;
				FC_EditMeshList();
				return 1;
			}
			if( e->target == &m_MPLBtnAdd )
			{
				MA.parts.push_back( new SGRX_MeshAssetPart );
				ReloadPartList();
				m_frame->UpdateMouse();
			}
			if( e->target == &m_MPLEditButton )
			{
				PreparePart( m_MPLEditButton.id2 );
			}
			if( e->target == &m_MPLEditButton.m_up )
			{
				size_t i = m_MPLEditButton.id2;
				if( i > 0 )
					TSWAP( MA.parts[ i ], MA.parts[ i - 1 ] );
				ReloadPartList();
				m_frame->UpdateMouse();
				return 1;
			}
			if( e->target == &m_MPLEditButton.m_dn )
			{
				size_t i = m_MPLEditButton.id2;
				if( i < MA.parts.size() - 1 )
					TSWAP( MA.parts[ i ], MA.parts[ i + 1 ] );
				ReloadPartList();
				m_frame->UpdateMouse();
				return 1;
			}
			if( e->target == &m_MPLEditButton.m_del )
			{
				MA.parts.erase( m_MPLEditButton.id2 );
				m_pid = NOT_FOUND;
				ReloadPartList();
				m_frame->UpdateMouse();
				return 1;
			}
			break;
		}
	}
	return EDGUILayoutRow::OnEvent( e );
}


struct EDGUIMeshModel : EDGUIItemNameFilterModel
{
	int GetSourceItemCount(){ return g_EdAS->meshAssets.size(); }
	void GetSourceItemName( int i, String& out ){ g_EdAS->meshAssets[ i ].GetDesc( out ); }
	void GetSourceItemSearchText( int i, String& out ){ g_EdAS->meshAssets[ i ].GetFullName( out ); }
}
g_MeshModel;


EDGUIAssetMeshList::EDGUIAssetMeshList() :
	m_group( true, "Mesh assets" ),
	m_editButton( false )
{
	m_btnAdd.caption = "Add mesh";
	m_filter.caption = "Filter";
	
	g_MeshModel.All();
	m_buttons.m_model = &g_MeshModel;
	m_buttons.Add( &m_editButton );
	m_group.Add( &m_buttons );
	Add( &m_btnAdd );
	Add( &m_filter );
	Add( &m_group );
}

void EDGUIAssetMeshList::Prepare()
{
	g_MeshModel.Search( m_filter.m_value );
	m_buttons.UpdateOptions();
}

int EDGUIAssetMeshList::OnEvent( EDGUIEvent* e )
{
	switch( e->type )
	{
	case EDGUI_EVENT_PROPEDIT:
		if( e->target == &m_filter )
		{
			Prepare();
		}
		break;
	case EDGUI_EVENT_BTNCLICK:
		if( e->target == &m_btnAdd )
		{
			g_EdAS->meshAssets.push_back( SGRX_MeshAsset() );
			FC_EditMesh( g_EdAS->meshAssets.size() - 1 );
			return 1;
		}
		if( e->target == &m_editButton )
		{
			FC_EditMesh( m_editButton.id2 );
			return 1;
		}
		if( e->target == &m_editButton.m_del )
		{
			g_EdAS->meshAssets.erase( m_editButton.id2 );
			Prepare();
			return 1;
		}
		break;
	}
	return EDGUILayoutRow::OnEvent( e );
}


EDGUIAssetAnimBundle::EDGUIAssetAnimBundle() :
	m_group( true, "Animation bundle" ),
	m_outputCategory( &g_UIPickers->category ),
	m_previewMesh( &g_UIPickers->meshAsset ),
	m_AN_group( true, "Animation" ),
	m_AN_source( &g_UIPickers->animName ),
	m_ANL_group( true, "Animation list" ),
	m_ANL_editButton( false ),
	m_AS_group( true, "Animation source" ),
	m_AS_fileName( &g_UIPickers->assetPath ),
	m_ASL_group( true, "Animation source list" ),
	m_ASL_editButton( false ),
	m_abid( NOT_FOUND ),
	m_sid( NOT_FOUND ),
	m_aid( NOT_FOUND )
{
	m_outputCategory.m_requestReload = true;
	
	m_btnDuplicate.caption = "Duplicate";
	m_btnDelete.caption = "Delete";
	
	m_outputCategory.caption = "Output category";
	m_outputName.caption = "Output name";
	m_bundlePrefix.caption = "Bundle prefix";
	m_previewMesh.caption = "Preview mesh";
	
	m_AN_source.caption = "Source/name";
	m_AN_name.caption = "Name override";
	m_AN_startFrame.caption = "Start frame";
	m_AN_endFrame.caption = "End frame";
	
	m_ANL_btnAdd.caption = "Add animation";
	
	m_AS_fileName.caption = "Source file";
	m_AS_prefix.caption = "Prefix";
	m_AS_addAll.caption = "Add all animations from this source";
	
	m_ASL_btnAdd.caption = "Add animation source";
	
	m_topCol.Add( &m_btnDuplicate );
	m_topCol.Add( &m_btnDelete );
	
	m_group.Add( &m_outputCategory );
	m_group.Add( &m_outputName );
	m_group.Add( &m_bundlePrefix );
	m_group.Add( &m_previewMesh );
	
	m_AN_cont.Add( &m_AN_source );
	m_AN_cont.Add( &m_AN_name );
	m_AN_cont.Add( &m_AN_startFrame );
	m_AN_cont.Add( &m_AN_endFrame );
	
	m_ANL_buttons.m_model = &m_animModel;
	m_ANL_buttons.Add( &m_ANL_editButton );
	m_ANL_group.Add( &m_ANL_btnAdd );
	m_ANL_group.Add( &m_ANL_buttons );
	
	m_AS_cont.Add( &m_AS_fileName );
	m_AS_cont.Add( &m_AS_prefix );
	m_AS_cont.Add( &m_AS_addAll );
	
	m_ASL_buttons.m_model = &m_sourceModel;
	m_ASL_buttons.Add( &m_ASL_editButton );
	m_ASL_group.Add( &m_ASL_btnAdd );
	m_ASL_group.Add( &m_ASL_buttons );
	
	Add( &m_topCol );
	Add( &m_group );
	Add( &m_AN_group );
	Add( &m_ANL_group );
	Add( &m_AS_group );
	Add( &m_ASL_group );
}

EDGUIAssetAnimBundle::~EDGUIAssetAnimBundle()
{
	m_ANL_buttons.m_model = NULL;
	m_ASL_buttons.m_model = NULL;
}

void EDGUIAssetAnimBundle::UpdatePreviewAnim()
{
	SGRX_AnimBundleAsset& ABA = g_EdAS->animBundleAssets[ m_abid ];
	AnimHandle anim = SGRX_ProcessSingleAnim( ABA, m_aid );
//	FC_SetAnim( g_EdAS->GetMesh( m_previewMesh.m_value ), anim );
}

void EDGUIAssetAnimBundle::Prepare( size_t abid )
{
	g_UIPickers->meshAsset.Reload();
	
	m_abid = abid;
	m_sid = NOT_FOUND;
	m_aid = NOT_FOUND;
	SGRX_AnimBundleAsset& ABA = g_EdAS->animBundleAssets[ abid ];
	
	m_outputCategory.SetValue( ABA.outputCategory );
	m_outputName.SetValue( ABA.outputName );
	m_bundlePrefix.SetValue( ABA.bundlePrefix );
	m_previewMesh.SetValue( ABA.previewMesh );
	
	ReloadAnimSourceList();
	PrepareAnimSource( NOT_FOUND );
	
	ReloadAnimList();
	PrepareAnim( NOT_FOUND );
}

void EDGUIAssetAnimBundle::ReloadAnimSourceList()
{
	SGRX_AnimBundleAsset& ABA = g_EdAS->animBundleAssets[ m_abid ];
	m_sourceModel.abAsset = &ABA;
	m_ASL_buttons.UpdateOptions();
	ReloadImpScenes();
}

void EDGUIAssetAnimBundle::PrepareAnimSource( size_t sid )
{
	m_sid = sid;
	
	if( m_sid != NOT_FOUND )
	{
		const SGRX_ABAnimSource& AS = g_EdAS->animBundleAssets[ m_abid ].sources[ sid ];
		m_AS_fileName.SetValue( AS.file );
		m_AS_prefix.SetValue( AS.prefix );
		
		m_AS_group.Add( &m_AS_cont );
	}
	else
		m_AS_group.Clear();
}

void EDGUIAssetAnimBundle::ReloadAnimList()
{
	SGRX_AnimBundleAsset& ABA = g_EdAS->animBundleAssets[ m_abid ];
	m_animModel.abAsset = &ABA;
	m_ANL_buttons.UpdateOptions();
	OnChangeLayout();
}

void EDGUIAssetAnimBundle::PrepareAnim( size_t aid )
{
	m_aid = aid;
	
	if( m_aid != NOT_FOUND )
	{
		const SGRX_ABAnimation& AN = g_EdAS->animBundleAssets[ m_abid ].anims[ aid ];
		m_AN_source.SetValue( AN.source );
		m_AN_name.SetValue( AN.name );
		
		char bfr[ 32 ];
		if( AN.startFrame != -1 )
		{
			sgrx_snprintf( bfr, 32, "%d", AN.startFrame );
			m_AN_startFrame.SetValue( bfr );
		}
		else
			m_AN_startFrame.SetValue( "" );
		if( AN.endFrame != -1 )
		{
			sgrx_snprintf( bfr, 32, "%d", AN.endFrame );
			m_AN_endFrame.SetValue( bfr );
		}
		else
			m_AN_endFrame.SetValue( "" );
		
		m_AN_group.Add( &m_AN_cont );
		
		UpdatePreviewAnim();
	}
	else
		m_AN_group.Clear();
}

void EDGUIAssetAnimBundle::ReloadImpScenes()
{
	SGRX_AnimBundleAsset& ABA = g_EdAS->animBundleAssets[ m_abid ];
	g_UIPickers->ClearAnimScenes();
	for( size_t i = 0; i < ABA.sources.size(); ++i )
	{
		g_UIPickers->AddAnimScene( new SGRX_Scene3D( ABA.sources[ i ].file, SIOF_Anims ) );
	}
	g_UIPickers->animName.Reload();
}

int EDGUIAssetAnimBundle::OnEvent( EDGUIEvent* e )
{
	if( m_abid != NOT_FOUND )
	{
		SGRX_AnimBundleAsset& ABA = g_EdAS->animBundleAssets[ m_abid ];
		SGRX_ABAnimSource* AS = m_sid != NOT_FOUND ? &ABA.sources[ m_sid ] : NULL;
		SGRX_ABAnimation* AN = m_aid != NOT_FOUND ? &ABA.anims[ m_aid ] : NULL;
		switch( e->type )
		{
		case EDGUI_EVENT_PROPEDIT:
			if( e->target == &m_outputCategory ){ ABA.outputCategory = m_outputCategory.m_value; }
			if( e->target == &m_outputName ){ ABA.outputName = m_outputName.m_value; }
			if( e->target == &m_bundlePrefix ){ ABA.bundlePrefix = m_bundlePrefix.m_value; }
			if( e->target == &m_previewMesh )
			{
				ABA.previewMesh = m_previewMesh.m_value;
				UpdatePreviewAnim();
			}
			if( AN )
			{
				bool edited = false;
				if( e->target == &m_AN_source ){ AN->source = m_AN_source.m_value; edited = true; }
				if( e->target == &m_AN_name ){ AN->name = m_AN_name.m_value; edited = true; }
				if( e->target == &m_AN_startFrame ){ edited = true;
					AN->startFrame = m_AN_startFrame.m_value.size() ? String_ParseInt( m_AN_startFrame.m_value ) : -1; }
				if( e->target == &m_AN_endFrame ){ edited = true;
					AN->endFrame = m_AN_endFrame.m_value.size() ? String_ParseInt( m_AN_endFrame.m_value ) : -1; }
				if( edited )
					ReloadAnimList();
			}
			if( AS )
			{
				bool edited = false;
				if( e->target == &m_AS_fileName ){ AS->file = m_AS_fileName.m_value; edited = true; }
				if( e->target == &m_AS_prefix ){ AS->prefix = m_AS_prefix.m_value; edited = true; }
				if( edited )
					ReloadAnimSourceList();
			}
			break;
		case EDGUI_EVENT_PROPCHANGE:
			if( e->target != &m_previewMesh )
			{
				ABA.ri.rev_asset++;
			}
			if( e->target == &m_AN_source || e->target == &m_AN_startFrame ||
				e->target == &m_AN_endFrame )
			{
				UpdatePreviewAnim();
			}
			break;
		case EDGUI_EVENT_BTNCLICK:
			if( e->target == &m_btnDuplicate )
			{
				SGRX_AnimBundleAsset ABAcopy;
				ABAcopy.Clone( g_EdAS->animBundleAssets[ m_abid ] );
				m_abid = g_EdAS->animBundleAssets.size();
				ABAcopy.outputName.append( " - Copy" );
				g_EdAS->animBundleAssets.push_back( ABAcopy );
				Prepare( m_abid );
				return 1;
			}
			if( e->target == &m_btnDelete )
			{
				g_EdAS->animBundleAssets.erase( m_abid );
				m_animModel.abAsset = NULL;
				m_sourceModel.abAsset = NULL;
				m_abid = NOT_FOUND;
				FC_EditAnimBundleList();
				return 1;
			}
			if( e->target == &m_ANL_btnAdd )
			{
				ABA.anims.push_back( SGRX_ABAnimation() );
				ReloadAnimList();
				m_frame->UpdateMouse();
			}
			if( e->target == &m_ANL_editButton )
			{
				PrepareAnim( m_ANL_editButton.id2 );
			}
			if( e->target == &m_ANL_editButton.m_del )
			{
				ABA.anims.erase( m_ANL_editButton.id2 );
				m_aid = NOT_FOUND;
				ReloadAnimList();
				m_frame->UpdateMouse();
				m_AN_group.Clear();
				return 1;
			}
			if( e->target == &m_AS_addAll && AS )
			{
				ImpScene3DHandle sh = new SGRX_Scene3D( AS->file, SIOF_Anims );
				Array< String > anims;
				sh->GetAnimList( anims );
				for( size_t i = 0; i < anims.size(); ++i )
				{
					ABA.anims.push_back( SGRX_ABAnimation() );
					ABA.anims.last().source = anims[ i ];
				}
				ABA.ri.rev_asset++;
				ReloadAnimList();
				m_frame->UpdateMouse();
			}
			if( e->target == &m_ASL_btnAdd )
			{
				ABA.sources.push_back( SGRX_ABAnimSource() );
				ReloadAnimSourceList();
				m_frame->UpdateMouse();
			}
			if( e->target == &m_ASL_editButton )
			{
				PrepareAnimSource( m_ASL_editButton.id2 );
			}
			if( e->target == &m_ASL_editButton.m_del )
			{
				ABA.sources.erase( m_ASL_editButton.id2 );
				m_sid = NOT_FOUND;
				ReloadAnimSourceList();
				m_frame->UpdateMouse();
				m_AS_group.Clear();
				return 1;
			}
			break;
		}
	}
	return EDGUILayoutRow::OnEvent( e );
}


struct EDGUIAnimBundleModel : EDGUIItemNameFilterModel
{
	int GetSourceItemCount(){ return g_EdAS->animBundleAssets.size(); }
	void GetSourceItemName( int i, String& out ){ g_EdAS->animBundleAssets[ i ].GetDesc( out ); }
	void GetSourceItemSearchText( int i, String& out ){ g_EdAS->animBundleAssets[ i ].GetFullName( out ); }
}
g_AnimBundleModel;


EDGUIAssetAnimBundleList::EDGUIAssetAnimBundleList() :
	m_group( true, "Anim. bundle assets" ),
	m_editButton( false )
{
	m_btnAdd.caption = "Add anim. bundle";
	m_filter.caption = "Filter";
	
	g_AnimBundleModel.All();
	m_buttons.m_model = &g_AnimBundleModel;
	m_buttons.Add( &m_editButton );
	m_group.Add( &m_buttons );
	Add( &m_btnAdd );
	Add( &m_filter );
	Add( &m_group );
}

void EDGUIAssetAnimBundleList::Prepare()
{
	g_AnimBundleModel.Search( m_filter.m_value );
	m_buttons.UpdateOptions();
}

int EDGUIAssetAnimBundleList::OnEvent( EDGUIEvent* e )
{
	switch( e->type )
	{
	case EDGUI_EVENT_PROPEDIT:
		if( e->target == &m_filter )
		{
			Prepare();
		}
		break;
	case EDGUI_EVENT_BTNCLICK:
		if( e->target == &m_btnAdd )
		{
			g_EdAS->animBundleAssets.push_back( SGRX_AnimBundleAsset() );
			FC_EditAnimBundle( g_EdAS->animBundleAssets.size() - 1 );
			return 1;
		}
		if( e->target == &m_editButton )
		{
			FC_EditAnimBundle( m_editButton.id2 );
			return 1;
		}
		if( e->target == &m_editButton.m_del )
		{
			g_EdAS->animBundleAssets.erase( m_editButton.id2 );
			Prepare();
			return 1;
		}
		break;
	}
	return EDGUILayoutRow::OnEvent( e );
}


EDGUIMainFrame::EDGUIMainFrame() :
	m_UIMenuSplit( true, 26, 0 ),
	m_UIParamSplit( false, 0, 0.55f )
{
	m_UIMenuSplit.SetSecondPane( &m_UIParamSplit );
	m_UIParamSplit.SetSecondPane( &m_UIParamScroll );
	m_UIParamScroll.Add( &m_UIParamList );
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

void EDGUIMainFrame::EditMesh( size_t id )
{
	ClearParamList();
	m_UIMesh.Prepare( id );
	AddToParamList( &m_UIMesh );
}
void EDGUIMainFrame::EditMeshList()
{
	ClearParamList();
	m_UIMeshList.Prepare();
	AddToParamList( &m_UIMeshList );
}
void EDGUIMainFrame::EditAnimBundle( size_t id )
{
	ClearParamList();
	m_UIAnimBundle.Prepare( id );
	AddToParamList( &m_UIAnimBundle );
}
void EDGUIMainFrame::EditAnimBundleList()
{
	ClearParamList();
	m_UIAnimBundleList.Prepare();
	AddToParamList( &m_UIAnimBundleList );
}



//
// ASSET
//

bool PickCategoryName( const char* label, String& name )
{
	int id = 0;
	String namelist = "<None>";
	namelist.push_back( '\0' );
	for( size_t i = 0; i < g_EdAS->categories.size(); ++i )
	{
		if( g_EdAS->categories.item( i ).key == name )
			id = i + 1;
		namelist.append( g_EdAS->categories.item( i ).key );
		namelist.append( " (" );
		namelist.append( g_EdAS->categories.item( i ).value );
		namelist.append( ")" );
		namelist.push_back( '\0' );
	}
	namelist.push_back( '\0' );
	bool ret = ImGui::Combo( label, &id, namelist.data() );
	if( ret )
	{
		if( id > 0 && id <= (int) g_EdAS->categories.size() )
			name = g_EdAS->categories.item( id - 1 ).key;
		else
			name = "";
	}
	return ret;
}

void EditFilter( size_t i, SGRX_ImageFilter* IF )
{
	static const char* sharpen_modes[] = { "0-1", "1-1", "1-2" };
	
	switch( IF->GetType() )
	{
	case SGRX_AIF_Resize: {
		SGRX_CAST( SGRX_ImageFilter_Resize*, F, IF );
		IMGUI_GROUP( "Resize", true,
		{
			IMGUIEditInt( "Width", F->width, 1, 4096 );
			IMGUIEditInt( "Height", F->height, 1, 4096 );
			IMGUIEditInt( "Depth", F->depth, 1, 4096 );
			IMGUIEditBool( "SRGB", F->srgb );
		});
		} break;
	case SGRX_AIF_Rearrange: {
		SGRX_CAST( SGRX_ImageFilter_Rearrange*, F, IF );
		IMGUI_GROUP( "Rearrange", true,
		{
			IMGUIEditInt( "Width", F->width, 1, 4096 );
		});
		} break;
	case SGRX_AIF_Sharpen: {
		SGRX_CAST( SGRX_ImageFilter_Sharpen*, F, IF );
		IMGUI_GROUP( "Sharpen", true,
		{
			IMGUIEditFloat( "Factor", F->factor, 0, 1 );
			IMGUI_COMBOBOX( "Mode", F->mode, sharpen_modes );
		});
		} break;
	case SGRX_AIF_ToLinear: {
		IMGUI_GROUP( "To linear", true, { ImGui::Text( "- no parameters -" ); } );
		} break;
	case SGRX_AIF_FromLinear: {
		IMGUI_GROUP( "From linear", true, { ImGui::Text( "- no parameters -" ); } );
		} break;
	case SGRX_AIF_ExpandRange: {
		SGRX_CAST( SGRX_ImageFilter_ExpandRange*, F, IF );
		IMGUI_GROUP( "Expand range", true,
		{
			IMGUIEditVec4( "Min. value", F->vmin, 0, 1 );
			IMGUIEditVec4( "Max. value", F->vmax, 0, 1 );
		});
		} break;
	case SGRX_AIF_BCP: {
		SGRX_CAST( SGRX_ImageFilter_BCP*, F, IF );
		IMGUI_GROUP( "Brightness/contrast/power", true,
		{
			IMGUIEditBool( "Apply brightness/contrast 1", F->apply_bc1 );
			IMGUIEditFloat( "Brightness 1", F->brightness, -10, 10 );
			IMGUIEditFloat( "Contrast 1", F->contrast, 0.01f, 100 );
			IMGUIEditBool( "Apply power", F->apply_pow );
			IMGUIEditFloat( "Power", F->power, 0.01f, 100 );
			IMGUIEditBool( "Apply brightness/contrast 2", F->apply_bc2 );
			IMGUIEditFloat( "Brightness 2", F->brightness_2, -10, 10 );
			IMGUIEditFloat( "Contrast 2", F->contrast_2, 0.01f, 100 );
		});
		} break;
	default: ImGui::Text( "<UNKNOWN FILTER>" ); break;
	}
}

void EditTextureAsset( SGRX_TextureAsset& ta )
{
	bool chg = false;
	bool rev = false;
	
	ImGui::Text( "Texture" );
	ImGui::Separator();
	
	chg |= g_NUIAssetPicker->Property( "Select source file", "Source file", ta.sourceFile );
	rev |= PickCategoryName( "Output category", ta.outputCategory );
	rev |= IMGUIEditString( "Output name", ta.outputName, 256 );
	
	if( ImGui::Button( SGRX_TextureOutputFormat_ToString( ta.outputType ),
		ImVec2( ImGui::GetContentRegionAvailWidth() * 2.f/3.f, 20 ) ) )
		ImGui::OpenPopup( "output_type" );
	ImGui::SameLine();
	ImGui::Text( "Output type" );
	if( ImGui::BeginPopup( "output_type" ) )
	{
		if( ImGui::Selectable( SGRX_TextureOutputFormat_ToString( SGRX_TOF_PNG_RGBA32 ) ) )
		{
			ta.outputType = SGRX_TOF_PNG_RGBA32;
			rev |= true;
		}
		if( ImGui::Selectable( SGRX_TextureOutputFormat_ToString( SGRX_TOF_STX_RGBA32 ) ) )
		{
			ta.outputType = SGRX_TOF_STX_RGBA32;
			rev |= true;
		}
		ImGui::EndPopup();
	}
	
	chg |= IMGUIEditBool( "Is SRGB?", ta.isSRGB );
	chg |= IMGUIEditBool( "Generate mipmaps", ta.mips );
	chg |= IMGUIEditBool( "Use linear interpolation", ta.lerp );
	chg |= IMGUIEditBool( "Clamp X", ta.clampx );
	chg |= IMGUIEditBool( "Clamp Y", ta.clampy );
	
	ImGui::Separator();
	ImGui::Text( "Filters" );
	ImGui::Separator();
	
	if( ImGui::Button( "Add filter", ImVec2( ImGui::GetContentRegionAvail().x, 24 ) ) )
		ImGui::OpenPopup( "add_filter" );
	if( ImGui::BeginPopup( "add_filter" ) )
	{
		ImGui::Text( "Select filter type:" );
		ImGui::Separator();
		if( ImGui::Selectable( "Resize" ) ) ta.filters.push_back( new SGRX_ImageFilter_Resize );
		if( ImGui::Selectable( "Rearrange" ) ) ta.filters.push_back( new SGRX_ImageFilter_Rearrange );
		if( ImGui::Selectable( "Sharpen" ) ) ta.filters.push_back( new SGRX_ImageFilter_Sharpen );
		if( ImGui::Selectable( "To linear" ) ) ta.filters.push_back( new SGRX_ImageFilter_Linear(false) );
		if( ImGui::Selectable( "From linear" ) ) ta.filters.push_back( new SGRX_ImageFilter_Linear(true) );
		if( ImGui::Selectable( "Expand range" ) ) ta.filters.push_back( new SGRX_ImageFilter_ExpandRange );
		if( ImGui::Selectable( "Brightness/contrast/power" ) ) ta.filters.push_back( new SGRX_ImageFilter_BCP );
		ImGui::EndPopup();
	}
	IMGUIEditArray( ta.filters, EditFilter, NULL );
	
	if( chg || rev )
		ta.ri.rev_asset++;
}

void EditMeshAsset( SGRX_MeshAsset& ma )
{
	ImGui::Text( "Mesh" );
	ImGui::Separator();
	
}

void EditAnimBundleAsset( SGRX_AnimBundleAsset& aba )
{
	ImGui::Text( "Animation bundle" );
	ImGui::Separator();
	
}


SGRX_Asset* g_CurAsset;
String g_CurCategory;
String g_CurCatName;
String g_CurCatPath;
bool g_RenameAssetCats = false;

void SetCurAsset( SGRX_Asset* asset )
{
	g_CurAsset = asset;
}
void SetCurCategory( size_t i )
{
	if( i == NOT_FOUND )
	{
		g_CurCategory = "";
		g_CurCatName = "";
		g_CurCatPath = "";
		return;
	}
	g_CurCategory = g_EdAS->categories.item( i ).key;
	g_CurCatName = g_CurCategory;
	g_CurCatPath = g_EdAS->categories.item( i ).value;
}


void EditCurAsset()
{
	if( !g_CurAsset )
	{
		ImGui::Text( "Select asset on the left side to edit it" );
		return;
	}
	
	ImGui::Columns( 2 );
	if( ImGui::Button( "Duplicate", ImVec2( ImGui::GetContentRegionAvailWidth(), 16 ) ) )
	{
		switch( g_CurAsset->assetType )
		{
		case SGRX_AT_Texture: {
				SGRX_TextureAsset new_asset = *g_CurAsset->ToTexture();
				g_EdAS->textureAssets.push_back( new_asset );
				g_CurAsset = &g_EdAS->textureAssets.last();
			} break;
		case SGRX_AT_Mesh: {
				SGRX_MeshAsset new_asset = *g_CurAsset->ToMesh();
				g_EdAS->meshAssets.push_back( new_asset );
				g_CurAsset = &g_EdAS->meshAssets.last();
			} break;
		case SGRX_AT_AnimBundle: {
				SGRX_AnimBundleAsset new_asset = *g_CurAsset->ToAnimBundle();
				g_EdAS->animBundleAssets.push_back( new_asset );
				g_CurAsset = &g_EdAS->animBundleAssets.last();
			} break;
		}
		g_CurAsset->outputName.append( " - Copy" );
	}
	ImGui::NextColumn();
	if( ImGui::Button( "Delete", ImVec2( ImGui::GetContentRegionAvailWidth(), 16 ) ) )
	{
		switch( g_CurAsset->assetType )
		{
		case SGRX_AT_Texture:
			g_EdAS->textureAssets.erase( g_CurAsset->ToTexture() - g_EdAS->textureAssets.data() );
			break;
		case SGRX_AT_Mesh:
			g_EdAS->meshAssets.erase( g_CurAsset->ToMesh() - g_EdAS->meshAssets.data() );
			break;
		case SGRX_AT_AnimBundle:
			g_EdAS->animBundleAssets.erase( g_CurAsset->ToAnimBundle() - g_EdAS->animBundleAssets.data() );
			break;
		}
		g_CurAsset = NULL;
		return;
	}
	ImGui::Columns( 1 );
	ImGui::Separator();
	
	switch( g_CurAsset->assetType )
	{
	case SGRX_AT_Texture: EditTextureAsset( *g_CurAsset->ToTexture() ); break;
	case SGRX_AT_Mesh: EditMeshAsset( *g_CurAsset->ToMesh() ); break;
	case SGRX_AT_AnimBundle: EditAnimBundleAsset( *g_CurAsset->ToAnimBundle() ); break;
	}
}


void RenameAssetCat( SGRX_Asset* A )
{
	if( A->outputCategory == g_CurCategory )
	{
		A->outputCategory = g_CurCatName;
		A->ri.rev_asset++;
	}
}

void EditCurCategory()
{
	if( ImGui::Selectable( "Back to category list" ) )
	{
		SetCurCategory( NOT_FOUND );
		return;
	}
	ImGui::Separator();
	
	ImGui::Text( "Edit category: %s", StackPath(g_CurCategory).str );
	ImGui::Separator();
	
	IMGUIEditString( "Name", g_CurCatName, 256 );
	IMGUIEditString( "Path", g_CurCatPath, 256 );
	IMGUIEditBool( "Also edit asset categories on rename", g_RenameAssetCats );
	
	const char* act = g_CurCatName != g_CurCategory
		? ( g_EdAS->categories.isset( g_CurCatName ) ? "Move/Overwrite" : "Move/rename" )
		: "Edit";
	if( ImGui::Button( act ) )
	{
		if( g_CurCatName != g_CurCategory )
		{
			g_EdAS->categories.unset( g_CurCategory );
		}
		g_EdAS->categories.set( g_CurCatName, g_CurCatPath );
		
		if( g_RenameAssetCats && g_CurCatName != g_CurCategory )
		{
			for( size_t i = 0; i < g_EdAS->textureAssets.size(); ++i )
				RenameAssetCat( &g_EdAS->textureAssets[ i ] );
			for( size_t i = 0; i < g_EdAS->meshAssets.size(); ++i )
				RenameAssetCat( &g_EdAS->meshAssets[ i ] );
			for( size_t i = 0; i < g_EdAS->animBundleAssets.size(); ++i )
				RenameAssetCat( &g_EdAS->animBundleAssets[ i ] );
		}
		
		SetCurCategory( NOT_FOUND );
	}
}

void EditCategoryList()
{
	if( g_CurCategory.size() )
	{
		EditCurCategory();
		return;
	}
	
	ImGui::Text( "Categories" );
	ImGui::Separator();
	
	for( size_t i = 0; i < g_EdAS->categories.size(); ++i )
	{
		char bfr[ 256 ];
		sgrx_snprintf( bfr, 256, "%s => %s",
			StackString<100>( g_EdAS->categories.item( i ).key ).str,
			StackString<100>( g_EdAS->categories.item( i ).value ).str );
		if( ImGui::Selectable( bfr ) )
			SetCurCategory( i );
	}
}


//
// ASSET LIST
//

enum EGroupBy
{
	GB_None,
	GB_Type,
	GB_Category,
	
	GB__MAX,
};

enum ESortBy
{
	SB_Name_ASC,
	SB_Name_DESC,
	
	SB__MAX,
};

bool g_ShowTextures = true;
bool g_ShowMeshes = true;
bool g_ShowAnimBundles = true;
int g_GroupBy = GB_Category;
int g_SortBy = SB_Name_ASC;
String g_Filter;

#define IMPL_ASSET_SORT_FN( name, test1, test2 ) \
	int name( const void* a, const void* b ) \
	{ \
		SGRX_CAST( SGRX_Asset**, pa, a ); \
		SGRX_CAST( SGRX_Asset**, pb, b ); \
		int t1 = test1; if( t1 ) return t1; \
		int t2 = test2; if( t2 ) return t2; \
		return 0; \
	}
#define ISORT( sa, sb, p ) (int((sa)->p) - int((sb)->p))
#define SCOMP( sa, sb, p ) (SV((sa)->p).compare_to( (sb)->p ))
IMPL_ASSET_SORT_FN( assetsort_none_namea, 0, SCOMP( *pa, *pb, outputName ) );
IMPL_ASSET_SORT_FN( assetsort_none_named, 0, -SCOMP( *pa, *pb, outputName ) );
IMPL_ASSET_SORT_FN( assetsort_type_namea, ISORT( *pa, *pb, assetType ), SCOMP( *pa, *pb, outputName ) );
IMPL_ASSET_SORT_FN( assetsort_type_named, ISORT( *pa, *pb, assetType ), -SCOMP( *pa, *pb, outputName ) );
IMPL_ASSET_SORT_FN( assetsort_cat_namea, SCOMP( *pa, *pb, outputCategory ), SCOMP( *pa, *pb, outputName ) );
IMPL_ASSET_SORT_FN( assetsort_cat_named, SCOMP( *pa, *pb, outputCategory ), -SCOMP( *pa, *pb, outputName ) );
int (*g_AssetCmpFuncs[ GB__MAX ][ SB__MAX ])( const void*, const void* ) =
{
	{ assetsort_none_namea, assetsort_none_named },
	{ assetsort_type_namea, assetsort_type_named },
	{ assetsort_cat_namea, assetsort_cat_named },
};

void EditAssetList()
{
	static const char* type_letters[] = { "T", "M", "A" };
	static const char* type_names[] = { "Textures", "Meshes", "Anim. bundles" };
	static const char* gb_options[] = { "None", "Type", "Category" };
	static const char* sb_options[] = { "Name [ASC]", "Name [DESC]" };
	
	IMGUIEditBool( "Show textures", g_ShowTextures );
	ImGui::SameLine();
	IMGUIEditBool( "Show meshes", g_ShowMeshes );
	IMGUIEditBool( "Show anim. bundles", g_ShowAnimBundles );
	IMGUI_COMBOBOX( "Group by", g_GroupBy, gb_options );
	IMGUI_COMBOBOX( "Sort by", g_SortBy, sb_options );
	IMGUIEditString( "Search", g_Filter, 256 );
	ImGui::Separator();
	
	Array< SGRX_Asset* > assets;
	if( g_ShowTextures )
	{
		for( size_t i = 0; i < g_EdAS->textureAssets.size(); ++i )
		{
			if( !g_Filter.size() || SV(g_EdAS->textureAssets[ i ].outputName).match_loose( g_Filter ) )
				assets.push_back( &g_EdAS->textureAssets[ i ] );
		}
	}
	if( g_ShowMeshes )
	{
		for( size_t i = 0; i < g_EdAS->meshAssets.size(); ++i )
		{
			if( !g_Filter.size() || SV(g_EdAS->meshAssets[ i ].outputName).match_loose( g_Filter ) )
				assets.push_back( &g_EdAS->meshAssets[ i ] );
		}
	}
	if( g_ShowAnimBundles )
	{
		for( size_t i = 0; i < g_EdAS->animBundleAssets.size(); ++i )
		{
			if( !g_Filter.size() || SV(g_EdAS->animBundleAssets[ i ].outputName).match_loose( g_Filter ) )
				assets.push_back( &g_EdAS->animBundleAssets[ i ] );
		}
	}
	qsort( assets.data(), assets.size(), sizeof(SGRX_Asset*), g_AssetCmpFuncs[ g_GroupBy ][ g_SortBy ] );
	
	if( ImGui::BeginChild( "Assets", ImGui::GetContentRegionAvail() ) )
	{
		StringView lastgroup;
		bool lastopen = false;
		
		char bfr[ 256 ];
		for( size_t i = 0; i < assets.size(); ++i )
		{
			SGRX_Asset* A = assets[ i ];
			
			StringView curgroup;
			if( g_GroupBy == GB_Type )
			{
				curgroup = type_names[ A->assetType ];
			}
			else if( g_GroupBy == GB_Category )
			{
				curgroup = A->outputCategory;
			}
			if( curgroup != lastgroup )
			{
				if( lastopen )
					ImGui::TreePop();
			//	ImGui::SetNextWindowCollapsed( false, ImGuiSetCond_Appearing );
				lastopen = ImGui::TreeNode( StackPath(curgroup) );
				lastgroup = curgroup;
			}
			
			if( lastopen || g_GroupBy == GB_None )
			{
				sgrx_snprintf( bfr, 256, "[%s] %.*s [%.*s]", type_letters[ A->assetType ],
					(int) A->outputName.size(), A->outputName.data(),
					(int) A->outputCategory.size(), A->outputCategory.data() );
				if( ImGui::MenuItem( bfr ) )
				{
					SetCurAsset( A );
				}
			}
		}
		
		if( lastopen )
			ImGui::TreePop();
		
		ImGui::EndChild();
	}
}



struct AssetRenderView : IMGUIRenderView
{
	AssetRenderView() : IMGUIRenderView( g_EdScene )
	{
		m_meshPrevInst = m_scene->CreateMeshInstance();
		lmm_prepmeshinst( m_meshPrevInst );
	}
	void DebugDraw()
	{
		BatchRenderer& br = GR2D_GetBatchRenderer();
		
		if( m_texPreview )
		{
			const TextureInfo& info = m_texPreview.GetInfo();
			float aspect = safe_fdiv( info.width, info.height );
			float w = 2 * TMAX( 1.0f, aspect );
			float h = 2 / TMIN( 1.0f, aspect );
			br.Reset();
			br.SetTexture( m_texPreview );
			br.Box( 0, 0, w, h );
		}
		
		SGRX_IMesh* mesh = m_meshPrevInst->GetMesh();
		if( mesh && m_meshPrevInst->enabled )
		{
			br.Reset();
			if( m_meshPrevInst->skin_matrices.size() )
			{
				for( size_t i = 0; i < m_meshPrevInst->skin_matrices.size(); ++i )
				{
					float sxt = ( 1 - float(i) / m_meshPrevInst->skin_matrices.size() );
					br.Axis( mesh->m_bones[ i ].skinOffset * m_meshPrevInst->skin_matrices[ i ], 0.1f + sxt * 0.1f );
				}
			}
			else
			{
				for( int i = 0; i < mesh->m_numBones; ++i )
				{
					float sxt = ( 1 - float(i) / mesh->m_numBones );
					br.Axis( mesh->m_bones[ i ].skinOffset, 0.1f + sxt * 0.1f );
				}
			}
		}
	}
	
	// preview data
	TextureHandle m_texPreview;
	MeshInstHandle m_meshPrevInst;
	AnimPlayer m_animPreview;
};

void FC_SetTexture( TextureHandle tex )
{
	g_NUIRenderView->m_texPreview = tex;
	g_NUIRenderView->m_meshPrevInst->enabled = false;
	g_NUIRenderView->m_meshPrevInst->skin_matrices.resize( 0 );
	g_NUIRenderView->m_animPreview.Prepare( NULL );
	g_NUIRenderView->m_animPreview.Stop();
}
void FC_SetMesh( MeshHandle mesh )
{
	g_NUIRenderView->m_texPreview = NULL;
	g_NUIRenderView->m_meshPrevInst->SetMesh( mesh );
	g_NUIRenderView->m_meshPrevInst->enabled = mesh != NULL;
	g_NUIRenderView->m_meshPrevInst->skin_matrices.resize( 0 );
	g_NUIRenderView->m_animPreview.Prepare( NULL );
	g_NUIRenderView->m_animPreview.Stop();
}
void FC_SetAnim( MeshHandle mesh, AnimHandle anim )
{
	g_NUIRenderView->m_texPreview = NULL;
	g_NUIRenderView->m_meshPrevInst->SetMesh( mesh );
	g_NUIRenderView->m_meshPrevInst->enabled = mesh != NULL;
	g_NUIRenderView->m_meshPrevInst->skin_matrices.resize( mesh->m_numBones );
	g_NUIRenderView->m_animPreview.Prepare( mesh );
	g_NUIRenderView->m_animPreview.Stop();
	g_NUIRenderView->m_animPreview.Play( anim );
}


void ASCR_Open()
{
	LOG << "Trying to open asset script";
	
	if( g_EdAS->Load( ASSET_SCRIPT_NAME ) == false )
	{
		LOG_ERROR << "FAILED TO LOAD ASSET SCRIPT";
		return;
	}
	g_EdAS->LoadAssetInfo( ASSET_INFO_NAME );
}

void ASCR_Save()
{
	LOG << "Trying to save asset script";
	
	if( g_EdAS->Save( ASSET_SCRIPT_NAME ) == false )
	{
		LOG_ERROR << "FAILED TO SAVE ASSET SCRIPT";
		return;
	}
	if( g_EdAS->SaveAssetInfo( ASSET_INFO_NAME ) == false )
	{
		LOG_ERROR << "FAILED TO SAVE ASSET INFO";
		return;
	}
}

void ASCR_Run( bool force )
{
	g_EdAS->LoadOutputInfo( OUTPUT_INFO_NAME );
	SGRX_ProcessAssets( *g_EdAS, force );
	g_EdAS->SaveOutputInfo( OUTPUT_INFO_NAME );
}


enum EditorMode
{
	EditAssets,
	EditCategories,
	MiscProps,
};
int g_mode = EditAssets;



struct ASEditor : IGame
{
	bool OnConfigure( int argc, char** argv )
	{
		int op = 0;
		const char* cat = NULL;
		for( int i = 1; i < argc; ++i )
		{
			if( !strcmp( argv[ i ], "--dtex" ) ) op = 1;
			if( !strcmp( argv[ i ], "--gmtl" ) ) op = 2;
			if( !strncmp( argv[ i ], "--cat=", 6 ) )
				cat = argv[ i ] + 6;
			else if( !strcmp( argv[ i ], "-c" ) && i + 1 < argc )
				cat = argv[ ++i ];
		}
		switch( op )
		{
		case 1: // dump textures
			{
				SGRX_AssetScript as;
				as.Load( ASSET_SCRIPT_NAME );
				for( size_t i = 0; i < as.textureAssets.size(); ++i )
				{
					SGRX_TextureAsset& TA = as.textureAssets[ i ];
					if( cat )
					{
						if( TA.outputCategory != StringView(cat) )
							continue;
					}
					printf( "%s/%s.%s\n",
						StackString<256>(as.GetCategoryPath(TA.outputCategory)).str,
						StackString<256>(TA.outputName).str,
						SGRX_TextureOutputFormat_Ext( TA.outputType ) );
				}
			}
			return false;
		case 2: // generate materials
			{
				SGRX_AssetScript as;
				as.Load( ASSET_SCRIPT_NAME );
				for( size_t i = 0; i < as.textureAssets.size(); ++i )
				{
					SGRX_TextureAsset& TA = as.textureAssets[ i ];
					if( cat )
					{
						if( TA.outputCategory != StringView(cat) )
							continue;
					}
					printf( "material %s\nshader default\n0 %s/%s.%s\n",
						StackString<256>(TA.outputName).str,
						StackString<256>(as.GetCategoryPath(TA.outputCategory)).str,
						StackString<256>(TA.outputName).str,
						SGRX_TextureOutputFormat_Ext( TA.outputType ) );
				}
			}
			return false;
		}
		return true;
	}
	bool OnInitialize()
	{
		GR2D_LoadFont( "core", "fonts/lato-regular.ttf" );
		GR2D_SetFont( "core", 12 );
		
		g_UIPickers = new EDGUIPickers;
		
		// core layout
		g_EdScene = GR_CreateScene();
		g_EdScene->camera.position = V3(3);
		g_EdScene->camera.UpdateMatrices();
		g_EdAS = new SGRX_AssetScript;
		g_UIFrame = new EDGUIMainFrame();
		g_UIFrame->Resize( GR_GetWidth(), GR_GetHeight() );
		
		SGRX_IMGUI_Init();
		
		g_NUIRenderView = new AssetRenderView;
		g_NUIAssetPicker = new IMGUIFilePicker( "assets", "" );
		g_NUIAssetPicker->m_layoutType = IMGUIPickerCore::Layout_List;
		
		ASCR_Open();
		
		return true;
	}
	void OnDestroy()
	{
		delete g_NUIAssetPicker;
		delete g_NUIRenderView;
		
		delete g_UIPickers;
		g_UIPickers = NULL;
		delete g_UIFrame;
		g_UIFrame = NULL;
		delete g_EdAS;
		g_EdAS = NULL;
		g_EdScene = NULL;
		
		SGRX_IMGUI_Free();
	}
	static bool IsImageFile( StringView path )
	{
		StringView ext = path.after_last( "." );
		return ext.equal_lower( "png" )
			|| ext.equal_lower( "jpg" )
			|| ext.equal_lower( "jpeg" )
			|| ext.equal_lower( "dds" )
			|| ext.equal_lower( "tga" )
			|| ext.equal_lower( "bmp" );
	}
	static bool IsMeshFile( StringView path )
	{
		StringView ext = path.after_last( "." );
		return ext.equal_lower( "dae" )
			|| ext.equal_lower( "fbx" )
			|| ext.equal_lower( "obj" )
			|| ext.equal_lower( "ssm" );
	}
	void OnEvent( const Event& e )
	{
		if( e.type == SDL_DROPFILE )
		{
			String path = RealPath( e.drop.file );
			if( IsImageFile( path ) )
			{
				String rootpath;
				FS_FindRealPath( "assets", rootpath );
				rootpath = RealPath( rootpath );
				if( PathIsUnder( path, rootpath ) )
				{
					String subpath = String_Concat( "assets/", GetRelativePath( path, rootpath ) );
					String category = "";
					if( g_CurAsset )
						category = g_CurAsset->outputCategory;
					String name = SV( subpath ).after_last( "/" ).until_last( "." );
					LOG << "Dropped image";
					LOG << "file:" << subpath;
					LOG << "name:" << name;
					LOG << "category:" << category;
					
					SGRX_TextureAsset ta;
					ta.sourceFile = subpath;
					ta.outputCategory = category;
					ta.outputName = name;
					ta.outputType = SGRX_TOF_STX_RGBA32;
					g_EdAS->textureAssets.push_back( ta );
					SetCurAsset( &g_EdAS->textureAssets.last() );
				}
			}
			if( IsMeshFile( path ) )
			{
				String rootpath;
				FS_FindRealPath( "assets", rootpath );
				rootpath = RealPath( rootpath );
				if( PathIsUnder( path, rootpath ) )
				{
					String subpath = String_Concat( "assets/", GetRelativePath( path, rootpath ) );
					String category = "";
					if( g_CurAsset )
						category = g_CurAsset->outputCategory;
					String name = SV( subpath ).after_last( "/" ).until_last( "." );
					LOG << "Dropped mesh";
					LOG << "file:" << subpath;
					LOG << "name:" << name;
					LOG << "category:" << category;
					
					SGRX_MeshAsset ma;
					ma.sourceFile = subpath;
					ma.outputCategory = category;
					ma.outputName = name;
					g_EdAS->meshAssets.push_back( ma );
					SetCurAsset( &g_EdAS->meshAssets.last() );
				}
			}
		}
		SGRX_IMGUI_Event( e );
	}
	void OnTick( float dt, uint32_t gametime )
	{
		GR2D_SetViewMatrix( Mat4::CreateUI( 0, 0, GR_GetWidth(), GR_GetHeight() ) );
		AnimInfo info;
		GR_ApplyAnimator( &g_NUIRenderView->m_animPreview, g_NUIRenderView->m_meshPrevInst );
		g_NUIRenderView->m_animPreview.Advance( dt, &info );
		
		SGRX_IMGUI_NewFrame();
		
		IMGUI_MAIN_WINDOW_BEGIN
		{
			if( ImGui::BeginMenuBar() )
			{
				if( ImGui::Button( "Save" ) )
				{
					ASCR_Save();
				}
				ImGui::SameLine();
				if( ImGui::Button( "Run" ) )
				{
					ASCR_Run( false );
				}
				ImGui::SameLine();
				if( ImGui::BeginMenu( "Force run" ) )
				{
					if( ImGui::MenuItem( "Force run" ) )
					{
						ASCR_Run( true );
					}
					ImGui::EndMenu();
				}
				
				ImGui::SameLine( 0, 50 );
				ImGui::Text( "Edit mode:" );
				ImGui::SameLine();
				ImGui::RadioButton( "Assets", &g_mode, EditAssets );
				ImGui::SameLine();
				ImGui::RadioButton( "Categories", &g_mode, EditCategories );
				ImGui::SameLine();
				ImGui::RadioButton( "Misc. settings", &g_mode, MiscProps );
				
				ImGui::EndMenuBar();
			}
			
			IMGUI_HSPLIT3( 0.2f, 0.7f,
			{
				EditAssetList();
			},
			{
				g_NUIRenderView->Process( dt );
			},
			{
				if( g_mode == EditAssets )
				{
					EditCurAsset();
				}
				else if( g_mode == EditCategories )
				{
					EditCategoryList();
				}
				else if( g_mode == MiscProps )
				{
					g_NUIRenderView->EditCameraParams();
				}
			});
		}
		IMGUI_MAIN_WINDOW_END;
		
		SGRX_IMGUI_Render();
		SGRX_IMGUI_ClearEvents();
	}
};


extern "C" EXPORT IGame* CreateGame()
{
	return new ASEditor;
}

