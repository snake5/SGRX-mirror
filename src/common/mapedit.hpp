

#pragma once
#define USE_HASHTABLE
#include "compiler.hpp"
#include "edutils.hpp"
#include "edcomui.hpp"


// v0: initial
// v1: added surface.lmquality
// v2: added ent[light].flareoffset
// v3: added surface.xfit/yfit, added groups
#define MAP_FILE_VERSION 3

#define MAX_BLOCK_POLYGONS 32

#define EDGUI_EVENT_SETENTITY EDGUI_EVENT_USER + 1


#ifdef MAPEDIT_DEFINE_GLOBALS
#  define MAPEDIT_GLOBAL( x ) x = NULL
#else
#  define MAPEDIT_GLOBAL( x ) extern x
#endif
MAPEDIT_GLOBAL( ScriptContext* g_ScriptCtx );
MAPEDIT_GLOBAL( struct EDGUIMainFrame* g_UIFrame );
MAPEDIT_GLOBAL( SceneHandle g_EdScene );
MAPEDIT_GLOBAL( struct EdWorld* g_EdWorld );
MAPEDIT_GLOBAL( struct EDGUISurfTexPicker* g_UISurfTexPicker );
MAPEDIT_GLOBAL( struct EDGUIMeshPicker* g_UIMeshPicker );
MAPEDIT_GLOBAL( struct EDGUIPartSysPicker* g_UIPartSysPicker );
MAPEDIT_GLOBAL( struct EDGUISoundPicker* g_UISoundPicker );
MAPEDIT_GLOBAL( struct EDGUIScrFnPicker* g_UIScrFnPicker );
MAPEDIT_GLOBAL( struct EDGUILevelOpenPicker* g_UILevelOpenPicker );
MAPEDIT_GLOBAL( struct EDGUILevelSavePicker* g_UILevelSavePicker );
MAPEDIT_GLOBAL( struct EDGUIEntList* g_EdEntList );



//
// GROUPS
//

struct EDGUIPropRsrc_PickParentGroup : EDGUIPropRsrc
{
	EDGUIPropRsrc_PickParentGroup( int32_t id, struct EDGUIGroupPicker* gp, const StringView& value );
	virtual void OnReload( bool after );
	int32_t m_id;
};

struct EdGroup : EDGUILayoutRow
{
	// REFCOUNTED
	FINLINE void Acquire(){ ++m_refcount; }
	FINLINE void Release(){ --m_refcount; if( m_refcount <= 0 ) delete this; }
	int32_t m_refcount;
	// REFCOUNTED END
	
	EdGroup( struct EdGroupManager* groupMgr, int32_t id, int32_t pid, const StringView& name );
	virtual int OnEvent( EDGUIEvent* e );
	Mat4 GetMatrix();
	StringView GetPath();
	EdGroup* Clone();
	
	StringView Name(){ return m_ctlName.m_value; }
	
	template< class T > void Serialize( T& arch )
	{
		m_ctlOrigin.Serialize( arch );
		m_ctlPos.Serialize( arch );
		m_ctlAngles.Serialize( arch );
		m_ctlScaleUni.Serialize( arch );
	}
	
	EdGroupManager* m_groupMgr;
	int32_t m_id;
	int32_t m_parent_id;
	bool m_needsMtxUpdate;
	Mat4 m_mtxLocal;
	Mat4 m_mtxCombined;
	bool m_needsPathUpdate;
	String m_path;
	
	EDGUIGroup m_group;
	EDGUIPropString m_ctlName;
	EDGUIPropRsrc_PickParentGroup m_ctlParent;
	EDGUIPropVec3 m_ctlOrigin;
	EDGUIPropVec3 m_ctlPos;
	EDGUIPropVec3 m_ctlAngles;
	EDGUIPropFloat m_ctlScaleUni;
	
	EDGUIButton m_addChild;
	EDGUIButton m_deleteDisownParent;
	EDGUIButton m_deleteDisownRoot;
	EDGUIButton m_deleteRecursive;
	EDGUIButton m_recalcOrigin;
	EDGUIButton m_cloneGroup;
	EDGUIButton m_cloneGroupWithSub;
	EDGUIButton m_exportObj;
};
typedef Handle< EdGroup > EdGroupHandle;
typedef HashTable< int32_t, EdGroupHandle > EdGroupHandleMap;

struct EDGUIGroupPicker : EDGUIRsrcPicker
{
	EDGUIGroupPicker( EdGroupManager* groupMgr ) : m_groupMgr( groupMgr )
	{
		caption = "Pick a group";
		Reload();
	}
	virtual void _OnChangeZoom()
	{
		EDGUIRsrcPicker::_OnChangeZoom();
		m_itemHeight /= 4;
	}
	void Reload();
	EdGroupManager* m_groupMgr;
	int32_t m_ignoreID;
};

struct EdGroupManager : EDGUILayoutRow
{
	EdGroupManager();
	virtual int OnEvent( EDGUIEvent* e );
	void DrawGroups();
	void AddRootGroup();
	Mat4 GetMatrix( int32_t id );
	StringView GetPath( int32_t id );
	EdGroup* AddGroup( int32_t parent_id = 0, StringView name = "", int32_t id = -1 );
	EdGroup* _AddGroup( int32_t id, int32_t parent_id, StringView name );
	EdGroup* FindGroupByID( int32_t id );
	EdGroup* FindGroupByPath( StringView path );
	bool GroupHasParent( int32_t id, int32_t parent_id );
	void PrepareEditGroup( EdGroup* grp );
	void PrepareCurrentEditGroup();
	void TransferGroupsToGroup( int32_t from, int32_t to );
	void QueueDestroy( EdGroup* grp );
	void ProcessDestroyQueue();
	void MatrixInvalidate( int32_t id );
	void PathInvalidate( int32_t id );
	
	template< class T > void Serialize( T& arch )
	{
		if( arch.version >= 3 )
		{
			arch.marker( "GRPLST" );
			{
				EdGroup* grp;
				int32_t groupcount = m_groups.size();
				arch << groupcount;
				for( int32_t i = 0; i < groupcount; ++i )
				{
					arch.marker( "GROUP" );
					if( T::IsWriter )
					{
						grp = m_groups.item( i ).value;
						arch << grp->m_id;
						arch << grp->m_parent_id;
						grp->m_ctlName.Serialize( arch );
					}
					else
					{
						int32_t id = 0, pid = 0;
						String name;
						arch << id;
						arch << pid;
						arch << name;
						grp = _AddGroup( id, pid, name );
					}
					arch << *grp;
				}
			}
		}
		else
			AddRootGroup();
	}
	
	Array< EdGroupHandle > m_destroyQueue;
	EdGroupHandleMap m_groups;
	EDGUIGroupPicker m_grpPicker;
	int32_t m_lastGroupID;
	EDGUIButton m_gotoRoot;
	EDGUIPropRsrc m_editedGroup;
};



//
// BLOCKS
//

#define ED_TEXGEN_COORDS 0
#define ED_TEXGEN_STRETCH 1

struct EdSurface
{
	String texname;
	int texgenmode;
	float xoff, yoff;
	float scale, aspect;
	float angle;
	float lmquality;
	int xfit, yfit;
	
	EdSurface() :
		texgenmode( ED_TEXGEN_COORDS ),
		xoff( 0 ), yoff( 0 ),
		scale( 1 ), aspect( 1 ),
		angle( 0 ), lmquality( 1 ),
		xfit( 0 ), yfit( 0 )
	{}
	
	template< class T > void Serialize( T& arch )
	{
		arch.marker( "SURFACE" );
		arch << texname;
		arch << texgenmode;
		arch << xoff << yoff;
		arch << scale << aspect;
		arch << angle;
		arch( lmquality, arch.version >= 1, 1.0f );
		arch( xfit, arch.version >= 3, 0 );
		arch( yfit, arch.version >= 3, 0 );
		
		if( T::IsReader ) Precache();
	}
	
	TextureHandle cached_texture;
	
	void Precache()
	{
		char bfr[ 128 ];
		snprintf( bfr, sizeof(bfr), "textures/%.*s.png", (int) texname.size(), texname.data() );
		cached_texture = GR_GetTexture( bfr );
	}
};

#define EdVtx_DECL "pf3nf30f21f2"
struct EdVtx
{
	Vec3 pos;
	Vec3 dummynrm;
	float tx0, ty0;
	float tx1, ty1;
	
	bool operator == ( const EdVtx& o ) const
	{
		return pos == o.pos
			&& tx0 == o.tx0
			&& ty0 == o.ty0
			&& tx1 == o.tx1
			&& ty1 == o.ty1;
	}
};

struct EdBlock
{
	EdBlock() : selected(false), group(0), position(V3(0)), z0(0), z1(1){}
	
	bool selected;
	int group;
	Vec3 position;
	float z0, z1;
	
	Array< Vec3 > poly;
	Array< EdSurface > surfaces;
	
	MeshHandle cached_mesh;
	MeshInstHandle cached_meshinst;
	
	template< class T > void Serialize( T& arch )
	{
		arch.marker( "BLOCK" );
		if( arch.version >= 3 )
		{
			arch << group;
			arch << position;
		}
		else
		{
			group = 0;
			Vec2 pos = { position.x, position.y };
			arch << pos;
			position = V3( pos.x, pos.y, 0 );
		}
		arch << z0 << z1;
		arch << poly;
		arch << surfaces;
		
		if( T::IsReader ) RegenerateMesh();
	}
	
	void _GetTexVecs( int surf, Vec3& tgx, Vec3& tgy );
	uint16_t _AddVtx( const Vec3& vpos, float z, const EdSurface& S, const Vec3& tgx, const Vec3& tgy, Array< EdVtx >& vertices, uint16_t voff );
	void _PostFitTexcoords( const EdSurface& S, EdVtx* vertices, size_t vcount );
	
	void GenCenterPos( EDGUISnapProps& SP );
	Vec3 FindCenter();
	
	bool RayIntersect( const Vec3& rpos, const Vec3& dir, float outdst[1], int* outsurf = NULL );
	
	void RegenerateMesh();
	
	LevelCache::Vertex _MakeGenVtx( const Vec3& vpos, float z, const EdSurface& S, const Vec3& tgx, const Vec3& tgy );
	void GenerateMesh( LevelCache& LC );
	void Export( OBJExporter& objex );
};



#define perp_prod(u,v) ((u).x * (v).y - (u).y * (v).x)
inline int intersect_lines( const Vec2& l1a, const Vec2& l1b, const Vec2& l2a, const Vec2& l2b, Vec2* out )
{
	Vec2 u = l1b - l1a;
	Vec2 v = l2b - l2a;
	Vec2 w = l1a - l2a;
	float D = perp_prod( u, v );
	
	if( fabs( D ) < SMALL_FLOAT )
		return 0;
	
	float sI = perp_prod( v, w ) / D;
	*out = l1a + sI * u;
	return 1;
}


struct EDGUIVertexProps : EDGUILayoutRow
{
	EDGUIVertexProps();
	void Prepare( EdBlock& B, int vid );
	virtual int OnEvent( EDGUIEvent* e );
	
	EdBlock* m_out;
	int m_vid;
	EDGUIGroup m_group;
	EDGUIPropVec3 m_pos;
	EDGUIButton m_insbef;
	EDGUIButton m_insaft;
};


struct EDGUISurfaceProps : EDGUILayoutRow
{
	EDGUISurfaceProps();
	void Prepare( EdBlock& B, int sid );
	void LoadParams( EdSurface& S, const char* name = "Surface" );
	void BounceBack( EdSurface& S );
	virtual int OnEvent( EDGUIEvent* e );
	
	EdBlock* m_out;
	int m_sid;
	EDGUIGroup m_group;
	EDGUIPropRsrc m_tex;
	EDGUIPropVec2 m_off;
	EDGUIPropVec2 m_scaleasp;
	EDGUIPropFloat m_angle;
	EDGUIPropFloat m_lmquality;
	EDGUIPropInt m_xfit;
	EDGUIPropInt m_yfit;
};

struct EDGUIBlockProps : EDGUILayoutRow
{
	EDGUIBlockProps();
	void Prepare( EdBlock& B );
	virtual int OnEvent( EDGUIEvent* e );
	
	EdBlock* m_out;
	EDGUIGroup m_group;
	EDGUIGroup m_vertGroup;
	EDGUIPropFloat m_z0;
	EDGUIPropFloat m_z1;
	EDGUIPropVec3 m_pos;
	EDGUIPropRsrc m_blkGroup;
	Array< EDGUIPropVec3 > m_vertProps;
	Array< EDGUISurfaceProps > m_surfProps;
};


//
// ENTITIES
//

typedef SerializeVersionHelper<TextReader> SVHTR;
typedef SerializeVersionHelper<TextWriter> SVHTW;

struct EdEntity : EDGUILayoutRow
{
	FINLINE void Acquire(){ ++m_refcount; }
	FINLINE void Release(){ --m_refcount; if( m_refcount <= 0 ) delete this; }
	int32_t m_refcount;
	
	EdEntity( bool isproto ) :
		m_refcount( 0 ),
		m_isproto( isproto ),
		m_group( true, "Entity properties" ),
		m_ctlPos( V3(0), 2, V3(-8192), V3(8192) )
	{
		tyname = "_entity_overrideme_";
		
		m_ctlPos.caption = "Position";
	}
	
	void LoadIcon();
	
	const Vec3& Pos() const { return m_ctlPos.m_value; }
	void SetPosition( const Vec3& pos ){ m_ctlPos.SetValue( pos ); }
	
	virtual int OnEvent( EDGUIEvent* e ){ return EDGUILayoutRow::OnEvent( e ); }
	
	virtual void Serialize( SVHTR& arch ) = 0;
	virtual void Serialize( SVHTW& arch ) = 0;
	virtual void UpdateCache( LevelCache& LC ){}
	
	virtual EdEntity* Clone() = 0;
	virtual bool RayIntersect( const Vec3& rpos, const Vec3& rdir, float outdst[1] )
	{
		return RaySphereIntersect( rpos, rdir, Pos(), 0.2f, outdst );
	}
	virtual void RegenerateMesh(){}
	virtual void DebugDraw(){}
	
	bool m_isproto;
	EDGUIGroup m_group;
	EDGUIPropVec3 m_ctlPos;
	TextureHandle m_iconTex;
};

typedef Handle< EdEntity > EdEntityHandle;

struct EDGUIEntButton : EDGUIButton
{
	EDGUIEntButton()
	{
		tyname = "entity-button";
	}
	
	EdEntityHandle m_ent_handle;
};



/////////////
////////////
///////////

struct EdEntMesh : EdEntity
{
	EdEntMesh( bool isproto = true );
	
	const String& Mesh() const { return m_ctlMesh.m_value; }
	const Vec3& RotAngles() const { return m_ctlAngles.m_value; }
	float ScaleUni() const { return m_ctlScaleUni.m_value; }
	const Vec3& ScaleSep() const { return m_ctlScaleSep.m_value; }
	Mat4 Matrix() const { return Mat4::CreateSRT( m_ctlScaleSep.m_value * m_ctlScaleUni.m_value, DEG2RAD( m_ctlAngles.m_value ), m_ctlPos.m_value ); }
	
	EdEntMesh& operator = ( const EdEntMesh& o );
	virtual EdEntity* Clone();
	
	template< class T > void SerializeT( T& arch )
	{
		arch << m_ctlAngles;
		arch << m_ctlScaleUni;
		arch << m_ctlScaleSep;
		arch << m_ctlMesh;
	}
	virtual void Serialize( SVHTR& arch ){ SerializeT( arch ); }
	virtual void Serialize( SVHTW& arch ){ SerializeT( arch ); }
	
	virtual void UpdateCache( LevelCache& LC );
	virtual int OnEvent( EDGUIEvent* e );
	virtual void DebugDraw();
	virtual void RegenerateMesh();
	
	EDGUIPropVec3 m_ctlAngles;
	EDGUIPropFloat m_ctlScaleUni;
	EDGUIPropVec3 m_ctlScaleSep;
	EDGUIPropRsrc m_ctlMesh;
	
	MeshHandle cached_mesh;
	MeshInstHandle cached_meshinst;
};

struct EdEntLight : EdEntity
{
	EdEntLight( bool isproto = true );
	
	float Range() const { return m_ctlRange.m_value; }
	float Power() const { return m_ctlPower.m_value; }
	const Vec3& ColorHSV() const { return m_ctlColorHSV.m_value; }
	float LightRadius() const { return m_ctlLightRadius.m_value; }
	int ShadowSampleCount() const { return m_ctlShSampleCnt.m_value; }
	float FlareSize() const { return m_ctlFlareSize.m_value; }
	Vec3 FlareOffset() const { return m_ctlFlareOffset.m_value; }
	bool IsSpotlight() const { return m_ctlIsSpotlight.m_value; }
	const Vec3& SpotRotation() const { return m_ctlSpotRotation.m_value; }
	float SpotInnerAngle() const { return m_ctlSpotInnerAngle.m_value; }
	float SpotOuterAngle() const { return m_ctlSpotOuterAngle.m_value; }
	float SpotCurve() const { return m_ctlSpotCurve.m_value; }
	Mat4 SpotMatrix() const { return Mat4::CreateRotationXYZ( DEG2RAD( SpotRotation() ) ); }
	Vec3 SpotDir() const { return SpotMatrix().TransformNormal( V3(0,0,-1) ).Normalized(); }
	Vec3 SpotUp() const { return SpotMatrix().TransformNormal( V3(0,-1,0) ).Normalized(); }
	
	EdEntLight& operator = ( const EdEntLight& o );
	virtual EdEntity* Clone();
	
	template< class T > void SerializeT( T& arch )
	{
		arch << m_ctlRange;
		arch << m_ctlPower;
		arch << m_ctlColorHSV;
		arch << m_ctlLightRadius;
		arch << m_ctlShSampleCnt;
		arch << m_ctlFlareSize;
		if( arch.version >= 2 )
			m_ctlFlareOffset.Serialize( arch );
		arch << m_ctlIsSpotlight;
		arch << m_ctlSpotRotation;
		arch << m_ctlSpotInnerAngle;
		arch << m_ctlSpotOuterAngle;
		arch << m_ctlSpotCurve;
	}
	virtual void Serialize( SVHTR& arch ){ SerializeT( arch ); }
	virtual void Serialize( SVHTW& arch ){ SerializeT( arch ); }
	
	virtual void DebugDraw();
	virtual void UpdateCache( LevelCache& LC );
	
	EDGUIPropFloat m_ctlRange;
	EDGUIPropFloat m_ctlPower;
	EDGUIPropVec3 m_ctlColorHSV;
	EDGUIPropFloat m_ctlLightRadius;
	EDGUIPropInt m_ctlShSampleCnt;
	EDGUIPropFloat m_ctlFlareSize;
	EDGUIPropVec3 m_ctlFlareOffset;
	EDGUIPropBool m_ctlIsSpotlight;
	EDGUIPropVec3 m_ctlSpotRotation;
	EDGUIPropFloat m_ctlSpotInnerAngle;
	EDGUIPropFloat m_ctlSpotOuterAngle;
	EDGUIPropFloat m_ctlSpotCurve;
};

struct EdEntLightSample : EdEntity
{
	EdEntLightSample( bool isproto = true );
	EdEntLightSample& operator = ( const EdEntLightSample& o );
	virtual EdEntity* Clone();
	
	virtual void Serialize( SVHTR& arch ){}
	virtual void Serialize( SVHTW& arch ){}
	
	virtual void UpdateCache( LevelCache& LC );
};

struct EdEntScripted : EdEntity
{
	struct Field
	{
		String key;
		EDGUIProperty* property;
	};
	
	EdEntScripted( const char* enttype, bool isproto = true );
	~EdEntScripted();
	
	EdEntScripted& operator = ( const EdEntScripted& o );
	virtual EdEntity* Clone();
	
	void Data2Fields();
	void Fields2Data();
	
	virtual void Serialize( SVHTR& arch );
	virtual void Serialize( SVHTW& arch );
	
	virtual void UpdateCache( LevelCache& LC );
	
	virtual void RegenerateMesh();
	
	virtual int OnEvent( EDGUIEvent* e );
	virtual void DebugDraw();
	
	void AddFieldBool( sgsString key, sgsString name, bool def );
	void AddFieldInt( sgsString key, sgsString name, int32_t def = 0, int32_t min = (int32_t) 0x80000000, int32_t max = (int32_t) 0x7fffffff );
	void AddFieldFloat( sgsString key, sgsString name, float def = 0, int prec = 2, float min = -FLT_MAX, float max = FLT_MAX );
	void AddFieldVec2( sgsString key, sgsString name, Vec2 def = V2(0), int prec = 2, Vec2 min = V2(-FLT_MAX), Vec2 max = V2(FLT_MAX) );
	void AddFieldVec3( sgsString key, sgsString name, Vec3 def = V3(0), int prec = 2, Vec3 min = V3(-FLT_MAX), Vec3 max = V3(FLT_MAX) );
	void AddFieldString( sgsString key, sgsString name, sgsString def );
	void AddFieldRsrc( sgsString key, sgsString name, EDGUIRsrcPicker* rsrcPicker, sgsString def );
	void SetMesh( sgsString name );
	void SetMeshInstanceCount( int count );
	void SetMeshInstanceMatrix( int which, const Mat4& mtx );
	void GetMeshAABB( Vec3 out[2] );
	
	char m_typename[ 64 ];
	sgsVariable m_data;
	sgsVariable onChange;
	sgsVariable onDebugDraw;
	sgsVariable onGather;
	Array< Field > m_fields;
	
	LevelCache* m_levelCache;
	
	MeshHandle cached_mesh;
	Array< MeshInstHandle > cached_meshinsts;
};



/////////////
////////////
///////////

extern sgs_RegFuncConst g_ent_scripted_rfc[];

struct EDGUIEntList : EDGUIGroup
{
	struct Decl
	{
		const char* name;
		EdEntity* ent;
	};
	
	EDGUIEntList();
	~EDGUIEntList();
	virtual int OnEvent( EDGUIEvent* e );
	
	EDGUIEntButton* m_buttons;
	int m_button_count;
};

EdEntity* ENT_FindProtoByName( const char* name );

template< class T > void ENT_Serialize( T& arch, EdEntity* e )
{
	String ty = e->tyname;
	
	arch.marker( "ENTITY" );
	arch << ty;
	arch << e->m_ctlPos;
	arch << *e;
}

template< class T > EdEntity* ENT_Unserialize( T& arch )
{
	String ty;
	Vec3 p;
	
	arch.marker( "ENTITY" );
	arch << ty;
	arch << p;
	
	EdEntity* e = ENT_FindProtoByName( StackString< 128 >( ty ) );
	if( !e )
	{
		LOG_ERROR << "FAILED TO FIND ENTITY: " << ty;
		return NULL;
	}
	e = e->Clone();
	e->m_ctlPos.SetValue( p );
	e->Serialize( arch );
	e->RegenerateMesh();
	
	return e;
}


//
// WORLD
//

struct EdWorld : EDGUILayoutRow
{
	EdWorld();
	
	template< class T > void Serialize( T& arch )
	{
		arch.marker( "WORLD" );
		SerializeVersionHelper<T> svh( arch, MAP_FILE_VERSION );
		
		svh << m_ctlAmbientColor;
		svh << m_ctlDirLightDir;
		svh << m_ctlDirLightColor;
		svh << m_ctlDirLightDivergence;
		svh << m_ctlDirLightNumSamples;
		svh << m_ctlLightmapClearColor;
		svh << m_ctlRADNumBounces;
		svh << m_ctlLightmapDetail;
		svh << m_ctlLightmapBlurSize;
		svh << m_ctlAODistance;
		svh << m_ctlAOMultiplier;
		svh << m_ctlAOFalloff;
		svh << m_ctlAOEffect;
	//	svh << m_ctlAODivergence;
		svh << m_ctlAOColor;
		svh << m_ctlAONumSamples;
		
		svh << m_blocks;
		
		if( T::IsWriter )
		{
			int32_t numents = m_entities.size();
			svh << numents;
			for( size_t i = 0; i < m_entities.size(); ++i )
			{
				ENT_Serialize( svh, m_entities[ i ] );
			}
		}
		else
		{
			int32_t numents;
			svh << numents;
			m_entities.clear();
			for( int32_t i = 0; i < numents; ++i )
			{
				EdEntity* e = ENT_Unserialize( svh );
				if( e )
					m_entities.push_back( e );
			}
		}
	}
	
	void Reset();
	void TestData();
	void RegenerateMeshes();
	void DrawWires_Blocks( int hlblock, int selblock );
	void DrawPoly_BlockSurf( int block, int surf, bool sel );
	void DrawPoly_BlockVertex( int block, int vert, bool sel );
	void DrawWires_Entities( int hlmesh, int selmesh );
	bool RayBlocksIntersect( const Vec3& pos, const Vec3& dir, int searchfrom, float outdst[1], int outblock[1] );
	bool RayEntitiesIntersect( const Vec3& pos, const Vec3& dir, int searchfrom, float outdst[1], int outent[1] );
	
	void DeleteSelectedBlocks();
	// returns if there were any selected blocks
	bool DuplicateSelectedBlocksAndMoveSelection();
	int GetNumSelectedBlocks();
	int GetOnlySelectedBlock();
	void SelectBlock( int block, bool mod );
	
	Vec3 FindCenterOfGroup( int32_t grp );
	void FixTransformsOfGroup( int32_t grp );
	void CopyObjectsToGroup( int32_t grpfrom, int32_t grpto );
	void TransferObjectsToGroup( int32_t grpfrom, int32_t grpto );
	void DeleteObjectsInGroup( int32_t grp );
	void ExportGroupAsOBJ( int32_t grp, const StringView& name );
	
	EDGUIItem* GetBlockProps( size_t bid )
	{
		m_ctlBlockProps.Prepare( m_blocks[ bid ] );
		return &m_ctlBlockProps;
	}
	EDGUIItem* GetVertProps( size_t bid, size_t vid )
	{
		m_ctlVertProps.Prepare( m_blocks[ bid ], vid );
		return &m_ctlVertProps;
	}
	EDGUIItem* GetSurfProps( size_t bid, size_t sid )
	{
		m_ctlSurfProps.Prepare( m_blocks[ bid ], sid );
		return &m_ctlSurfProps;
	}
	EDGUIItem* GetEntityProps( size_t mid )
	{
		return m_entities[ mid ];
	}
	
	VertexDeclHandle m_vd;
	
	Array< EdBlock > m_blocks;
	Array< EdEntityHandle > m_entities;
	EdGroupManager m_groupMgr;
	
	EDGUIGroup m_ctlGroup;
	EDGUIPropVec3 m_ctlAmbientColor;
	EDGUIPropVec2 m_ctlDirLightDir;
	EDGUIPropVec3 m_ctlDirLightColor;
	EDGUIPropFloat m_ctlDirLightDivergence;
	EDGUIPropInt m_ctlDirLightNumSamples;
	EDGUIPropVec3 m_ctlLightmapClearColor;
	EDGUIPropInt m_ctlRADNumBounces;
	EDGUIPropFloat m_ctlLightmapDetail;
	EDGUIPropFloat m_ctlLightmapBlurSize;
	EDGUIPropFloat m_ctlAODistance;
	EDGUIPropFloat m_ctlAOMultiplier;
	EDGUIPropFloat m_ctlAOFalloff;
	EDGUIPropFloat m_ctlAOEffect;
//	EDGUIPropFloat m_ctlAODivergence;
	EDGUIPropVec3 m_ctlAOColor;
	EDGUIPropInt m_ctlAONumSamples;
	
	EDGUIBlockProps m_ctlBlockProps;
	EDGUIVertexProps m_ctlVertProps;
	EDGUISurfaceProps m_ctlSurfProps;
};


struct EdEditTransform
{
	virtual bool OnEnter()
	{
		m_startCursorPos = GetCursorPos();
		SaveState();
		RecalcTransform();
		ApplyTransform();
		return true;
	}
	virtual void OnExit(){}
	virtual int OnViewEvent( EDGUIEvent* e ){ return 0; }
	virtual void Draw(){}
	
	virtual void SaveState(){}
	virtual void RestoreState(){}
	virtual void ApplyTransform(){}
	virtual void RecalcTransform(){}
	
	// projection space without Z
	Vec2 GetCursorPos();
	Vec2 GetScreenPos( const Vec3& p );
	Vec2 GetScreenDir( const Vec3& d );
	Vec2 MovePointOnLine( const Vec2& p, const Vec2& lo, const Vec2& ld );
	
	Vec2 m_startCursorPos;
};

struct EdBasicEditTransform : EdEditTransform
{
	virtual int OnViewEvent( EDGUIEvent* e );
};

struct EdBlockEditTransform : EdBasicEditTransform
{
	enum ConstraintMode
	{
		Camera,
		XAxis,
		XPlane,
		YAxis,
		YPlane,
		ZAxis,
		ZPlane,
	};
	struct SavedBlock
	{
		int id;
		EdBlock data;
	};
	
	virtual bool OnEnter();
	virtual int OnViewEvent( EDGUIEvent* e );
	virtual void SaveState();
	virtual void RestoreState();
	Vec3 GetMovementVector( const Vec2& a, const Vec2& b );
	
	Array< SavedBlock > m_blocks;
	ConstraintMode m_cmode;
	Vec3 m_origin;
};

struct EdBlockMoveTransform : EdBlockEditTransform
{
	virtual int OnViewEvent( EDGUIEvent* e );
	virtual void Draw();
	virtual void ApplyTransform();
	virtual void RecalcTransform();
	
	Vec3 m_transform;
};


struct EdEditMode
{
	virtual void OnEnter(){}
	virtual void OnExit(){}
	virtual int OnUIEvent( EDGUIEvent* e ){ return 0; }
	virtual void OnViewEvent( EDGUIEvent* e ){}
	virtual void Draw(){}
};

struct EdDrawBlockEditMode : EdEditMode
{
	enum ED_BlockDrawMode
	{
		BD_Polygon = 1,
		BD_BoxStrip = 2,
	};

	EdDrawBlockEditMode();
	void OnEnter();
	int OnUIEvent( EDGUIEvent* e );
	void OnViewEvent( EDGUIEvent* e );
	void Draw();
	void _AddNewBlock();
	
	ED_BlockDrawMode m_blockDrawMode;
	Array< Vec2 > m_drawnVerts;
	EDGUIPropFloat m_newBlockPropZ0;
	EDGUIPropFloat m_newBlockPropZ1;
	EDGUISurfaceProps m_newSurfProps;
};

struct EdEditBlockEditMode : EdEditMode
{
	EdEditBlockEditMode();
	void OnEnter();
	void OnViewEvent( EDGUIEvent* e );
	void Draw();
	void _ReloadBlockProps();
	
	int m_hlBlock;
	int m_selBlock;
	int m_hlSurf;
	int m_selSurf;
	int m_hlVert;
	int m_selVert;
	bool m_dragAdjacent;
	
	Vec2 m_cpdiff;
	bool m_grabbed;
	Vec2 m_origPos;
	Vec2 m_origPos0;
	Vec2 m_origPos1;
	
	EdBlockMoveTransform m_transform;
};

struct EdEditVertexEditMode : EdEditMode
{
	void OnEnter();
	void OnViewEvent( EDGUIEvent* e );
	void Draw();
};

struct EdPaintSurfsEditMode : EdEditMode
{
	EdPaintSurfsEditMode();
	void OnEnter();
	void OnViewEvent( EDGUIEvent* e );
	void Draw();
	
	int m_paintBlock;
	int m_paintSurf;
	bool m_isPainting;
	EDGUISurfaceProps m_paintSurfProps;
};

struct EdAddEntityEditMode : EdEditMode
{
	EdAddEntityEditMode();
	void OnEnter();
	int OnUIEvent( EDGUIEvent* e );
	void OnViewEvent( EDGUIEvent* e );
	void Draw();
	void SetEntityType( const EdEntityHandle& eh );
	void _AddNewEntity();
	
	EdEntityHandle m_entityProps;
	EDGUIEntList m_entGroup;
};

struct EdEditEntityEditMode : EdEditMode
{
	EdEditEntityEditMode();
	void OnEnter();
	int OnUIEvent( EDGUIEvent* e );
	void OnViewEvent( EDGUIEvent* e );
	void Draw();
	void _ReloadEntityProps();
	
	int m_hlEnt;
	int m_selEnt;
	
	Vec2 m_cpdiff;
	bool m_grabbed;
	Vec2 m_origPos;
};

struct EdEditGroupEditMode : EdEditMode
{
	void OnEnter();
	void Draw();
};

struct EdEditLevelEditMode : EdEditMode
{
	void OnEnter();
};


//
// MAIN FRAME
//
struct EDGUIMainFrame : EDGUIFrame, EDGUIRenderView::FrameInterface
{
	EDGUIMainFrame();
	void PostInit();
	int OnEvent( EDGUIEvent* e );
	bool ViewEvent( EDGUIEvent* e );
	void _DrawCursor( bool drawimg, float height );
	void DrawCursor( bool drawimg = true );
	void DebugDraw();
	
	void AddToParamList( EDGUIItem* item );
	void ClearParamList();
	void RefreshMouse();
	Vec3 GetCursorRayPos();
	Vec3 GetCursorRayDir();
	Vec2 GetCursorPlanePos();
	float GetCursorPlaneHeight();
	void SetCursorPlaneHeight( float z );
	bool IsCursorAiming();
	void Snap( Vec2& v );
	void Snap( Vec3& v );
	Vec2 Snapped( const Vec2& v );
	Vec3 Snapped( const Vec3& v );
	
	void ResetEditorState();
	void Level_New();
	void Level_Open();
	void Level_Save();
	void Level_SaveAs();
	void Level_Compile();
	void Level_Real_Open( const String& str );
	void Level_Real_Save( const String& str );
	void Level_Real_Compile();
	void SetEditMode( EdEditMode* em );
	void SetEditTransform( EdEditTransform* et );
	void SetModeHighlight( EDGUIButton* mybtn );
	
	String m_fileName;
	
	// EDIT MODES
	EdEditTransform* m_editTF;
	EdEditMode* m_editMode;
	EdDrawBlockEditMode m_emDrawBlock;
	EdEditBlockEditMode m_emEditBlock;
	EdEditVertexEditMode m_emEditVertex;
	EdPaintSurfsEditMode m_emPaintSurfs;
	EdAddEntityEditMode m_emAddEntity;
	EdEditEntityEditMode m_emEditEntity;
	EdEditGroupEditMode m_emEditGroup;
	EdEditLevelEditMode m_emEditLevel;
	
	// extra edit data
	TextureHandle m_txMarker;
	EDGUISnapProps m_snapProps;
	int m_keyMod;
	
	// core layout
	EDGUILayoutSplitPane m_UIMenuSplit;
	EDGUILayoutSplitPane m_UIParamSplit;
	EDGUILayoutColumn m_UIMenuButtons;
	EDGUILayoutRow m_UIParamList;
	EDGUIRenderView m_UIRenderView;

	// menu
	EDGUILabel m_MB_Cat0;
	EDGUIButton m_MBNew;
	EDGUIButton m_MBOpen;
	EDGUIButton m_MBSave;
	EDGUIButton m_MBSaveAs;
	EDGUIButton m_MBCompile;
	EDGUILabel m_MB_Cat1;
	EDGUIButton m_MBDrawBlock;
	EDGUIButton m_MBEditBlock;
	EDGUIButton m_MBPaintSurfs;
	EDGUIButton m_MBAddEntity;
	EDGUIButton m_MBEditEntity;
	EDGUIButton m_MBEditGroups;
	EDGUIButton m_MBLevelInfo;
};



