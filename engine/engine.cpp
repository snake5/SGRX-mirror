

#include <stdio.h>
#include <time.h>
#ifdef __MINGW32__
#include <x86intrin.h>
#else
#include <intrin.h>
#endif
#include <windows.h>

#define USE_VEC2
#define USE_VEC3
#define USE_VEC4
#define USE_QUAT
#define USE_MAT4
#define USE_ARRAY
#define USE_HASHTABLE
#define USE_SERIALIZATION

#define INCLUDE_REAL_SDL
#include "engine_int.hpp"
#include "enganim.hpp"
#include "renderer.hpp"


uint32_t GetTimeMsec()
{
#ifdef __linux
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#else
	clock_t clk = clock();
	return clk * 1000 / CLOCKS_PER_SEC;
#endif
}


//
// GLOBALS
//

typedef HashTable< StringView, SGRX_ITexture* > TextureHashTable;
typedef HashTable< StringView, SGRX_IShader* > ShaderHashTable;
typedef HashTable< StringView, SGRX_IVertexDecl* > VertexDeclHashTable;
typedef HashTable< StringView, SGRX_IMesh* > MeshHashTable;
typedef HashTable< StringView, AnimHandle > AnimHashTable;

static String g_GameLibName = "game";

static bool g_Running = true;
static SDL_Window* g_Window = NULL;
static void* g_GameLib = NULL;
static IGame* g_Game = NULL;
static uint32_t g_GameTime = 0;
static ActionMap* g_ActionMap;
static Vec2 g_CursorPos = {0,0};
static Array< IScreen* > g_OverlayScreens;

static RenderSettings g_RenderSettings = { 1024, 576, false, false, true };
static const char* g_RendererName = "sgrx-render-d3d9";
static void* g_RenderLib = NULL;
static pfnRndInitialize g_RfnInitialize = NULL;
static pfnRndFree g_RfnFree = NULL;
static pfnRndCreateRenderer g_RfnCreateRenderer = NULL;
static IRenderer* g_Renderer = NULL;
static BatchRenderer* g_BatchRenderer = NULL;
static FontRenderer* g_FontRenderer = NULL;
static TextureHashTable* g_Textures = NULL;
static ShaderHashTable* g_Shaders = NULL;
static VertexDeclHashTable* g_VertexDecls = NULL;
static MeshHashTable* g_Meshes = NULL;
static AnimHashTable* g_Anims = NULL;


//
// LOGGING
//

SGRX_Log::SGRX_Log() : end_newline(true), need_sep(false), sep("") {}
SGRX_Log::~SGRX_Log(){ sep = ""; if( end_newline ) *this << "\n"; }

void SGRX_Log::prelog()
{
	if( need_sep )
		printf( "%s", sep );
	else
		need_sep = true;
}

SGRX_Log& SGRX_Log::operator << ( EMod_Partial ){ end_newline = false; return *this; }
SGRX_Log& SGRX_Log::operator << ( ESpec_Date )
{
	time_t ttv;
	time( &ttv );
	struct tm T = *localtime( &ttv );
	char pbuf[ 256 ] = {0};
	strftime( pbuf, 255, "%Y-%m-%d %H:%M:%S", &T );
	printf( pbuf );
	return *this;
}
SGRX_Log& SGRX_Log::operator << ( const Separator& s ){ sep = s.sep; return *this; }
SGRX_Log& SGRX_Log::operator << ( bool v ){ prelog(); printf( "[%s / %02X]", v ? "true" : "false", (int) v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( int8_t v ){ prelog(); printf( "%d", (int) v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( uint8_t v ){ prelog(); printf( "%d", (int) v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( int16_t v ){ prelog(); printf( "%d", (int) v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( uint16_t v ){ prelog(); printf( "%d", (int) v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( int32_t v ){ prelog(); printf( "%" PRId32, v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( uint32_t v ){ prelog(); printf( "%" PRIu32, v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( int64_t v ){ prelog(); printf( "%" PRId64, v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( uint64_t v ){ prelog(); printf( "%" PRIu64, v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( float v ){ return *this << (double) v; }
SGRX_Log& SGRX_Log::operator << ( double v ){ prelog(); printf( "%g", v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( const void* v ){ prelog(); printf( "[%p]", v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( const char* v ){ prelog(); printf( "%s", v ); return *this; }
SGRX_Log& SGRX_Log::operator << ( const StringView& sv ){ prelog(); printf( "[%d]\"%.*s\"", (int) sv.size(), (int) sv.size(), sv.data() ); return *this; }
SGRX_Log& SGRX_Log::operator << ( const String& sv ){ return *this << (StringView) sv; }
SGRX_Log& SGRX_Log::operator << ( const Vec2& v )
{
	prelog();
	printf( "Vec2( %g ; %g )", v.x, v.y );
	return *this;
}
SGRX_Log& SGRX_Log::operator << ( const Vec3& v )
{
	prelog();
	printf( "Vec3( %g ; %g ; %g )", v.x, v.y, v.z );
	return *this;
}
SGRX_Log& SGRX_Log::operator << ( const Quat& q )
{
	prelog();
	printf( "Quat( %g ; %g ; %g ; w = %g )", q.x, q.y, q.z, q.w );
	return *this;
}
SGRX_Log& SGRX_Log::operator << ( const Mat4& v )
{
	prelog();
	printf( "Mat4(\n" );
	printf( "\t%g\t%g\t%g\t%g\n",  v.m[0][0], v.m[0][1], v.m[0][2], v.m[0][3] );
	printf( "\t%g\t%g\t%g\t%g\n",  v.m[1][0], v.m[1][1], v.m[1][2], v.m[1][3] );
	printf( "\t%g\t%g\t%g\t%g\n",  v.m[2][0], v.m[2][1], v.m[2][2], v.m[2][3] );
	printf( "\t%g\t%g\t%g\t%g\n)", v.m[3][0], v.m[3][1], v.m[3][2], v.m[3][3] );
	return *this;
}




bool Window_HasClipboardText()
{
	return SDL_HasClipboardText();
}

bool Window_GetClipboardText( String& out )
{
	char* cbtext = SDL_GetClipboardText();
	if( !cbtext )
		return false;
	out = cbtext;
	return true;
}

bool Window_SetClipboardText( const StringView& text )
{
	return 0 == SDL_SetClipboardText( String_Concat( text, "\0" ).data() );
}



//
// GAME SYSTEMS
//

void Command::_SetState( float x )
{
	value = x;
	state = x >= threshold;
}

void Command::_Advance()
{
	prev_value = value;
	prev_state = state;
}

void Game_RegisterAction( Command* cmd )
{
	g_ActionMap->Register( cmd );
}

void Game_UnregisterAction( Command* cmd )
{
	g_ActionMap->Unregister( cmd );
}

void Game_BindKeyToAction( uint32_t key, Command* cmd )
{
	g_ActionMap->Map( ACTINPUT_MAKE( ACTINPUT_KEY, key ), cmd );
}

void Game_BindKeyToAction( uint32_t key, const StringView& cmd )
{
	g_ActionMap->Map( ACTINPUT_MAKE( ACTINPUT_KEY, key ), cmd );
}

Vec2 Game_GetCursorPos()
{
	return g_CursorPos;
}


bool Game_HasOverlayScreen( IScreen* screen )
{
	return g_OverlayScreens.has( screen );
}

void Game_AddOverlayScreen( IScreen* screen )
{
	g_OverlayScreens.push_back( screen );
	screen->OnStart();
}

void Game_RemoveOverlayScreen( IScreen* screen )
{
	if( g_OverlayScreens.has( screen ) )
	{
		screen->OnEnd();
		g_OverlayScreens.remove_all( screen );
	}
}

static void process_overlay_screens( float dt )
{
	for( size_t i = 0; i < g_OverlayScreens.size(); ++i )
	{
		IScreen* scr = g_OverlayScreens[ i ];
		if( scr->Draw( dt ) )
		{
			g_OverlayScreens.erase( i-- );
			if( !g_OverlayScreens.has( scr ) )
				scr->OnEnd();
		}
	}
}

void Game_OnEvent( const Event& e )
{
	g_Game->OnEvent( e );
	
	if( e.type == SDL_MOUSEMOTION )
	{
		g_CursorPos.x = e.motion.x;
		g_CursorPos.y = e.motion.y;
	}
	
	for( size_t i = g_OverlayScreens.size(); i > 0; )
	{
		i--;
		IScreen* screen = g_OverlayScreens[ i ];
		if( screen->OnEvent( e ) )
			return; // event inhibited
	}
	
	if( e.type == SDL_KEYDOWN || e.type == SDL_KEYUP )
	{
		Command* cmd = g_ActionMap->Get( ACTINPUT_MAKE_KEY( e.key.keysym.sym ) );
		if( cmd )
			cmd->_SetState( e.key.state );
	}
	
	if( e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP )
	{
		Command* cmd = g_ActionMap->Get( ACTINPUT_MAKE_MOUSE( e.button.button ) );
		if( cmd )
			cmd->_SetState( e.button.state );
	}
}

void Game_Process( float dt )
{
	float f[4] = { 0.2f, 0.4f, 0.6f, 1.0f };
	g_Renderer->Clear( f );
	
	g_Game->OnTick( dt, g_GameTime );
	
	process_overlay_screens( dt );
	
	g_BatchRenderer->Flush();
}


void ParseDefaultTextureFlags( const StringView& flags, uint32_t& outusageflags )
{
	if( flags.contains( ":nosrgb" ) ) outusageflags &= ~TEXFLAGS_SRGB;
	if( flags.contains( ":srgb" ) ) outusageflags |= TEXFLAGS_SRGB;
	if( flags.contains( ":wrapx" ) ) outusageflags &= ~TEXFLAGS_CLAMP_X;
	if( flags.contains( ":wrapy" ) ) outusageflags &= ~TEXFLAGS_CLAMP_Y;
	if( flags.contains( ":clampx" ) ) outusageflags |= TEXFLAGS_CLAMP_X;
	if( flags.contains( ":clampy" ) ) outusageflags |= TEXFLAGS_CLAMP_Y;
	if( flags.contains( ":nolerp" ) ) outusageflags &= ~(TEXFLAGS_LERP_X | TEXFLAGS_LERP_Y);
	if( flags.contains( ":nolerpx" ) ) outusageflags &= ~TEXFLAGS_LERP_X;
	if( flags.contains( ":nolerpy" ) ) outusageflags &= ~TEXFLAGS_LERP_Y;
	if( flags.contains( ":lerp" ) ) outusageflags |= (TEXFLAGS_LERP_X | TEXFLAGS_LERP_Y);
	if( flags.contains( ":lerpx" ) ) outusageflags |= TEXFLAGS_LERP_X;
	if( flags.contains( ":lerpy" ) ) outusageflags |= TEXFLAGS_LERP_Y;
	if( flags.contains( ":nomips" ) ) outusageflags &= ~TEXFLAGS_HASMIPS;
	if( flags.contains( ":mips" ) ) outusageflags |= TEXFLAGS_HASMIPS;
}

bool IGame::OnLoadTexture( const StringView& key, ByteArray& outdata, uint32_t& outusageflags )
{
	if( !key )
		return false;
	
	StringView path = key.until( ":" );
	
	if( !LoadBinaryFile( path, outdata ) )
		return false;
	
	outusageflags = TEXFLAGS_HASMIPS | TEXFLAGS_LERP_X | TEXFLAGS_LERP_Y;
	if( path.contains( "diff." ) )
	{
		// diffuse maps
		outusageflags |= TEXFLAGS_SRGB;
	}
	
	StringView flags = key.from( ":" );
	ParseDefaultTextureFlags( flags, outusageflags );
	
	return true;
}

bool IGame::OnLoadShader( const StringView& type, const StringView& key, String& outdata )
{
	if( !key )
		return false;
	
	if( key.part( 0, 4 ) == "mtl:" )
	{
		int i = 0;
		String prepend;
		StringView tpl, mtl, cur, it = key.part( 4 );
		while( it.size() )
		{
			i++;
			cur = it.until( ":" );
			if( i == 1 )
				mtl = cur;
			else if( i == 2 )
				tpl = cur;
			else
			{
				prepend.append( STRLIT_BUF( "#define " ) );
				prepend.append( cur.data(), cur.size() );
				prepend.append( STRLIT_BUF( "\n" ) );
			}
			it.skip( cur.size() + 1 );
		}
		
		String tpl_data, mtl_data;
		if( !OnLoadShaderFile( type, String_Concat( "tpl_", tpl ), tpl_data ) )
			return false;
		if( !OnLoadShaderFile( type, String_Concat( "mtl_", mtl ), mtl_data ) )
			return false;
		outdata = String_Concat( prepend, String_Replace( tpl_data, "__CODE__", mtl_data ) );
		return true;
	}
	return OnLoadShaderFile( type, key, outdata );
}

bool IGame::OnLoadShaderFile( const StringView& type, const StringView& path, String& outdata )
{
	String filename = "shaders_";
	filename.append( type.data(), type.size() );
	filename.push_back( '/' );
	filename.append( path.data(), path.size() );
	filename.append( STRLIT_BUF( ".shd" ) );
	
	if( !LoadTextFile( filename, outdata ) )
	{
		LOG_WARNING << "Failed to load shader file: " << filename << " (type=" << type << ", path=" << path << ")";
		return false;
	}
	return ParseShaderIncludes( type, path, outdata );
}

bool IGame::ParseShaderIncludes( const StringView& type, const StringView& path, String& outdata )
{
	return true;
}

bool IGame::OnLoadMesh( const StringView& key, ByteArray& outdata )
{
	if( !key )
		return false;
	
	StringView path = key.until( ":" );
	
	if( !LoadBinaryFile( path, outdata ) )
		return false;
	
	return true;
}


void RenderStats::Reset()
{
	numVisMeshes = 0;
	numVisPLights = 0;
	numVisSLights = 0;
	numDrawCalls = 0;
	numSDrawCalls = 0;
	numMDrawCalls = 0;
	numPDrawCalls = 0;
}


SGRX_ITexture::~SGRX_ITexture()
{
	g_Textures->unset( m_key );
}

const TextureInfo& TextureHandle::GetInfo() const
{
	static TextureInfo dummy_info = {0};
	if( !item )
		return dummy_info;
	return item->m_info;
}

bool TextureHandle::UploadRGBA8Part( void* data, int mip, int w, int h, int x, int y )
{
	if( !item )
		return false;
	
	const TextureInfo& TI = item->m_info;
	
	if( mip < 0 || mip >= TI.mipcount )
	{
		LOG_ERROR << "Cannot UploadRGBA8Part - mip count out of bounds (" << mip << "/" << TI.mipcount << ")";
		return false;
	}
	
	TextureInfo mti;
	if( !TextureInfo_GetMipInfo( &TI, mip, &mti ) )
	{
		LOG_ERROR << "Cannot UploadRGBA8Part - failed to get mip info (" << mip << ")";
		return false;
	}
	
	if( w < 0 ) w = mti.width;
	if( h < 0 ) h = mti.height;
	
	return item->UploadRGBA8Part( data, mip, x, y, w, h );
}


SGRX_IShader::~SGRX_IShader()
{
	g_Shaders->unset( m_key );
}


SGRX_IVertexDecl::~SGRX_IVertexDecl()
{
	g_VertexDecls->unset( m_text );
}

const VDeclInfo& VertexDeclHandle::GetInfo()
{
	static VDeclInfo dummy_info = {0};
	if( !item )
		return dummy_info;
	return item->m_info;
}


SGRX_IMesh::SGRX_IMesh() :
	m_dataFlags( 0 ),
	m_vertexCount( 0 ),
	m_vertexDataSize( 0 ),
	m_indexCount( 0 ),
	m_indexDataSize( 0 ),
	m_numParts( 0 ),
	m_numBones( 0 ),
	m_boundsMin( Vec3::Create( 0 ) ),
	m_boundsMax( Vec3::Create( 0 ) ),
	m_refcount( 0 )
{
}

SGRX_IMesh::~SGRX_IMesh()
{
	g_Meshes->unset( m_key );
}

bool SGRX_IMesh::SetPartData( SGRX_MeshPart* parts, int count )
{
	if( count < 0 || count > MAX_MESH_PARTS )
		return false;
	int i;
	for( i = 0; i < count; ++i )
	{
		m_parts[ i ] = parts[ i ];
		for( size_t pass_id = 0; pass_id < g_Renderer->m_renderPasses.size(); ++pass_id )
		{
			const SGRX_RenderPass& PASS = g_Renderer->m_renderPasses[ pass_id ];
			if( PASS.type != RPT_SHADOWS && PASS.type != RPT_OBJECT )
				continue;
			
			char bfr[ 1000 ] = {0};
			snprintf( bfr, sizeof(bfr), "mtl:%.*s:%.*s", SHADER_NAME_LENGTH, m_parts[ i ].shader_name, (int) PASS.shader_name.size(), PASS.shader_name.data() );
			m_parts[ i ].shaders[ pass_id ] = GR_GetShader( bfr );
			snprintf( bfr, sizeof(bfr), "mtl:%.*s:%.*s:SKIN", SHADER_NAME_LENGTH, m_parts[ i ].shader_name, (int) PASS.shader_name.size(), PASS.shader_name.data() );
			m_parts[ i ].shaders_skin[ pass_id ] = GR_GetShader( bfr );
		}
	}
	for( ; i < count; ++i )
		m_parts[ i ] = SGRX_MeshPart();
	m_numParts = count;
	return true;
}

bool SGRX_IMesh::SetBoneData( SGRX_MeshBone* bones, int count )
{
	if( count < 0 || count > MAX_MESH_BONES )
		return false;
	int i;
	for( i = 0; i < count; ++i )
		m_bones[ i ] = bones[ i ];
	for( ; i < count; ++i )
		m_bones[ i ] = SGRX_MeshBone();
	m_numBones = count;
	return RecalcBoneMatrices();
}

bool SGRX_IMesh::RecalcBoneMatrices()
{
	if( !m_numBones )
	{
		return true;
	}
	
	for( int b = 0; b < m_numBones; ++b )
	{
		if( m_bones[ b ].parent_id < -1 || m_bones[ b ].parent_id >= b )
		{
			LOG_WARNING << "RecalcBoneMatrices: each parent_id must point to a previous bone or no bone (-1) [error in bone "
				<< b << ": " << m_bones[ b ].parent_id << "]";
			return false;
		}
	}
	
	Mat4 skinOffsets[ MAX_MESH_BONES ];
	for( int b = 0; b < m_numBones; ++b )
	{
		if( m_bones[ b ].parent_id >= 0 )
			skinOffsets[ b ].Multiply( m_bones[ b ].boneOffset, skinOffsets[ m_bones[ b ].parent_id ] );
		else
			skinOffsets[ b ] = m_bones[ b ].boneOffset;
		m_bones[ b ].skinOffset = skinOffsets[ b ];
	}
	for( int b = 0; b < m_numBones; ++b )
	{
		if( !skinOffsets[ b ].InvertTo( m_bones[ b ].invSkinOffset ) )
		{
			LOG_WARNING << "RecalcBoneMatrices: failed to invert skin offset matrix #" << b;
			m_bones[ b ].invSkinOffset.SetIdentity();
		}
	}
	return true;
}

bool SGRX_IMesh::SetAABBFromVertexData( const void* data, size_t size, VertexDeclHandle vd )
{
	return GetAABBFromVertexData( vd.GetInfo(), (const char*) data, size, m_boundsMin, m_boundsMax );
}


SGRX_Log& SGRX_Log::operator << ( const SGRX_Camera& cam )
{
	*this << "CAMERA:";
	*this << "\n    position = " << cam.position;
	*this << "\n    direction = " << cam.direction;
	*this << "\n    up = " << cam.up;
	*this << "\n    angle = " << cam.angle;
	*this << "\n    aspect = " << cam.aspect;
	*this << "\n    aamix = " << cam.aamix;
	*this << "\n    znear = " << cam.znear;
	*this << "\n    zfar = " << cam.zfar;
	*this << "\n    mView = " << cam.mView;
	*this << "\n    mProj = " << cam.mProj;
	*this << "\n    mInvView = " << cam.mInvView;
	return *this;
}

void SGRX_Camera::UpdateViewMatrix()
{
	mView.LookAt( position, direction, up );
	mView.InvertTo( mInvView );
}

void SGRX_Camera::UpdateProjMatrix()
{
	mProj.Perspective( angle, aspect, aamix, znear, zfar );
}

void SGRX_Camera::UpdateMatrices()
{
	UpdateViewMatrix();
	UpdateProjMatrix();
}

Vec3 SGRX_Camera::WorldToScreen( const Vec3& pos )
{
	Vec3 P = mView.TransformPos( pos );
	P = mProj.TransformPos( P );
	P.x = P.x * 0.5f + 0.5f;
	P.y = P.y * -0.5f + 0.5f;
	return P;
}

bool SGRX_Camera::GetCursorRay( float x, float y, Vec3& pos, Vec3& dir )
{
	Vec3 tPos = { x * 2 - 1, y * -2 + 1, 0 };
	Vec3 tTgt = { x * 2 - 1, y * -2 + 1, 1 };
	
	Mat4 viewProjMatrix, inv;
	viewProjMatrix.Multiply( mView, mProj );
	if( !viewProjMatrix.InvertTo( inv ) )
		return false;
	
	tPos = inv.TransformPos( tPos );
	tTgt = inv.TransformPos( tTgt );
	Vec3 tDir = ( tTgt - tPos ).Normalized();
	
	pos = tPos;
	dir = tDir;
	return true;
}


SGRX_Light::SGRX_Light( SGRX_Scene* s ) :
	_scene( s ),
	type( LIGHT_POINT ),
	enabled( true ),
	position( Vec3::Create( 0 ) ),
	direction( Vec3::Create( 0, 1, 0 ) ),
	updir( Vec3::Create( 0, 0, 1 ) ),
	color( Vec3::Create( 1 ) ),
	range( 100 ),
	power( 2 ),
	angle( 60 ),
	aspect( 1 ),
	hasShadows( false ),
	_mibuf_begin( NULL ),
	_mibuf_end( NULL ),
	_refcount( 0 )
{
	RecalcMatrices();
}

SGRX_Light::~SGRX_Light()
{
	if( _scene )
	{
		_scene->m_lights.unset( this );
	}
}

void SGRX_Light::RecalcMatrices()
{
}


SGRX_MeshInstance::SGRX_MeshInstance( SGRX_Scene* s ) :
	_scene( s ),
	color( Vec4::Create( 1 ) ),
	enabled( true ),
	cpuskin( false ),
	_lightbuf_begin( NULL ),
	_lightbuf_end( NULL ),
	_refcount( 0 )
{
	matrix.SetIdentity();
	for( int i = 0; i < MAX_MI_CONSTANTS; ++i )
		constants[ i ] = Vec4::Create( 0 );
}

SGRX_MeshInstance::~SGRX_MeshInstance()
{
	if( _scene )
	{
		_scene->m_meshInstances.unset( this );
	}
}


SGRX_Scene::SGRX_Scene() :
	cullScene( NULL ),
	fogColor( Vec3::Create( 0.5 ) ),
	fogHeightFactor( 0 ),
	fogDensity( 0.01f ),
	fogHeightDensity( 0 ),
	fogStartHeight( 0.01f ),
	fogMinDist( 0 ),
	ambientLightColor( Vec3::Create( 0.1f ) ),
	dirLightColor( Vec3::Create( 0.8f ) ),
	dirLightDir( Vec3::Create( -1 ).Normalized() ),
	m_refcount( 0 )
{
	camera.position = Vec3::Create( 10, 10, 10 );
	camera.direction = -camera.position.Normalized();
	camera.up = Vec3::Create( 0, 0, 1 );
	camera.angle = 90;
	camera.aspect = 1;
	camera.aamix = 0.5f;
	camera.znear = 1;
	camera.zfar = 1000;
	camera.UpdateMatrices();
}

SGRX_Scene::~SGRX_Scene()
{
}

MeshInstHandle SGRX_Scene::CreateMeshInstance()
{
	SGRX_MeshInstance* mi = new SGRX_MeshInstance( this );
	m_meshInstances.set( mi, NULL );
	return mi;
}

//bool SGRX_Scene::RemoveMeshInstance( MeshInstHandle mih )
//{
//	if( !mih || mih->_scene != this )
//		return false;
//	return m_meshInstances.unset( mih );
//}

LightHandle SGRX_Scene::CreateLight()
{
	SGRX_Light* lt = new SGRX_Light( this );
	m_lights.set( lt, NULL );
	return lt;
}

//bool SGRX_Scene::RemoveLight( LightHandle lh )
//{
//	if( !lh || lh->_scene != this )
//		return false;
//	return m_lights.unset( lh );
//}


int GR_GetWidth(){ return g_RenderSettings.width; }
int GR_GetHeight(){ return g_RenderSettings.height; }


TextureHandle GR_CreateTexture( int width, int height, int format, int mips )
{
	TextureInfo ti = { 0, TEXTYPE_2D, width, height, 1, format, mips };
	SGRX_ITexture* tex = g_Renderer->CreateTexture( &ti, NULL );
	if( !tex )
	{
		// error is already printed
		return TextureHandle();
	}
	
	tex->m_refcount = 0;
	tex->m_info = ti;
	
	LOG << "Created 2D texture: " << width << "x" << height << ", format=" << format << ", mips=" << mips;
	return tex;
}

TextureHandle GR_GetTexture( const StringView& path )
{
	SGRX_ITexture* tx = g_Textures->getcopy( path );
	if( tx )
		return tx;
	
	uint32_t usageflags;
	ByteArray imgdata;
	if( !g_Game->OnLoadTexture( path, imgdata, usageflags ) )
	{
		LOG_ERROR << LOG_DATE << "  Could not find texture: " << path;
		return TextureHandle();
	}
	
	TextureData texdata;
	if( !TextureData_Load( &texdata, imgdata, path ) )
	{
		// error is already printed
		return TextureHandle();
	}
	texdata.info.flags = usageflags;
	
	SGRX_ITexture* tex = g_Renderer->CreateTexture( &texdata.info, texdata.data.data() );
	if( !tex )
	{
		// error is already printed
		return TextureHandle();
	}
	
	tex->m_info = texdata.info;
	tex->m_refcount = 0;
	tex->m_key.append( path.data(), path.size() );
	g_Textures->set( tex->m_key, tex );
	
	LOG << "Loaded texture: " << path;
	return tex;
}

TextureHandle GR_CreateRenderTexture( int width, int height, int format )
{
	TextureInfo ti = { 0, TEXTYPE_2D, width, height, 1, format, 1 };
	SGRX_ITexture* tex = g_Renderer->CreateRenderTexture( &ti );
	if( !tex )
	{
		// error is already printed
		return TextureHandle();
	}
	
	tex->m_refcount = 0;
	tex->m_info = ti;
	
	LOG << "Created renderable texture: " << width << "x" << height << ", format=" << format;
	return tex;
}


ShaderHandle GR_GetShader( const StringView& path )
{
	SGRX_IShader* shd = g_Shaders->getcopy( path );
	if( shd )
		return shd;
	
	String code;
	if( !g_Game->OnLoadShader( g_Renderer->GetInfo().shaderType, path, code ) )
	{
		LOG_ERROR << LOG_DATE << "  Could not find shader: " << path;
		return ShaderHandle();
	}
	
	if( g_Renderer->GetInfo().compileShaders )
	{
		String errors;
		ByteArray comp;
		
		if( !g_Renderer->CompileShader( code, comp, errors ) )
		{
			LOG_ERROR << LOG_DATE << "  Failed to compile shader: " << path;
			LOG << errors;
			LOG << "---";
			return ShaderHandle();
		}
		
		shd = g_Renderer->CreateShader( comp );
	}
	else
	{
		// TODO: I know...
		ByteArray bcode;
		bcode.resize( code.size() );
		memcpy( bcode.data(), code.data(), code.size() );
		shd = g_Renderer->CreateShader( bcode );
	}
	
	if( !shd )
	{
		// error already printed in renderer
		return NULL;
	}
	
	shd->m_key = path;
	shd->m_refcount = 0;
	g_Shaders->set( shd->m_key, shd );
	
	LOG << "Loaded shader: " << path;
	return shd;
}


VertexDeclHandle GR_GetVertexDecl( const StringView& vdecl )
{
	SGRX_IVertexDecl* VD = g_VertexDecls->getcopy( vdecl );
	if( VD )
		return VD;
	
	VDeclInfo vdinfo = {0};
	const char* err = VDeclInfo_Parse( &vdinfo, StackString< 64 >( vdecl ) );
	if( err )
	{
		LOG_ERROR << LOG_DATE << "  Failed to parse vertex declaration - " << err << " (" << vdecl << ")";
		return NULL;
	}
	
	VD = g_Renderer->CreateVertexDecl( vdinfo );
	if( !VD )
	{
		// error already printed in renderer
		return NULL;
	}
	
	VD->m_text = vdecl;
	VD->m_refcount = 0;
	g_VertexDecls->set( VD->m_text, VD );
	
	LOG << "Created vertex declaration: " << vdecl;
	return VD;
}


MeshHandle GR_CreateMesh()
{
	SGRX_IMesh* mesh = g_Renderer->CreateMesh();
	return mesh;
}

MeshHandle GR_GetMesh( const StringView& path )
{
	SGRX_IMesh* mesh = g_Meshes->getcopy( path );
	if( mesh )
		return mesh;
	
	ByteArray meshdata;
	if( !g_Game->OnLoadMesh( path, meshdata ) )
	{
		LOG_ERROR << LOG_DATE << "  Failed to access mesh data file - " << path;
		return NULL;
	}
	
	MeshFileData mfd;
	const char* err = MeshData_Parse( (char*) meshdata.data(), meshdata.size(), &mfd );
	if( err )
	{
		LOG_ERROR << LOG_DATE << "  Failed to parse mesh file - " << err;
		return NULL;
	}
	
	SGRX_MeshBone bones[ MAX_MESH_BONES ];
	for( int i = 0; i < mfd.numBones; ++i )
	{
		MeshFileBoneData* mfdb = &mfd.bones[ i ];
		bones[ i ].name.append( mfdb->boneName, mfdb->boneNameSize );
		bones[ i ].boneOffset = mfdb->boneOffset;
		bones[ i ].parent_id = mfdb->parent_id == 255 ? -1 : mfdb->parent_id;
	}
	
	VertexDeclHandle vdh;
	mesh = g_Renderer->CreateMesh();
	if( !mesh ||
		!( vdh = GR_GetVertexDecl( StringView( mfd.formatData, mfd.formatSize ) ) ) ||
		!mesh->SetVertexData( mfd.vertexData, mfd.vertexDataSize, vdh, ( mfd.dataFlags & MDF_TRIANGLESTRIP ) != 0 ) ||
		!mesh->SetIndexData( mfd.indexData, mfd.indexDataSize, ( mfd.dataFlags & MDF_INDEX_32 ) != 0 ) ||
		!mesh->SetBoneData( bones, mfd.numBones ) )
	{
		// error already printed
		return NULL;
	}
	
	mesh->m_dataFlags = mfd.dataFlags;
	mesh->m_boundsMin = mfd.boundsMin;
	mesh->m_boundsMax = mfd.boundsMax;
	
	SGRX_MeshPart parts[ MAX_MESH_PARTS ] = {0};
	for( int i = 0; i < mfd.numParts; ++i )
	{
		parts[ i ].vertexOffset = mfd.parts[ i ].vertexOffset;
		parts[ i ].vertexCount = mfd.parts[ i ].vertexCount;
		parts[ i ].indexOffset = mfd.parts[ i ].indexOffset;
		parts[ i ].indexCount = mfd.parts[ i ].indexCount;
		if( mfd.parts[ i ].materialStringSizes[0] >= SHADER_NAME_LENGTH )
		{
			LOG_WARNING << "Shader name for part " << i << " is too long";
		}
		else
		{
			memset( parts[ i ].shader_name, 0, sizeof( parts[ i ].shader_name ) );
			strncpy( parts[ i ].shader_name, mfd.parts[ i ].materialStrings[0], mfd.parts[ i ].materialStringSizes[0] );
		}
		for( int tid = 0; tid < mfd.parts[ i ].materialTextureCount; ++tid )
		{
			parts[ i ].textures[ tid ] = GR_GetTexture( StringView( mfd.parts[ i ].materialStrings[ tid + 1 ], mfd.parts[ i ].materialStringSizes[ tid + 1 ] ) );
		}
	}
	if( !mesh->SetPartData( parts, mfd.numParts ) )
	{
		LOG_WARNING << "Failed to set part data";
	}
	
	mesh->m_key = path;
	g_Meshes->set( mesh->m_key, mesh );
	
	LOG << "Created mesh: " << path;
	return mesh;
}


SGRX_Animation::~SGRX_Animation()
{
}

Vec3* SGRX_Animation::GetPosition( int track )
{
	return (Vec3*) &data[ track * 10 * frameCount ];
}

Quat* SGRX_Animation::GetRotation( int track )
{
	return (Quat*) &data[ track * 10 * frameCount + 3 * frameCount ];
}

Vec3* SGRX_Animation::GetScale( int track )
{
	return (Vec3*) &data[ track * 10 * frameCount + 7 * frameCount ];
}

void SGRX_Animation::GetState( int track, float framePos, Vec3& outpos, Quat& outrot, Vec3& outscl )
{
	Vec3* pos = GetPosition( track );
	Quat* rot = GetRotation( track );
	Vec3* scl = GetScale( track );
	
	if( framePos < 0 )
		framePos = 0;
	else if( framePos > frameCount )
		framePos = frameCount;
	
	int f0 = floor( framePos );
	int f1 = f0 + 1;
	if( f1 >= frameCount )
		f1 = f0;
	float q = framePos - f0;
	
	outpos = TLERP( pos[ f0 ], pos[ f1 ], q );
	outrot = TLERP( rot[ f0 ], rot[ f1 ], q );
	outscl = TLERP( scl[ f0 ], scl[ f1 ], q );
}

static SGRX_Animation* _create_animation( AnimFileParser* afp, int anim )
{
	assert( anim >= 0 && anim < (int) afp->animData.size() );
	
	const AnimFileParser::Anim& AN = afp->animData[ anim ];
	
	SGRX_Animation* nanim = new SGRX_Animation;
	nanim->_refcount = 0;
	
	nanim->name.assign( AN.name, AN.nameSize );
	nanim->frameCount = AN.frameCount;
	nanim->speed = AN.speed;
	nanim->data.resize( AN.trackCount * 10 * AN.frameCount );
	for( int t = 0; t < AN.trackCount; ++t )
	{
		const AnimFileParser::Track& TRK = afp->trackData[ AN.trackDataOff + t ];
		nanim->trackNames.push_back( StringView( TRK.name, TRK.nameSize ) );
		
		float* indata = TRK.dataPtr;
		Vec3* posdata = nanim->GetPosition( t );
		Quat* rotdata = nanim->GetRotation( t );
		Vec3* scldata = nanim->GetScale( t );
		
		for( int f = 0; f < AN.frameCount; ++f )
		{
			posdata[ f ].x = indata[ f * 10 + 0 ];
			posdata[ f ].y = indata[ f * 10 + 1 ];
			posdata[ f ].z = indata[ f * 10 + 2 ];
			rotdata[ f ].x = indata[ f * 10 + 3 ];
			rotdata[ f ].y = indata[ f * 10 + 4 ];
			rotdata[ f ].z = indata[ f * 10 + 5 ];
			rotdata[ f ].w = indata[ f * 10 + 6 ];
			scldata[ f ].x = indata[ f * 10 + 7 ];
			scldata[ f ].y = indata[ f * 10 + 8 ];
			scldata[ f ].z = indata[ f * 10 + 9 ];
		}
	}
	
	return nanim;
}

void Animator::Prepare( String* new_names, int count )
{
	if( new_names )
		names.assign( new_names, count );
	else
	{
		names.clear();
		names.resize( count );
	}
	position.clear();
	rotation.clear();
	scale.clear();
	factor.clear();
	position.resize( count );
	rotation.resize( count );
	scale.resize( count );
	factor.resize( count );
	for( int i = 0; i < count; ++i )
	{
		position[ i ] = V3(0);
		rotation[ i ] = Quat::Identity;
		scale[ i ] = V3(1);
		factor[ i ] = 1;
	}
}

bool Animator::Prepare( const MeshHandle& mesh )
{
	SGRX_IMesh* M = mesh;
	if( !M )
		return false;
	Prepare( NULL, M->m_numBones );
	if( M->m_numBones )
	{
		for( int i = 0; i < M->m_numBones; ++i )
			names[ i ] = M->m_bones[ i ].name;
	}
	return true;
}

int GR_LoadAnims( const StringView& path, const StringView& prefix )
{
	ByteArray ba;
	if( !LoadBinaryFile( path, ba ) )
	{
		LOG << "Failed to load animation file: " << path;
		return 0;
	}
	AnimFileParser afp( ba );
	if( afp.error )
	{
		LOG << "Failed to parse animation file (" << path << ") - " << afp.error;
		return 0;
	}
	
	int numanims = 0;
	for( int i = 0; i < (int) afp.animData.size(); ++i )
	{
		SGRX_Animation* anim = _create_animation( &afp, i );
		
		if( prefix )
		{
			anim->name.insert( 0, prefix.data(), prefix.size() );
		}
		
		numanims++;
		g_Anims->set( anim->name, anim );
	}
	
	LOG << "Loaded " << numanims << " animations from " << path << " with prefix " << prefix;
	
	return numanims;
}

AnimHandle GR_GetAnim( const StringView& name )
{
	return g_Anims->getcopy( name );
}

bool GR_ApplyAnimator( const Animator& anim, MeshInstHandle mih )
{
	SGRX_MeshInstance* MI = mih;
	if( !MI )
		return false;
	SGRX_IMesh* mesh = MI->mesh;
	if( !mesh )
		return false;
	size_t sz = MI->skin_matrices.size();
	if( sz != anim.position.size() )
		return false;
	if( sz != mesh->m_numBones )
		return false;
	SGRX_MeshBone* MB = mesh->m_bones;
	
	for( size_t i = 0; i < sz; ++i )
	{
		Mat4& M = MI->skin_matrices[ i ];
		M = Mat4::CreateSRT( anim.scale[ i ], anim.rotation[ i ], anim.position[ i ] ) * MB[ i ].boneOffset;
		if( MB[ i ].parent_id >= 0 )
			M = M * MI->skin_matrices[ MB[ i ].parent_id ];
	}
	for( size_t i = 0; i < sz; ++i )
	{
		Mat4& M = MI->skin_matrices[ i ];
		M = MB[ i ].invSkinOffset * M;
	}
	return true;
}


SceneHandle GR_CreateScene()
{
	SGRX_Scene* scene = new SGRX_Scene;
	
	LOG << "Created scene";
	return scene;
}

bool GR_SetRenderPasses( SGRX_RenderPass* passes, int count )
{
	g_Renderer->SetRenderPasses( passes, count );
}

void GR_RenderScene( SceneHandle sh, bool enablePostProcessing, SGRX_Viewport* viewport, SGRX_DebugDraw* debugDraw )
{
	g_BatchRenderer->Flush();
	g_BatchRenderer->m_texture = NULL;
	g_Renderer->RenderScene( sh, enablePostProcessing, viewport, debugDraw );
}

RenderStats& GR_GetRenderStats()
{
	return g_Renderer->m_stats;
}



void GR2D_SetWorldMatrix( const Mat4& mtx )
{
	g_Renderer->SetWorldMatrix( mtx );
}

void GR2D_SetViewMatrix( const Mat4& mtx )
{
	g_Renderer->SetViewMatrix( mtx );
}

bool GR2D_SetFont( const StringView& name, int pxsize )
{
	return g_FontRenderer->SetFont( name, pxsize );
}

void GR2D_SetColor( float r, float g, float b, float a )
{
	g_BatchRenderer->Col( r, g, b, a );
}

void GR2D_SetTextCursor( float x, float y )
{
	g_FontRenderer->SetCursor( x, y );
}

Vec2 GR2D_GetTextCursor()
{
	return Vec2::Create( g_FontRenderer->m_cursor_x, g_FontRenderer->m_cursor_y );
}

int GR2D_GetTextLength( const StringView& text )
{
	if( !g_FontRenderer )
		return 0;
	return g_FontRenderer->GetTextWidth( text );
}

int GR2D_DrawTextLine( const StringView& text )
{
	return g_FontRenderer->PutText( g_BatchRenderer, text );
}

int GR2D_DrawTextLine( float x, float y, const StringView& text )
{
	g_FontRenderer->SetCursor( x, y );
	return g_FontRenderer->PutText( g_BatchRenderer, text );
}

int GR2D_DrawTextLine( float x, float y, const StringView& text, int halign, int valign )
{
	if( !g_FontRenderer->m_currentFont )
		return 0;
	float length = 0;
	if( halign != 0 )
		length = g_FontRenderer->GetTextWidth( text );
	return GR2D_DrawTextLine( x - round( halign * 0.5f * length ), round( y - valign * 0.5f * g_FontRenderer->m_currentFont->key.size ), text );
}

BatchRenderer& GR2D_GetBatchRenderer()
{
	return *g_BatchRenderer;
}


//
// RENDERING
//

BatchRenderer::BatchRenderer( struct IRenderer* r ) : m_renderer( r ), m_texture( NULL ), m_primType( PT_None )
{
	m_swapRB = r->GetInfo().swapRB;
	m_proto.x = 0;
	m_proto.y = 0;
	m_proto.z = 0;
	m_proto.u = 0;
	m_proto.v = 0;
	m_proto.color = 0xffffffff;
}

BatchRenderer& BatchRenderer::AddVertices( Vertex* verts, int count )
{
	m_verts.reserve( m_verts.size() + count );
	for( int i = 0; i < count; ++i )
		AddVertex( verts[ i ] );
	return *this;
}

BatchRenderer& BatchRenderer::AddVertex( const Vertex& vert )
{
	m_verts.push_back( vert );
	return *this;
}

BatchRenderer& BatchRenderer::Colb( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
	uint32_t col;
	if( m_swapRB )
		col = COLOR_RGBA( b, g, r, a );
	else
		col = COLOR_RGBA( r, g, b, a );
	m_proto.color = col;
	return *this;
}

BatchRenderer& BatchRenderer::SetPrimitiveType( EPrimitiveType pt )
{
	if( pt != m_primType )
	{
		Flush();
		m_primType = pt;
	}
	return *this;
}

BatchRenderer& BatchRenderer::Prev( int i )
{
	if( i < 0 || i >= m_verts.size() )
		AddVertex( m_proto );
	else
		AddVertex( m_verts[ m_verts.size() - 1 - i ] );
	return *this;
}

BatchRenderer& BatchRenderer::Quad( float x0, float y0, float x1, float y1, float z )
{
	SetPrimitiveType( PT_Triangles );
	Tex( 0, 0 ); Pos( x0, y0, z );
	Tex( 1, 0 ); Pos( x1, y0, z );
	Tex( 1, 1 ); Pos( x1, y1, z );
	Prev( 0 );
	Tex( 0, 1 ); Pos( x0, y1, z );
	Prev( 4 );
	return *this;
}

BatchRenderer& BatchRenderer::TurnedBox( float x, float y, float dx, float dy, float z )
{
	float tx = -dy;
	float ty = dx;
	SetPrimitiveType( PT_Triangles );
	Tex( 0, 0 ); Pos( x - dx - tx, y - dy - ty, z );
	Tex( 1, 0 ); Pos( x - dx + tx, y - dy + ty, z );
	Tex( 1, 1 ); Pos( x + dx + tx, y + dy + ty, z );
	Prev( 0 );
	Tex( 0, 1 ); Pos( x + dx - tx, y + dy - ty, z );
	Prev( 4 );
	return *this;
}

BatchRenderer& BatchRenderer::CircleFill( float x, float y, float r, float z, int verts )
{
	if( verts < 0 )
		verts = r * M_PI * 2;
	if( verts >= 3 )
	{
		Flush();
		SetPrimitiveType( PT_TriangleFan );
		Pos( x, y, z );
		float a = 0;
		float ad = M_PI * 2.0f / verts;
		for( int i = 0; i < verts; ++i )
		{
			Pos( x + sin( a ) * r, y + cos( a ) * r, z );
			a += ad;
		}
		Prev( verts - 1 );
		Flush();
	}
	return *this;
}

BatchRenderer& BatchRenderer::CircleOutline( float x, float y, float r, float z, int verts )
{
	if( verts < 0 )
		verts = r * M_PI * 2;
	if( verts >= 3 )
	{
		Flush();
		SetPrimitiveType( PT_LineStrip );
		float a = 0;
		float ad = M_PI * 2.0f / verts;
		for( int i = 0; i < verts; ++i )
		{
			Pos( x + sin( a ) * r, y + cos( a ) * r, z );
			a += ad;
		}
		Prev( verts - 1 );
		Flush();
	}
	return *this;
}

bool BatchRenderer::CheckSetTexture( const TextureHandle& tex )
{
	if( tex != m_texture )
	{
		Flush();
		m_texture = tex;
		return true;
	}
	return false;
}

BatchRenderer& BatchRenderer::SetTexture( const TextureHandle& tex )
{
	CheckSetTexture( tex );
	return *this;
}

BatchRenderer& BatchRenderer::Flush()
{
	if( m_verts.size() )
	{
		m_renderer->DrawBatchVertices( m_verts.data(), m_verts.size(), m_primType, m_texture );
		m_verts.clear();
	}
	return *this;
}


void SGRX_DebugDraw::_OnEnd()
{
	g_BatchRenderer->Flush();
}



//
// INTERNALS
//

static bool read_config()
{
	String text;
	if( !LoadTextFile( "config.cfg", text ) )
	{
		LOG << "Failed to load config.cfg";
		return false;
	}
	
	StringView it = text;
	it = it.after_all( SPACE_CHARS );
	while( it.size() )
	{
		StringView key = it.until_any( HSPACE_CHARS );
		if( key == "#" )
		{
			it = it.after( "\n" ).after_all( SPACE_CHARS );
			continue;
		}
		
		it.skip( key.size() );
		it.after_all( HSPACE_CHARS );
		
		StringView value = it.until( "\n" );
		it.skip( value.size() );
		value.trim( SPACE_CHARS );
		
		// PARSING
		if( key == "game" )
		{
			if( value.size() )
			{
				g_GameLibName = value;
				LOG << "Game library: " << value;
			}
		}
		else if( key == "dir" )
		{
			if( value.size() )
			{
				if( !CWDSet( value ) )
					LOG_ERROR << "FAILED TO SET GAME DIRECTORY";
				LOG << "Game directory: " << value;
			}
		}
		else
		{
			LOG_WARNING << "Unknown key (" << key << " = " << value << ")";
		}
		// END PARSING
		
		it = it.after_all( SPACE_CHARS );
	}
	
	return true;
}

static int init_graphics()
{
	g_Window = SDL_CreateWindow( "SGRX Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, g_RenderSettings.width, g_RenderSettings.height, 0 );
	SDL_StartTextInput();
	
	char renderer_dll[ 65 ] = {0};
	snprintf( renderer_dll, 64, "%s.dll", g_RendererName );
	g_RenderLib = SDL_LoadObject( renderer_dll );
	if( !g_RenderLib )
	{
		LOG_ERROR << "Failed to load renderer: '" << renderer_dll << "'";
		return 101;
	}
	g_RfnInitialize = (pfnRndInitialize) SDL_LoadFunction( g_RenderLib, "Initialize" );
	g_RfnFree = (pfnRndFree) SDL_LoadFunction( g_RenderLib, "Free" );
	g_RfnCreateRenderer = (pfnRndCreateRenderer) SDL_LoadFunction( g_RenderLib, "CreateRenderer" );
	if( !g_RfnInitialize || !g_RfnFree || !g_RfnCreateRenderer )
	{
		LOG_ERROR << "Failed to load functions from renderer (" << renderer_dll << ")";
		return 102;
	}
	const char* rendername = g_RendererName;
	if( !g_RfnInitialize( &rendername ) )
	{
		LOG_ERROR << "Failed to load renderer (" << rendername << ")";
		return 103;
	}
	
	SDL_SysWMinfo sysinfo;
	SDL_VERSION( &sysinfo.version );
	if( SDL_GetWindowWMInfo( g_Window, &sysinfo ) <= 0 )
	{
		LOG_ERROR << "Failed to get window pointer: " << SDL_GetError();
		return 104;
	}
	
	g_Renderer = g_RfnCreateRenderer( g_RenderSettings, sysinfo.info.win.window );
	if( !g_Renderer )
	{
		LOG_ERROR << "Failed to create renderer (" << rendername << ")";
		return 105;
	}
	LOG << LOG_DATE << "  Loaded renderer: " << rendername;
	
	g_Textures = new TextureHashTable();
	g_Shaders = new ShaderHashTable();
	g_VertexDecls = new VertexDeclHashTable();
	g_Meshes = new MeshHashTable();
	g_Anims = new AnimHashTable();
	LOG << LOG_DATE << "  Created renderer resource caches";
	
	g_BatchRenderer = new BatchRenderer( g_Renderer );
	LOG << LOG_DATE << "  Created batch renderer";
	
	InitializeFontRendering();
	g_FontRenderer = new FontRenderer();
	LOG << LOG_DATE << "  Created font renderer";
	
	g_Renderer->LoadInternalResources();
	LOG << LOG_DATE << "  Loaded internal renderer resources";
	
	return 0;
}

static void free_graphics()
{
	g_Renderer->UnloadInternalResources();
	
	delete g_FontRenderer;
	g_FontRenderer = NULL;
	
	delete g_BatchRenderer;
	g_BatchRenderer = NULL;
	
	delete g_Anims;
	g_Anims = NULL;
	
	delete g_Meshes;
	g_Meshes = NULL;
	
	delete g_VertexDecls;
	g_VertexDecls = NULL;
	
	delete g_Shaders;
	g_Shaders = NULL;
	
	delete g_Textures;
	g_Textures = NULL;
	
	g_Renderer->Destroy();
	g_Renderer = NULL;
	
	g_RfnFree();
	
	SDL_UnloadObject( g_RenderLib );
	g_RenderLib = NULL;
	
	SDL_DestroyWindow( g_Window );
	g_Window = NULL;
}


typedef IGame* (*pfnCreateGame) ();

int SGRX_EntryPoint( int argc, char** argv, int debug )
{
#if 1
	LOG << "Engine self-test...";
	int ret = TestSystems();
	if( ret )
	{
		LOG << "Test FAILED with code: " << ret;
		return 0;
	}
	LOG << "Test completed successfully.";
#endif
	
	LOG << LOG_DATE << "  Engine started";
	
	if( !read_config() )
		return 4;
	
	/* initialize SDL */
	if( SDL_Init(
		SDL_INIT_TIMER | SDL_INIT_VIDEO |
		SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC |
		SDL_INIT_GAMECONTROLLER |
		SDL_INIT_EVENTS | SDL_INIT_NOPARACHUTE
	) < 0 )
	{
		LOG_ERROR << "Couldn't initialize SDL: " << SDL_GetError();
		return 5;
	}
	
	g_ActionMap = new ActionMap;
	
	g_GameLibName.append( STRLIT_BUF( ".dll" ) );
	
	g_GameLib = SDL_LoadObject( StackPath( g_GameLibName ) );
	if( !g_GameLib )
	{
		LOG_ERROR << "Failed to load " << g_GameLibName;
		return 6;
	}
	pfnCreateGame cgproc = (pfnCreateGame) SDL_LoadFunction( g_GameLib, "CreateGame" );
	if( !cgproc )
	{
		LOG_ERROR << "Failed to find entry point";
		return 7;
	}
	g_Game = cgproc();
	if( !g_Game )
	{
		LOG_ERROR << "Failed to create the game";
		return 8;
	}
	
	g_Game->OnConfigure( argc, argv );
	
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	
	if( init_graphics() )
		return 16;
	
	g_Game->OnInitialize();
	
	uint32_t prevtime = GetTimeMsec();
	SDL_Event event;
	while( g_Running )
	{
		while( SDL_PollEvent( &event ) )
		{
			if( event.type == SDL_QUIT )
			{
				g_Running = false;
				break;
			}
			
			Game_OnEvent( event );
		}
		
		uint32_t curtime = GetTimeMsec();
		uint32_t dt = curtime - prevtime;
		prevtime = curtime;
		g_GameTime += dt;
		float fdt = dt / 1000.0f;
		
		Game_Process( fdt );
		
		g_Renderer->Swap();
	}
	
	g_Game->OnDestroy();
	while( g_OverlayScreens.size() )
	{
		Game_RemoveOverlayScreen( g_OverlayScreens.last() );
	}
	
	free_graphics();
	
	SDL_UnloadObject( g_GameLib );
	
	delete g_ActionMap;
	
	LOG << LOG_DATE << "  Engine finished";
	return 0;
}

