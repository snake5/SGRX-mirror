

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <d3d9.h>
#include "../ext/d3dx/d3dx9.h"

#define USE_VEC3
#define USE_VEC4
#define USE_MAT4
#define USE_ARRAY
#define USE_HASHTABLE
#define USE_SERIALIZATION
#include "renderer.hpp"


#define SAFE_RELEASE( x ) if( x ){ (x)->Release(); x = NULL; }


static IDirect3D9* g_D3D = NULL;


static D3DFORMAT texfmt2d3d( int fmt )
{
	switch( fmt )
	{
	case TEXFORMAT_BGRX8: return D3DFMT_X8R8G8B8;
	case TEXFORMAT_BGRA8:
	case TEXFORMAT_RGBA8: return D3DFMT_A8R8G8B8;
	case TEXFORMAT_R5G6B5: return D3DFMT_R5G6B5;
	
	case TEXFORMAT_DXT1: return D3DFMT_DXT1;
	case TEXFORMAT_DXT3: return D3DFMT_DXT3;
	case TEXFORMAT_DXT5: return D3DFMT_DXT5;
	}
	return (D3DFORMAT) 0;
}

static void swap4b2ms( uint32_t* data, int size, int mask1, int shift1R, int mask2, int shift2L )
{
	int i;
	for( i = 0; i < size; ++i )
	{
		uint32_t O = data[i];
		uint32_t N = ( O & ~( mask1 | mask2 ) ) | ( ( O & mask1 ) >> shift1R ) | ( ( O & mask2 ) << shift2L );
		data[i] = N;
	}
}

static void texdatacopy( D3DLOCKED_RECT* plr, TextureInfo* texinfo, void* data, int side, int mip )
{
	int ret;
	uint8_t *src, *dst;
	size_t i, off, copyrowsize = 0, copyrowcount = 0;
	TextureInfo mipTI;
	
	off = TextureData_GetMipDataOffset( texinfo, data, side, mip );
	ret = TextureInfo_GetMipInfo( texinfo, mip, &mipTI );
	ASSERT( ret );
	
//	printf( "read side=%d mip=%d at %d\n", side, mip, off );
	
	src = ((uint8_t*)data) + off;
	dst = (uint8_t*)plr->pBits;
	TextureInfo_GetCopyDims( &mipTI, &copyrowsize, &copyrowcount );
	
	for( i = 0; i < copyrowcount; ++i )
	{
		memcpy( dst, src, copyrowsize );
		if( texinfo->format == TEXFORMAT_RGBA8 )
			swap4b2ms( (uint32_t*) dst, copyrowsize / 4, 0xff0000, 16, 0xff, 16 );
		src += copyrowsize;
		dst += plr->Pitch;
	}
}

static void _ss_reset_states( IDirect3DDevice9* dev, int w, int h )
{
	dev->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	dev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE );
	dev->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	dev->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	dev->SetRenderState( D3DRS_LIGHTING, 0 );
	dev->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	dev->SetRenderState( D3DRS_ZENABLE, 0 );
	dev->SetRenderState( D3DRS_ALPHABLENDENABLE, 1 );
	dev->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, 1 );
	dev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	{
		float wm[ 16 ] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
		float vm[ 16 ] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 100,  0, 0, 0, 1 };
		float pm[ 16 ] = { 2.0f/(float)w, 0, 0, 0,  0, 2.0f/(float)h, 0, 0,  0, 0, 1.0f/999.0f, 1.0f/-999.0f,  0, 0, 0, 1 };
		dev->SetTransform( D3DTS_WORLD, (D3DMATRIX*) wm );
		dev->SetTransform( D3DTS_VIEW, (D3DMATRIX*) vm );
		dev->SetTransform( D3DTS_PROJECTION, (D3DMATRIX*) pm );
	}
}


struct D3D9Texture : SGRX_ITexture
{
	union
	{
		IDirect3DBaseTexture9* base;
		IDirect3DTexture9* tex2d;
		IDirect3DCubeTexture9* cube;
		IDirect3DVolumeTexture9* vol;
	}
	m_ptr;
	struct D3D9Renderer* m_renderer;
	
	D3D9Texture( bool isRenderTexture = false )
	{
		m_isRenderTexture = isRenderTexture;
	}
	~D3D9Texture();
	
	bool UploadRGBA8Part( void* data, int mip, int x, int y, int w, int h )
	{
		RECT rct = { x, y, x + w, y + h };
		D3DLOCKED_RECT lr;
		HRESULT hr = m_ptr.tex2d->LockRect( mip, &lr, &rct, D3DLOCK_DISCARD );
		if( FAILED( hr ) )
		{
			LOG_ERROR << "failed to lock D3D9 texture";
			return false;
		}
		
		for( int j = 0; j < h; ++j )
		{
			uint8_t* dst = (uint8_t*)lr.pBits + lr.Pitch * j;
			memcpy( dst, ((uint32_t*)data) + w * j, w * 4 );
			if( m_info.format == TEXFORMAT_RGBA8 )
				swap4b2ms( (uint32_t*) dst, w, 0xff0000, 16, 0xff, 16 );
		}
		
		hr = m_ptr.tex2d->UnlockRect( mip );
		if( FAILED( hr ) )
		{
			LOG_ERROR << "failed to unlock D3D9 texture";
			return false;
		}
		
		return true;
	}
	
	bool OnDeviceLost(){ return true; } // TODO
	bool OnDeviceReset(){ return true; }
};

struct D3D9RenderTexture : D3D9Texture
{
	// color texture (CT) = m_ptr.tex2d
	IDirect3DSurface9* CS; /* color (output 0) surface */
	IDirect3DTexture9* DT; /* depth (output 2) texture (not used for shadowmaps, such data in CT/CS already) */
	IDirect3DSurface9* DS; /* depth (output 2) surface (not used for shadowmaps, such data in CT/CS already) */
	IDirect3DSurface9* DSS; /* depth/stencil surface */
	int format;
	
	D3D9RenderTexture( bool isRenderTexture = false )
	{
		m_isRenderTexture = isRenderTexture;
	}
	~D3D9RenderTexture()
	{
		SAFE_RELEASE( CS );
		SAFE_RELEASE( DT );
		SAFE_RELEASE( DS );
		SAFE_RELEASE( DSS );
	}
};


struct D3D9Shader : SGRX_IShader
{
	IDirect3DVertexShader9* m_VS;
	IDirect3DPixelShader9* m_PS;
	ID3DXConstantTable* m_VSCT;
	ID3DXConstantTable* m_PSCT;
	struct D3D9Renderer* m_renderer;
	
	D3D9Shader( struct D3D9Renderer* r ) : m_VS( NULL ), m_PS( NULL ), m_VSCT( NULL ), m_PSCT( NULL ), m_renderer( r ){}
	~D3D9Shader()
	{
		SAFE_RELEASE( m_VSCT );
		SAFE_RELEASE( m_PSCT );
		SAFE_RELEASE( m_VS );
		SAFE_RELEASE( m_PS );
	}
};


struct D3D9VertexDecl : SGRX_IVertexDecl
{
	IDirect3DVertexDeclaration9* m_vdecl;
	struct D3D9Renderer* m_renderer;
	
	~D3D9VertexDecl()
	{
		SAFE_RELEASE( m_vdecl );
	}
};


struct D3D9Mesh : SGRX_IMesh
{
	IDirect3DVertexBuffer9* m_VB;
	IDirect3DIndexBuffer9* m_IB;
	struct D3D9Renderer* m_renderer;
	
	D3D9Mesh() : m_VB( NULL ), m_IB( NULL ), m_renderer( NULL ){}
	~D3D9Mesh();
	bool SetVertexData( const void* data, size_t size, VertexDeclHandle vd, bool tristrip )
	{
		return InitVertexBuffer( size ) && UpdateVertexData( data, size, vd, tristrip );
	}
	bool SetIndexData( const void* data, size_t size, bool i32 )
	{
		return InitIndexBuffer( size, i32 ) && UpdateIndexData( data, size );
	}
	bool InitVertexBuffer( size_t size );
	bool InitIndexBuffer( size_t size, bool i32 );
	bool UpdateVertexData( const void* data, size_t size, VertexDeclHandle vd, bool tristrip );
	bool UpdateIndexData( const void* data, size_t size );
	
	bool OnDeviceLost();
	bool OnDeviceReset();
};


struct ScreenSpaceVtx
{
	float x, y, z;
	float u0, v0;
	float u1, v1;
};

struct RTOutInfo
{
	IDirect3DSurface9* CS;
	IDirect3DSurface9* DS;
	IDirect3DSurface9* DSS;
	int w, h;
};

struct RTData
{
	IDirect3DTexture9* RTT_OCOL; /* oR, oG, oB, oA, RGBA16F */
	IDirect3DTexture9* RTT_PARM; /* distX, distY, emissive, oA (blending purposes), RGBA16F */
	IDirect3DTexture9* RTT_DEPTH; /* depth, R32F */
	IDirect3DTexture9* RTT_BLOOM_DSHP; /* bloom downsample/high-pass RT, RGBA8 */
	IDirect3DTexture9* RTT_BLOOM_BLUR1; /* bloom horizontal blur RT, RGBA8 */
	IDirect3DTexture9* RTT_BLOOM_BLUR2; /* bloom vertical blur RT, RGBA8 */
	
	IDirect3DSurface9* RTS_OCOL;
	IDirect3DSurface9* RTS_PARM;
	IDirect3DSurface9* RTS_DEPTH;
	IDirect3DSurface9* RTS_BLOOM_DSHP;
	IDirect3DSurface9* RTS_BLOOM_BLUR1;
	IDirect3DSurface9* RTS_BLOOM_BLUR2;
	IDirect3DSurface9* RTSD;
	
	IDirect3DSurface9* RTS_OCOL_MSAA;
	IDirect3DSurface9* RTS_PARM_MSAA;
	IDirect3DSurface9* RTS_DEPTH_MSAA;
	IDirect3DSurface9* RTSD_MSAA;
	
	int width, height;
	D3DMULTISAMPLE_TYPE mstype;
};


static const char* postproc_init( IDirect3DDevice9* dev, RTData* D, int w, int h, D3DMULTISAMPLE_TYPE msaa )
{
	HRESULT hr;
	memset( D, 0, sizeof(*D) );
	
	D->width = w;
	D->height = h;
	D->mstype = msaa;
	
	int w4 = w / 4;
	int h4 = h / 4;
	
	/* core */
	hr = dev->CreateTexture( w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &D->RTT_OCOL, NULL );
	if( FAILED( hr ) || !D->RTT_OCOL ) return "failed to create rgba16f render target texture (fs,1)";
	
	hr = dev->CreateTexture( w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &D->RTT_PARM, NULL );
	if( FAILED( hr ) || !D->RTT_PARM ) return "failed to create rgba16f render target texture (fs,2)";
	
	hr = dev->CreateTexture( w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &D->RTT_DEPTH, NULL );
	if( FAILED( hr ) || !D->RTT_PARM ) return "failed to create r32f depth stencil texture (fs,3)";
	
	/* bloom */
	hr = dev->CreateTexture( w4, h4, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &D->RTT_BLOOM_DSHP, NULL );
	if( FAILED( hr ) || !D->RTT_BLOOM_DSHP ) return "failed to create rgba16f render target texture (ds,4)";
	
	hr = dev->CreateTexture( w4, h4, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &D->RTT_BLOOM_BLUR1, NULL );
	if( FAILED( hr ) || !D->RTT_BLOOM_BLUR1 ) return "failed to create rgba16f render target texture (ds,5)";
	
	hr = dev->CreateTexture( w4, h4, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &D->RTT_BLOOM_BLUR2, NULL );
	if( FAILED( hr ) || !D->RTT_BLOOM_BLUR2 ) return "failed to create rgba16f render target texture (ds,6)";
	
	/* depth */
	hr = dev->CreateDepthStencilSurface( w, h, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &D->RTSD, NULL );
	if( FAILED( hr ) || !D->RTSD ) return "failed to create d24s8 depth+stencil surface (fs,7)";
	
	/* surfaces */
	D->RTT_OCOL->GetSurfaceLevel( 0, &D->RTS_OCOL );
	D->RTT_PARM->GetSurfaceLevel( 0, &D->RTS_PARM );
	D->RTT_DEPTH->GetSurfaceLevel( 0, &D->RTS_DEPTH );
	D->RTT_BLOOM_DSHP->GetSurfaceLevel( 0, &D->RTS_BLOOM_DSHP );
	D->RTT_BLOOM_BLUR1->GetSurfaceLevel( 0, &D->RTS_BLOOM_BLUR1 );
	D->RTT_BLOOM_BLUR2->GetSurfaceLevel( 0, &D->RTS_BLOOM_BLUR2 );
	
	if( msaa )
	{
		hr = dev->CreateRenderTarget( w, h, D3DFMT_A16B16G16R16F, msaa, 0, FALSE, &D->RTS_OCOL_MSAA, NULL );
		if( FAILED( hr ) || !D->RTS_OCOL_MSAA ) return "failed to create rgba16f render target surface (fs,8,aa,1)";
		
		hr = dev->CreateRenderTarget( w, h, D3DFMT_A16B16G16R16F, msaa, 0, FALSE, &D->RTS_PARM_MSAA, NULL );
		if( FAILED( hr ) || !D->RTS_PARM_MSAA ) return "failed to create rgba16f render target surface (fs,9,aa,2)";
		
		hr = dev->CreateRenderTarget( w, h, D3DFMT_R32F, msaa, 0, FALSE, &D->RTS_DEPTH_MSAA, NULL );
		if( FAILED( hr ) || !D->RTS_DEPTH_MSAA ) return "failed to create rgba16f render target surface (fs,10,aa,3)";
		
		hr = dev->CreateDepthStencilSurface( w, h, D3DFMT_D24S8, msaa, 0, TRUE, &D->RTSD_MSAA, NULL );
		if( FAILED( hr ) || !D->RTSD_MSAA ) return "failed to create d24s8 depth+stencil surface (fs,11,aa,7)";
		
		dev->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
	}
	
	return NULL;
}

static void postproc_free( RTData* D )
{
	SAFE_RELEASE( D->RTS_OCOL_MSAA );
	SAFE_RELEASE( D->RTS_PARM_MSAA );
	SAFE_RELEASE( D->RTS_DEPTH_MSAA );
	SAFE_RELEASE( D->RTSD_MSAA );
	
	SAFE_RELEASE( D->RTS_OCOL );
	SAFE_RELEASE( D->RTS_PARM );
	SAFE_RELEASE( D->RTS_DEPTH );
	SAFE_RELEASE( D->RTS_BLOOM_DSHP );
	SAFE_RELEASE( D->RTS_BLOOM_BLUR1 );
	SAFE_RELEASE( D->RTS_BLOOM_BLUR2 );
	SAFE_RELEASE( D->RTT_OCOL );
	SAFE_RELEASE( D->RTT_PARM );
	SAFE_RELEASE( D->RTT_DEPTH );
	SAFE_RELEASE( D->RTT_BLOOM_DSHP );
	SAFE_RELEASE( D->RTT_BLOOM_BLUR1 );
	SAFE_RELEASE( D->RTT_BLOOM_BLUR2 );
	SAFE_RELEASE( D->RTSD );
	
	D->mstype = D3DMULTISAMPLE_NONE;
	D->width = 0;
	D->height = 0;
}

static IDirect3DSurface9* postproc_get_ocol( RTData* D ){ return D->mstype != D3DMULTISAMPLE_NONE ? D->RTS_OCOL_MSAA : D->RTS_OCOL; }
static IDirect3DSurface9* postproc_get_parm( RTData* D ){ return D->mstype != D3DMULTISAMPLE_NONE ? NULL : D->RTS_PARM; }
static IDirect3DSurface9* postproc_get_depth( RTData* D ){ return D->mstype != D3DMULTISAMPLE_NONE ? NULL : D->RTS_DEPTH; }
static IDirect3DSurface9* postproc_get_dss( RTData* D ){ return D->mstype != D3DMULTISAMPLE_NONE ? D->RTSD_MSAA : D->RTSD; }

static void postproc_resolve( IDirect3DDevice9* dev, RTData* D )
{
	if( !D->mstype )
		return;
	
	dev->EndScene();
	RECT r = { 0, 0, D->width, D->height };
	dev->StretchRect( D->RTS_OCOL_MSAA, &r, D->RTS_OCOL, &r, D3DTEXF_NONE );
	dev->StretchRect( D->RTS_PARM_MSAA, &r, D->RTS_PARM, &r, D3DTEXF_NONE );
	dev->StretchRect( D->RTS_DEPTH_MSAA, &r, D->RTS_DEPTH, &r, D3DTEXF_NONE );
//	dev->StretchRect( D->RTSD_MSAA, &r, D->RTSD, &r, D3DTEXF_NONE ); // ?
	dev->BeginScene();
}


RendererInfo g_D3D9RendererInfo =
{
	true, // swap R/B
	true, // compile shaders
	"d3d9", // shader type
};

struct D3D9Renderer : IRenderer
{
	D3D9Renderer() : m_dbg_rt( false ){ m_view.SetIdentity(); m_proj.SetIdentity(); }
	void Destroy();
	const RendererInfo& GetInfo(){ return g_D3D9RendererInfo; }
	void LoadInternalResources();
	void UnloadInternalResources();
	
	void Swap();
	void Modify( const RenderSettings& settings );
	void SetCurrent(){} // does nothing since there's no thread context pointer
	void Clear( float* color_v4f, bool clear_zbuffer );
	void SetRenderState( int state, uint32_t val );
	
	void SetWorldMatrix( const Mat4& mtx );
	void SetViewMatrix( const Mat4& mtx );
	void SetViewport( int x0, int y0, int x1, int y1 );
	void SetScissorRect( bool enable, int* rect );
	
	SGRX_ITexture* CreateTexture( TextureInfo* texinfo, void* data = NULL );
	SGRX_ITexture* CreateRenderTexture( TextureInfo* texinfo );
	bool CompileShader( const StringView& code, ByteArray& outcomp, String& outerrors );
	SGRX_IShader* CreateShader( ByteArray& code );
	SGRX_IVertexDecl* CreateVertexDecl( const VDeclInfo& vdinfo );
	SGRX_IMesh* CreateMesh();
	
	bool SetRenderTarget( TextureHandle rt );
	
	void DrawBatchVertices( BatchRenderer::Vertex* verts, uint32_t count, EPrimitiveType pt, SGRX_ITexture* tex, SGRX_IShader* shd, Vec4* shdata, size_t shvcount );
	
	bool SetRenderPasses( SGRX_RenderPass* passes, int count );
	
	void RenderScene( SceneHandle scene, bool enablePostProcessing, SGRX_Viewport* viewport, SGRX_PostDraw* postDraw, SGRX_DebugDraw* debugDraw );
	uint32_t _RS_Cull_Camera_MeshList();
	uint32_t _RS_Cull_Camera_PointLightList();
	uint32_t _RS_Cull_Camera_SpotLightList();
	uint32_t _RS_Cull_SpotLight_MeshList( SGRX_Light* L );
	void _RS_Compile_MeshLists();
	void _RS_Render_Shadows();
	void _RS_RenderPass_Object( const SGRX_RenderPass& pass, size_t pass_id );
	void _RS_RenderPass_Screen( const SGRX_RenderPass& pass, IDirect3DBaseTexture9* tx_depth, const RTOutInfo& RTOUT );
	void _RS_DebugDraw( SGRX_DebugDraw* debugDraw, IDirect3DSurface9* test_dss, IDirect3DSurface9* orig_dss );
	
	void PostProcBlit( int w, int h, int downsample, int ppdata_location );
	
	bool ResetDevice();
	void ResetViewport();
	void _SetTextureInt( int slot, IDirect3DBaseTexture9* tex, uint32_t flags );
	void SetTexture( int slot, SGRX_ITexture* tex );
	void SetShader( SGRX_IShader* shd );
	
	FINLINE void VS_SetVec4Array( int at, Vec4* arr, int size ){ m_dev->SetVertexShaderConstantF( at, &arr->x, size ); }
	FINLINE void VS_SetVec4Array( int at, float* arr, int size ){ m_dev->SetVertexShaderConstantF( at, arr, size ); }
	FINLINE void VS_SetVec4( int at, const Vec4& v ){ m_dev->SetVertexShaderConstantF( at, &v.x, 1 ); }
	FINLINE void VS_SetFloat( int at, float f ){ Vec4 v = {f,f,f,f}; m_dev->SetVertexShaderConstantF( at, &v.x, 1 ); }
	FINLINE void VS_SetMat4( int at, const Mat4& v ){ m_dev->SetVertexShaderConstantF( at, v.a, 4 ); }
	
	FINLINE void PS_SetVec4Array( int at, Vec4* arr, int size ){ m_dev->SetPixelShaderConstantF( at, &arr->x, size ); }
	FINLINE void PS_SetVec4Array( int at, float* arr, int size ){ m_dev->SetPixelShaderConstantF( at, arr, size ); }
	FINLINE void PS_SetVec4( int at, const Vec4& v ){ m_dev->SetPixelShaderConstantF( at, &v.x, 1 ); }
	FINLINE void PS_SetFloat( int at, float f ){ Vec4 v = {f,f,f,f}; m_dev->SetPixelShaderConstantF( at, &v.x, 1 ); }
	FINLINE void PS_SetMat4( int at, const Mat4& v ){ m_dev->SetPixelShaderConstantF( at, v.a, 4 ); }
	
	FINLINE int GetWidth() const { return m_params.BackBufferWidth; }
	FINLINE int GetHeight() const { return m_params.BackBufferHeight; }
	
	void MI_ApplyConstants( SGRX_MeshInstance* MI );
	void Viewport_Apply( int downsample );
	
	// state
	TextureHandle m_currentRT;
	bool m_dbg_rt;
	
	// helpers
	RTData m_drd;
	Mat4 m_view, m_proj;
	
	SGRX_IShader* m_sh_pp_final;
	SGRX_IShader* m_sh_pp_dshp;
	SGRX_IShader* m_sh_pp_blur_h;
	SGRX_IShader* m_sh_pp_blur_v;
	SGRX_IShader* m_sh_debug_draw;
	
	// storage
	HashTable< D3D9Texture*, bool > m_ownTextures;
	HashTable< D3D9Mesh*, bool > m_ownMeshes;
	
	// specific
	IDirect3DDevice9* m_dev;
	D3DPRESENT_PARAMETERS m_params;
	IDirect3DSurface9* m_backbuf;
	IDirect3DSurface9* m_dssurf;
	
	// temp data
	SceneHandle m_currentScene;
	bool m_enablePostProcessing;
	SGRX_Viewport* m_viewport;
	ByteArray m_scratchMem;
	Array< SGRX_MeshInstance* > m_visible_meshes;
	Array< SGRX_MeshInstance* > m_visible_spot_meshes;
	Array< SGRX_Light* > m_visible_point_lights;
	Array< SGRX_Light* > m_visible_spot_lights;
	Array< SGRX_MeshInstLight > m_inst_light_buf;
	Array< SGRX_MeshInstLight > m_light_inst_buf;
};

extern "C" EXPORT bool Initialize( const char** outname )
{
	*outname = "Direct3D9";
	g_D3D = Direct3DCreate9( D3D_SDK_VERSION );
	return g_D3D != NULL;
}

extern "C" EXPORT void Free()
{
	g_D3D->Release();
	g_D3D = NULL;
}

static D3DMULTISAMPLE_TYPE aa_quality_to_mstype( int aaq )
{
	if( aaq <= 0 )
		return D3DMULTISAMPLE_NONE;
	if( aaq > 16 )
		return D3DMULTISAMPLE_16_SAMPLES;
	return (D3DMULTISAMPLE_TYPE) aaq;
}

extern "C" EXPORT IRenderer* CreateRenderer( const RenderSettings& settings, void* windowHandle )
{
	D3DPRESENT_PARAMETERS d3dpp;
	IDirect3DDevice9* d3ddev;
	
	ZeroMemory( &d3dpp, sizeof(d3dpp) );
	d3dpp.Windowed = 1;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.EnableAutoDepthStencil = 1;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.hDeviceWindow = (HWND) windowHandle;
	d3dpp.BackBufferWidth = settings.width;
	d3dpp.BackBufferHeight = settings.height;
	d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	d3dpp.MultiSampleType = settings.aa_mode == ANTIALIAS_MULTISAMPLE ? aa_quality_to_mstype( settings.aa_quality ) : D3DMULTISAMPLE_NONE;
	d3dpp.PresentationInterval = settings.vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	
	if( FAILED( g_D3D->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		(HWND) windowHandle,
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
		&d3dpp,
		&d3ddev ) ) )
	{
		LOG << "Failed to create the D3D9 device";
		return NULL;
	}
	
	_ss_reset_states( d3ddev, settings.width, settings.height );
	
	D3D9Renderer* R = new D3D9Renderer;
	R->m_dev = d3ddev;
	R->m_params = d3dpp;
	R->m_currSettings = settings;
	
	if( FAILED( R->m_dev->GetRenderTarget( 0, &R->m_backbuf ) ) )
	{
		LOG_ERROR << "Failed to retrieve the original render target";
		return NULL;
	}
	if( FAILED( R->m_dev->GetDepthStencilSurface( &R->m_dssurf ) ) )
	{
		LOG_ERROR << "Failed to retrieve the original depth/stencil surface";
		return NULL;
	}
	
	postproc_init( d3ddev, &R->m_drd, settings.width, settings.height, d3dpp.MultiSampleType );
	
	d3ddev->BeginScene();
	
	return R;
}


void D3D9Renderer::LoadInternalResources()
{
	ShaderHandle sh_pp_final = GR_GetShader( "testFRpost" );
	ShaderHandle sh_pp_dshp = GR_GetShader( "pp_bloom_dshp" );
	ShaderHandle sh_pp_blur_h = GR_GetShader( "pp_bloom_blur_h" );
	ShaderHandle sh_pp_blur_v = GR_GetShader( "pp_bloom_blur_v" );
	ShaderHandle sh_debug_draw = GR_GetShader( "debug_draw" );
	sh_pp_final->Acquire();
	sh_pp_dshp->Acquire();
	sh_pp_blur_h->Acquire();
	sh_pp_blur_v->Acquire();
	sh_debug_draw->Acquire();
	m_sh_pp_final = sh_pp_final;
	m_sh_pp_dshp = sh_pp_dshp;
	m_sh_pp_blur_h = sh_pp_blur_h;
	m_sh_pp_blur_v = sh_pp_blur_v;
	m_sh_debug_draw = sh_debug_draw;
}

void D3D9Renderer::UnloadInternalResources()
{
	m_sh_pp_final->Release();
	m_sh_pp_dshp->Release();
	m_sh_pp_blur_h->Release();
	m_sh_pp_blur_v->Release();
	m_sh_debug_draw->Release();
}


void D3D9Renderer::Destroy()
{
	postproc_free( &m_drd );
	
	SAFE_RELEASE( m_backbuf );
	SAFE_RELEASE( m_dssurf );
	IDirect3DResource9_Release( m_dev );
	delete this;
}

void D3D9Renderer::Swap()
{
	m_currentRT = NULL;
	
	m_dev->EndScene();
	if( m_dev->Present( NULL, NULL, NULL, NULL ) == D3DERR_DEVICELOST )
	{
		if( m_dev->TestCooperativeLevel() == D3DERR_DEVICENOTRESET )
		{
			ResetDevice();
		}
	}
	m_dev->BeginScene();
}

void D3D9Renderer::Modify( const RenderSettings& settings )
{
	bool needreset = m_currSettings.width != settings.width || m_currSettings.height != settings.height;
	m_currSettings = settings;
	
	if( needreset )
	{
		m_params.BackBufferWidth = settings.width;
		m_params.BackBufferHeight = settings.height;
		ResetDevice();
		ResetViewport();
		m_dev->BeginScene();
	}
}

void D3D9Renderer::Clear( float* color_v4f, bool clear_zbuffer )
{
	uint32_t cc = 0;
	uint32_t flags = 0;
	if( color_v4f )
	{
		cc = COLOR_RGBA( color_v4f[2] * 255, color_v4f[1] * 255, color_v4f[0] * 255, color_v4f[3] * 255 );
		flags = D3DCLEAR_TARGET;
	}
	if( clear_zbuffer )
		flags |= D3DCLEAR_ZBUFFER;
	m_dev->Clear( 0, NULL, flags, cc, 1.0f, 0 );
}

void D3D9Renderer::SetRenderState( int state, uint32_t val )
{
	if( state == RS_ZENABLE )
	{
		m_dev->SetRenderState( D3DRS_ZENABLE, !!val );
	}
}


void D3D9Renderer::SetWorldMatrix( const Mat4& mtx )
{
	m_dev->SetTransform( D3DTS_WORLD, (const D3DMATRIX*) &mtx );
}

void D3D9Renderer::SetViewMatrix( const Mat4& mtx )
{
	m_view = Mat4::Identity;
	float w = GetWidth();
	float h = GetHeight();
	m_dev->SetTransform( D3DTS_VIEW, (D3DMATRIX*) &Mat4::Identity );
	Mat4 mfx = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  w ? -1.0f / w : 0, h ? 1.0f / h : 0, 0, 1 };
	m_proj = Mat4().Multiply( mtx, mfx );
	m_dev->SetTransform( D3DTS_PROJECTION, (D3DMATRIX*) &m_proj );
}

void D3D9Renderer::SetScissorRect( bool enable, int* rect )
{
	m_dev->SetRenderState( D3DRS_SCISSORTESTENABLE, enable );
	if( rect )
	{
		RECT r = { rect[0], rect[1], rect[2], rect[3] };
		m_dev->SetScissorRect( &r );
	}
}

void D3D9Renderer::SetViewport( int x0, int y0, int x1, int y1 )
{
	D3DVIEWPORT9 vp = { x0, y0, x1 - x0, y1 - y0, 0.0, 1.0 };
	m_dev->SetViewport( &vp );
}


D3D9Texture::~D3D9Texture()
{
	m_renderer->m_ownTextures.unset( this );
	SAFE_RELEASE( m_ptr.base );
}

SGRX_ITexture* D3D9Renderer::CreateTexture( TextureInfo* texinfo, void* data )
{
	int mip, side;
	HRESULT hr;
	// TODO: filter unsupported formats / dimensions
	
	if( texinfo->type == TEXTYPE_2D )
	{
		IDirect3DTexture9* d3dtex;
		
		hr = m_dev->CreateTexture( texinfo->width, texinfo->height, texinfo->mipcount, 0, texfmt2d3d( texinfo->format ), D3DPOOL_MANAGED, &d3dtex, NULL );
		if( FAILED( hr ) )
		{
			LOG_ERROR << "could not create D3D9 texture (type: 2D, w: " << texinfo->width << ", h: " <<
				texinfo->height << ", mips: " << texinfo->mipcount << ", fmt: " << texinfo->format << ", d3dfmt: " << texfmt2d3d( texinfo->format );
			return NULL;
		}
		
		if( data )
		{
			// load all mip levels into it
			for( mip = 0; mip < texinfo->mipcount; ++mip )
			{
				D3DLOCKED_RECT lr;
				hr = d3dtex->LockRect( mip, &lr, NULL, D3DLOCK_DISCARD );
				if( FAILED( hr ) )
				{
					LOG_ERROR << "failed to lock D3D9 texture";
					return NULL;
				}
				
				texdatacopy( &lr, texinfo, data, 0, mip );
				
				hr = d3dtex->UnlockRect( mip );
				if( FAILED( hr ) )
				{
					LOG_ERROR << "failed to unlock D3D9 texture";
					return NULL;
				}
			}
		}
		
		D3D9Texture* T = new D3D9Texture;
		T->m_renderer = this;
		T->m_info = *texinfo;
		T->m_ptr.tex2d = d3dtex;
		m_ownTextures.set( T, true );
		return T;
	}
	else if( texinfo->type == TEXTYPE_CUBE )
	{
		IDirect3DCubeTexture9* d3dtex;
		
		hr = m_dev->CreateCubeTexture( texinfo->width, texinfo->mipcount, 0, texfmt2d3d( texinfo->format ), D3DPOOL_MANAGED, &d3dtex, NULL );
		
		if( data )
		{
			// load all mip levels into it
			for( side = 0; side < 6; ++side )
			{
				for( mip = 0; mip < texinfo->mipcount; ++mip )
				{
					D3DLOCKED_RECT lr;
					hr = d3dtex->LockRect( (D3DCUBEMAP_FACES) side, mip, &lr, NULL, D3DLOCK_DISCARD );
					if( FAILED( hr ) )
					{
						LOG_ERROR << "failed to lock D3D9 texture";
						return NULL;
					}
					
					texdatacopy( &lr, texinfo, data, side, mip );
					
					hr = d3dtex->UnlockRect( (D3DCUBEMAP_FACES) side, mip );
					if( FAILED( hr ) )
					{
						LOG_ERROR << "failed to unlock D3D9 texture";
						return NULL;
					}
				}
			}
		}
		
		D3D9Texture* T = new D3D9Texture;
		T->m_renderer = this;
		T->m_info = *texinfo;
		T->m_ptr.cube = d3dtex;
		m_ownTextures.set( T, true );
		return T;
	}
	
	LOG_ERROR << "TODO [reached a part of not-yet-defined behavior]";
	return NULL;
}


SGRX_ITexture* D3D9Renderer::CreateRenderTexture( TextureInfo* texinfo )
{
	D3DFORMAT d3dfmt = (D3DFORMAT) 0;
	HRESULT hr = 0;
	int width = texinfo->width, height = texinfo->height, format = texinfo->format;
	
	switch( format )
	{
	case RT_FORMAT_BACKBUFFER:
		d3dfmt = D3DFMT_A16B16G16R16F;
		break;
	case RT_FORMAT_DEPTH:
		d3dfmt = D3DFMT_R32F;
		break;
	default:
		LOG_ERROR << "format ID was not recognized / supported: " << format;
		return NULL;
	}
	
	
	IDirect3DTexture9 *CT = NULL, *DT = NULL;
	IDirect3DSurface9 *CS = NULL, *DS = NULL, *DSS = NULL;
	D3D9RenderTexture* RT = NULL;
	
	if( format == RT_FORMAT_DEPTH )
	{
		hr = m_dev->CreateTexture( width, height, 1, D3DUSAGE_RENDERTARGET, d3dfmt, D3DPOOL_DEFAULT, &CT, NULL );
		if( FAILED( hr ) || !CT )
		{
			LOG_ERROR << "failed to create D3D9 render target texture (HRESULT=" << (void*) hr << ")";
			goto cleanup;
		}
		
		hr = m_dev->CreateDepthStencilSurface( width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &DSS, NULL );
		if( FAILED( hr ) || !DSS )
		{
			LOG_ERROR << "failed to create D3D9 d24s8 depth+stencil surface (HRESULT=" << (void*) hr << ")";
			goto cleanup;
		}
		
		hr = CT->GetSurfaceLevel( 0, &CS );
		if( FAILED( hr ) || !CS )
		{
			LOG_ERROR << "failed to get surface from D3D9 render target texture (HRESULT=" << (void*) hr << ")";
			goto cleanup;
		}
	}
	else
	{
		hr = m_dev->CreateTexture( width, height, 1, D3DUSAGE_RENDERTARGET, d3dfmt, D3DPOOL_DEFAULT, &CT, NULL );
		if( FAILED( hr ) || !CT )
		{
			LOG_ERROR << "failed to create D3D9 render target texture (HRESULT=" << (void*) hr << ")";
			goto cleanup;
		}
		
		hr = m_dev->CreateTexture( width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R32F, D3DPOOL_DEFAULT, &DT, NULL );
		if( FAILED( hr ) || !DT )
		{
			LOG_ERROR << "failed to create D3D9 r32f depth stencil texture (HRESULT=" << (void*) hr << ")";
			goto cleanup;
		}
		
		hr = m_dev->CreateDepthStencilSurface( width, height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, TRUE, &DSS, NULL );
		if( FAILED( hr ) || !DSS )
		{
			LOG_ERROR << "failed to create D3D9 d24s8 depth+stencil surface (HRESULT=" << (void*) hr << ")";
			goto cleanup;
		}
		
		hr = CT->GetSurfaceLevel( 0, &CS );
		if( FAILED( hr ) || !CS )
		{
			LOG_ERROR << "failed to get surface from D3D9 render target texture (HRESULT=" << (void*) hr << ")";
			goto cleanup;
		}
		
		hr = DT->GetSurfaceLevel( 0, &DS );
		if( FAILED( hr ) || !CS )
		{
			LOG_ERROR << "failed to get surface from D3D9 depth stencil texture (HRESULT=" << (void*) hr << ")";
			goto cleanup;
		}
	}
	
	RT = new D3D9RenderTexture;
	RT->m_renderer = this;
	RT->m_info = *texinfo;
	RT->m_ptr.tex2d = CT;
	RT->CS = CS;
	RT->DS = DS;
	RT->DT = DT;
	RT->DSS = DSS;
	m_ownTextures.set( RT, true );
	return RT;
	
cleanup:
	SAFE_RELEASE( CS );
	SAFE_RELEASE( DS );
	SAFE_RELEASE( DT );
	SAFE_RELEASE( DSS );
	SAFE_RELEASE( CT );
	return NULL;
}


bool D3D9Renderer::CompileShader( const StringView& code, ByteArray& outcomp, String& outerrors )
{
	HRESULT hr;
	ID3DXBuffer *outbuf = NULL, *outerr = NULL;
	
	static const D3DXMACRO vsmacros[] = { { "VS", "1" }, { NULL, NULL } };
	static const D3DXMACRO psmacros[] = { { "PS", "1" }, { NULL, NULL } };
	
	ByteWriter bw( &outcomp );
	bw.marker( "CSH\x7f", 4 );
	
	int32_t shsize;
	
	hr = D3DXCompileShader( code.data(), code.size(), vsmacros, NULL, "main", "vs_3_0", D3DXSHADER_OPTIMIZATION_LEVEL3, &outbuf, &outerr, NULL );
	if( FAILED( hr ) )
	{
		if( outerr )
		{
			const char* errtext = (const char*) outerr->GetBufferPointer();
			outerrors.append( STRLIT_BUF( "Errors in vertex shader compilation:\n" ) );
			outerrors.append( errtext, strlen( errtext ) );
		}
		else
			outerrors.append( STRLIT_BUF( "Unknown error in vertex shader compilation" ) );
		
		SAFE_RELEASE( outbuf );
		SAFE_RELEASE( outerr );
		return false;
	}
	
	shsize = outbuf->GetBufferSize();
	
	bw << shsize;
	bw.memory( outbuf->GetBufferPointer(), shsize );
	
	SAFE_RELEASE( outbuf );
	SAFE_RELEASE( outerr );
	
	hr = D3DXCompileShader( code.data(), code.size(), psmacros, NULL, "main", "ps_3_0", D3DXSHADER_OPTIMIZATION_LEVEL3, &outbuf, &outerr, NULL );
	if( FAILED( hr ) )
	{
		if( outerr )
		{
			const char* errtext = (const char*) outerr->GetBufferPointer();
			outerrors.append( STRLIT_BUF( "Errors in pixel shader compilation:\n" ) );
			outerrors.append( errtext, strlen( errtext ) );
		}
		else
			outerrors.append( STRLIT_BUF( "Unknown error in pixel shader compilation" ) );
		
		SAFE_RELEASE( outbuf );
		SAFE_RELEASE( outerr );
		return false;
	}
	
	shsize = outbuf->GetBufferSize();
	
	bw << shsize;
	bw.memory( outbuf->GetBufferPointer(), shsize );
	
	SAFE_RELEASE( outbuf );
	SAFE_RELEASE( outerr );
	
	return true;
}

SGRX_IShader* D3D9Renderer::CreateShader( ByteArray& code )
{
	HRESULT hr;
	ByteReader br( &code );
	br.marker( "CSH\x7f", 4 );
	if( br.error )
		return NULL;
	
	int32_t vslen = 0;
	br << vslen;
	uint8_t* vsbuf = &code[ 8 ];
	uint8_t* psbuf = &code[ 12 + vslen ];
	
	IDirect3DVertexShader9* VS = NULL;
	IDirect3DPixelShader9* PS = NULL;
	ID3DXConstantTable* VSCT = NULL;
	ID3DXConstantTable* PSCT = NULL;
	D3D9Shader* out = NULL;
	
	hr = m_dev->CreateVertexShader( (const DWORD*) vsbuf, &VS );
	if( FAILED( hr ) || !VS )
		{ LOG << "Failed to create a D3D9 vertex shader"; goto cleanup; }
	hr = D3DXGetShaderConstantTable( (const DWORD*) vsbuf, &VSCT );
	if( FAILED( hr ) || !VSCT )
		{ LOG << "Failed to create a constant table for vertex shader"; goto cleanup; }
	
	hr = m_dev->CreatePixelShader( (const DWORD*) psbuf, &PS );
	if( FAILED( hr ) || !PS )
		{ LOG << "Failed to create a D3D9 pixel shader"; goto cleanup; }
	hr = D3DXGetShaderConstantTable( (const DWORD*) psbuf, &PSCT );
	if( FAILED( hr ) || !PSCT )
		{ LOG << "Failed to create a constant table for pixel shader"; goto cleanup; }
	
	out = new D3D9Shader( this );
	out->m_VS = VS;
	out->m_PS = PS;
	out->m_VSCT = VSCT;
	out->m_PSCT = PSCT;
	return out;
	
cleanup:
	SAFE_RELEASE( VS );
	SAFE_RELEASE( PS );
	SAFE_RELEASE( VSCT );
	SAFE_RELEASE( PSCT );
	return NULL;
}


static int vdecltype_to_eltype[] =
{
	D3DDECLTYPE_FLOAT1,
	D3DDECLTYPE_FLOAT2,
	D3DDECLTYPE_FLOAT3,
	D3DDECLTYPE_FLOAT4,
	D3DDECLTYPE_D3DCOLOR,
};

static int vdeclusage_to_elusage[] =
{
	D3DDECLUSAGE_POSITION,
	D3DDECLUSAGE_COLOR,
	D3DDECLUSAGE_NORMAL,
	D3DDECLUSAGE_TANGENT,
	D3DDECLUSAGE_BLENDWEIGHT,
	D3DDECLUSAGE_BLENDINDICES,
	D3DDECLUSAGE_TEXCOORD,
	D3DDECLUSAGE_TEXCOORD,
	D3DDECLUSAGE_TEXCOORD,
	D3DDECLUSAGE_TEXCOORD,
};

static int vdeclusage_to_elusageindex[] = { 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };

SGRX_IVertexDecl* D3D9Renderer::CreateVertexDecl( const VDeclInfo& vdinfo )
{
	D3DVERTEXELEMENT9 elements[ VDECL_MAX_ITEMS + 1 ], end[1] = { D3DDECL_END() };
	for( int i = 0; i < vdinfo.count; ++i )
	{
		elements[ i ].Stream = 0;
		elements[ i ].Offset = vdinfo.offsets[ i ];
		elements[ i ].Type = vdecltype_to_eltype[ vdinfo.types[ i ] ];
		elements[ i ].Method = D3DDECLMETHOD_DEFAULT;
		elements[ i ].Usage = vdeclusage_to_elusage[ vdinfo.usages[ i ] ];
		elements[ i ].UsageIndex = vdeclusage_to_elusageindex[ vdinfo.usages[ i ] ];
		if( vdinfo.usages[ i ] == VDECLUSAGE_BLENDIDX && vdinfo.types[ i ] == VDECLTYPE_BCOL4 )
			elements[ i ].Type = D3DDECLTYPE_UBYTE4;
		if( vdinfo.usages[ i ] == VDECLUSAGE_BLENDWT && vdinfo.types[ i ] == VDECLTYPE_BCOL4 )
			elements[ i ].Type = D3DDECLTYPE_UBYTE4N;
	}
	memcpy( elements + vdinfo.count, end, sizeof(*end) );
	
	IDirect3DVertexDeclaration9* VD = NULL;
	if( FAILED( m_dev->CreateVertexDeclaration( elements, &VD ) ) || !VD )
	{
		LOG_ERROR << "Failed to create D3D9 vertex declaration";
		return NULL;
	}
	
	D3D9VertexDecl* vdecl = new D3D9VertexDecl;
	vdecl->m_vdecl = VD;
	vdecl->m_info = vdinfo;
	vdecl->m_renderer = this;
	return vdecl;
}


D3D9Mesh::~D3D9Mesh()
{
	m_renderer->m_ownMeshes.unset( this );
	SAFE_RELEASE( m_VB );
	SAFE_RELEASE( m_IB );
}

bool D3D9Mesh::InitVertexBuffer( size_t size )
{
	bool dyn = !!( m_dataFlags & MDF_DYNAMIC );
	SAFE_RELEASE( m_VB );
	m_renderer->m_dev->CreateVertexBuffer( size, dyn ? D3DUSAGE_DYNAMIC : 0, 0, dyn ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED, &m_VB, NULL );
	if( !m_VB )
	{
		LOG_ERROR << "failed to create D3D9 vertex buffer (size=" << size << ")";
		return false;
	}
	m_vertexDataSize = size;
	return true;
}

bool D3D9Mesh::InitIndexBuffer( size_t size, bool i32 )
{
	bool dyn = !!( m_dataFlags & MDF_DYNAMIC );
	SAFE_RELEASE( m_IB );
	m_renderer->m_dev->CreateIndexBuffer( size, dyn ? D3DUSAGE_DYNAMIC : 0, i32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16, dyn ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED, &m_IB, NULL );
	if( !m_IB )
	{
		LOG_ERROR << "failed to create D3D9 index buffer (size=" << size << ", i32=" << i32 << ")";
		return false;
	}
	m_dataFlags = ( m_dataFlags & ~MDF_INDEX_32 ) | ( MDF_INDEX_32 * i32 );
	m_indexDataSize = size;
	return true;
}

bool D3D9Mesh::UpdateVertexData( const void* data, size_t size, VertexDeclHandle vd, bool tristrip )
{
	void* vb_data;
	
	if( size > m_vertexDataSize )
	{
		LOG_ERROR << "given vertex data is too big";
		return false;
	}
	
	if( FAILED( m_VB->Lock( 0, 0, &vb_data, D3DLOCK_DISCARD ) ) )
	{
		LOG_ERROR << "failed to lock D3D9 vertex buffer";
		return false;
	}
	
	memcpy( vb_data, data, size );
	
	if( FAILED( m_VB->Unlock() ) )
	{
		LOG_ERROR << "failed to unlock D3D9 vertex buffer";
		return false;
	}
	
	m_vertexDecl = vd;
	return true;
}

bool D3D9Mesh::UpdateIndexData( const void* data, size_t size )
{
	void* ib_data;
	
	if( size > m_indexDataSize )
	{
		LOG_ERROR << "given index data is too big";
		return false;
	}
	
	if( FAILED( m_IB->Lock( 0, 0, &ib_data, D3DLOCK_DISCARD ) ) )
	{
		LOG_ERROR << "failed to lock D3D9 index buffer";
		return false;
	}
	
	memcpy( ib_data, data, size );
	
	if( FAILED( m_IB->Unlock() ) )
	{
		LOG_ERROR << "failed to unlock D3D9 index buffer";
		return false;
	}
	
	return true;
}

bool D3D9Mesh::OnDeviceLost()
{
	void *src_data, *dst_data;
	const char* reason = NULL;
	if( m_dataFlags & MDF_DYNAMIC )
	{
		int i32 = m_dataFlags & MDF_INDEX_32;
		IDirect3DVertexBuffer9* tmpVB = NULL;
		IDirect3DIndexBuffer9* tmpIB = NULL;
		
		if( m_vertexDataSize )
		{
			if( FAILED( m_renderer->m_dev->CreateVertexBuffer( m_vertexDataSize, D3DUSAGE_DYNAMIC, 0, D3DPOOL_SYSTEMMEM, &tmpVB, NULL ) ) )
				{ reason = "failed to create temp. VB"; goto fail; }
			src_data = dst_data = NULL;
			if( FAILED( m_VB->Lock( 0, 0, &src_data, 0 ) ) ){ reason = "failed to lock orig. VB"; goto fail; }
			if( FAILED( tmpVB->Lock( 0, 0, &dst_data, 0 ) ) ){ reason = "failed to lock temp. VB"; goto fail; }
			memcpy( dst_data, src_data, m_vertexDataSize );
			if( FAILED( tmpVB->Unlock() ) ){ reason = "failed to unlock orig. VB"; goto fail; }
			if( FAILED( m_VB->Unlock() ) ){ reason = "failed to unlock temp. VB"; goto fail; }
		}
		SAFE_RELEASE( m_VB );
		m_VB = tmpVB;
		
		if( m_indexDataSize )
		{
			if( FAILED( m_renderer->m_dev->CreateIndexBuffer( m_indexDataSize, D3DUSAGE_DYNAMIC, i32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16, D3DPOOL_SYSTEMMEM, &tmpIB, NULL ) ) )
				{ reason = "failed to create temp. IB"; goto fail; }
			src_data = dst_data = NULL;
			if( FAILED( m_IB->Lock( 0, 0, &src_data, 0 ) ) ){ reason = "failed to lock orig. IB"; goto fail; }
			if( FAILED( tmpIB->Lock( 0, 0, &dst_data, 0 ) ) ){ reason = "failed to lock temp. IB"; goto fail; }
			memcpy( dst_data, src_data, m_indexDataSize );
			if( FAILED( tmpIB->Unlock() ) ){ reason = "failed to unlock orig. IB"; goto fail; }
			if( FAILED( m_IB->Unlock() ) ){ reason = "failed to unlock temp. IB"; goto fail; }
		}
		SAFE_RELEASE( m_IB );
		m_IB = tmpIB;
	}
	
	return true;
	
fail:
	LOG_ERROR << "failed to handle lost device mesh: %s" << ( reason ? reason : "<unknown reason>" );
	return false;
}

bool D3D9Mesh::OnDeviceReset()
{
	void *src_data, *dst_data;
	if( m_dataFlags & MDF_DYNAMIC )
	{
		int i32 = m_dataFlags & MDF_INDEX_32;
		IDirect3DVertexBuffer9* tmpVB = NULL;
		IDirect3DIndexBuffer9* tmpIB = NULL;
		
		if( m_vertexDataSize )
		{
			if( FAILED( m_renderer->m_dev->CreateVertexBuffer( m_vertexDataSize, D3DUSAGE_DYNAMIC, 0, D3DPOOL_DEFAULT, &tmpVB, NULL ) ) ) goto fail;
			src_data = dst_data = NULL;
			if( FAILED( m_VB->Lock( 0, 0, &src_data, 0 ) ) ) goto fail;
			if( FAILED( tmpVB->Lock( 0, 0, &dst_data, D3DLOCK_DISCARD ) ) ) goto fail;
			memcpy( dst_data, src_data, m_vertexDataSize );
			if( FAILED( tmpVB->Unlock() ) ) goto fail;
			if( FAILED( m_VB->Unlock() ) ) goto fail;
		}
		SAFE_RELEASE( m_VB );
		m_VB = tmpVB;
		
		if( m_indexDataSize )
		{
			if( FAILED( m_renderer->m_dev->CreateIndexBuffer( m_indexDataSize, D3DUSAGE_DYNAMIC, i32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16, D3DPOOL_DEFAULT, &tmpIB, NULL ) ) ) goto fail;
			src_data = dst_data = NULL;
			if( FAILED( m_IB->Lock( 0, 0, &src_data, 0 ) ) ) goto fail;
			if( FAILED( tmpIB->Lock( 0, 0, &dst_data, D3DLOCK_DISCARD ) ) ) goto fail;
			memcpy( dst_data, src_data, m_indexDataSize );
			if( FAILED( tmpIB->Unlock() ) ) goto fail;
			if( FAILED( m_IB->Unlock() ) ) goto fail;
		}
		SAFE_RELEASE( m_IB );
		m_IB = tmpIB;
	}
	
	return true;
	
fail:
	LOG_ERROR << "failed to handle reset device mesh";
	return false;
}

SGRX_IMesh* D3D9Renderer::CreateMesh()
{
	D3D9Mesh* mesh = new D3D9Mesh;
	mesh->m_renderer = this;
	m_ownMeshes.set( mesh, true );
	return mesh;
}


bool D3D9Renderer::SetRenderTarget( TextureHandle rt )
{
	if( rt && !rt->m_isRenderTexture )
		return false;
	m_currentRT = rt;
	D3D9RenderTexture* RTT = rt ? (D3D9RenderTexture*) rt.item : NULL;
	IDirect3DSurface9* bbsurf = RTT ? RTT->CS : m_backbuf;
	IDirect3DSurface9* dssurf = RTT ? RTT->DSS : m_dssurf;
	m_dev->SetRenderTarget( 0, bbsurf );
	m_dev->SetDepthStencilSurface( dssurf );
	return true;
}


FINLINE D3DPRIMITIVETYPE conv_prim_type( EPrimitiveType pt )
{
	switch( pt )
	{
	case PT_Points: return D3DPT_POINTLIST;
	case PT_Lines: return D3DPT_LINELIST;
	case PT_LineStrip: return D3DPT_LINESTRIP;
	case PT_Triangles: return D3DPT_TRIANGLELIST;
	case PT_TriangleStrip: return D3DPT_TRIANGLESTRIP;
	case PT_TriangleFan: return D3DPT_TRIANGLEFAN;
	default: return (D3DPRIMITIVETYPE) 0;
	}
}

FINLINE uint32_t get_prim_count( EPrimitiveType pt, uint32_t numverts )
{
	switch( pt )
	{
	case PT_Points: return numverts;
	case PT_Lines: return numverts / 2;
	case PT_LineStrip: if( numverts < 2 ) return 0; return numverts - 1;
	case PT_Triangles: return numverts / 3;
	case PT_TriangleStrip:
	case PT_TriangleFan: if( numverts < 3 ) return 0; return numverts - 2;
	default: return 0;
	}
}

void D3D9Renderer::DrawBatchVertices( BatchRenderer::Vertex* verts, uint32_t count, EPrimitiveType pt, SGRX_ITexture* tex, SGRX_IShader* shd, Vec4* shdata, size_t shvcount )
{
	SetShader( shd );
	SetTexture( 0, tex );
	VS_SetVec4Array( 0, shdata, shvcount );
	PS_SetVec4Array( 0, shdata, shvcount );
	m_dev->SetFVF( D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE );
	m_dev->DrawPrimitiveUP( conv_prim_type( pt ), get_prim_count( pt, count ), verts, sizeof( *verts ) );
}



bool D3D9Renderer::SetRenderPasses( SGRX_RenderPass* passes, int count )
{
	for( int i = 0; i < count; ++i )
	{
		SGRX_RenderPass& PASS = passes[ i ];
		if( PASS.type != RPT_SHADOWS && PASS.type != RPT_OBJECT && PASS.type != RPT_SCREEN )
		{
			LOG_ERROR << "Invalid type for pass " << i;
			return false;
		}
		if( !PASS.shader_name )
		{
			LOG_ERROR << "No shader name for pass " << i;
			return false;
		}
	}
	
	Array< SGRX_RenderPass > oldpasses = m_renderPasses;
	m_renderPasses = Array< SGRX_RenderPass >( passes, count );
	
	for( int i = 0; i < (int) m_renderPasses.size(); ++i )
	{
		SGRX_RenderPass& PASS = m_renderPasses[ i ];
		if( PASS.type == RPT_SCREEN )
			PASS._shader = GR_GetShader( PASS.shader_name );
	}
	
	// TODO reload mesh shaders
	
	return true;
}



#define TEXTURE_FLAGS_FULLSCREEN (TEXFLAGS_HASMIPS | TEXFLAGS_LERP_X | TEXFLAGS_LERP_Y | TEXFLAGS_CLAMP_X | TEXFLAGS_CLAMP_Y)

/*
	R E N D E R I N G
	
	=== CONSTANT INFO ===
- POINT LIGHT: (total size: 2 constants, max. count: 16)
	VEC4 [px, py, pz, radius]
	VEC4 [cr, cg, cb, power]
- SPOT LIGHT: (total size: 4 constants for each shader, 2 textures, max. count: 4)
	TEXTURES [cookie, shadowmap]
[pixel shader]:
	VEC4 [px, py, pz, radius]
	VEC4 [cr, cg, cb, power]
	VEC4 [dx, dy, dz, angle]
	VEC4 [ux, uy, uz, -]
[vertex shader]:
	VEC4x4 [shadow map matrix]
- AMBIENT + DIRECTIONAL (SUN) LIGHT: (total size: 3 constants)
	VEC4 [ar, ag, ab, -]
	VEC4 [dx, dy, dz, -]
	VEC4 [cr, cg, cb, -]
	TEXTURES [some number of shadowmaps]
- DIR. AMBIENT DATA: (total size: 6 constants)
	VEC4 colorXP, colorXN, colorYP, colorYN, colorZP, colorZN
- SKY DATA: (total size: 1 constant)
	VEC4 [skyblend, -, -, -]
- FOG DATA: (total size: 2 constants)
	VEC4 [cr, cg, cb, min.dst]
	VEC4 [h1, d1, h2, d2] (heights, densities)
- VERTEX SHADER
	0-3: camera view matrix
	4-7: camera projection matrix
	23: light counts (point, spot)
	24-39: VS spot light data
- PIXEL SHADER
	0-3: camera inverse view matrix
	4: camera position
	11: sky data
	12-13: fog data
	14-19: dir.amb. data
	20-22: dir. light
	23: light counts (point, spot)
	24-39: PS spot light data
	56-87: point light data
	---
	100-115: instance data
*/
void D3D9Renderer::RenderScene( SceneHandle scene, bool enablePostProcessing, SGRX_Viewport* viewport, SGRX_PostDraw* postDraw, SGRX_DebugDraw* debugDraw )
{
	if( !scene )
		return;
	
	m_enablePostProcessing = enablePostProcessing;
	m_viewport = viewport;
	m_currentScene = scene;
	const SGRX_Camera& CAM = scene->camera;
	
	// PREPARE RT
	IDirect3DTexture9* tx_depth = m_drd.RTT_DEPTH;
	IDirect3DSurface9* su_depth = m_drd.RTS_DEPTH;
	RTOutInfo RTOUT = { NULL, su_depth, NULL, m_currSettings.width, m_currSettings.height };
	if( m_currentRT )
	{
		D3D9RenderTexture* RT = (D3D9RenderTexture*) m_currentRT.item;
		RTOUT.CS = RT->CS;
		RTOUT.DS = RT->DS;
		RTOUT.DSS = RT->DSS;
		RTOUT.w = RT->m_info.width;
		RTOUT.h = RT->m_info.height;
		tx_depth = RT->DT;
		su_depth = RT->DS;
	}
	else
	{
		m_dev->GetRenderTarget( 0, &RTOUT.CS );
		m_dev->GetDepthStencilSurface( &RTOUT.DSS );
	}
	int w = RTOUT.w, h = RTOUT.h;
	
	m_stats.Reset();
	// CULLING
	SGRX_Cull_Camera_Prepare( m_currentScene );
	m_stats.numVisMeshes = _RS_Cull_Camera_MeshList();
	m_stats.numVisPLights = _RS_Cull_Camera_PointLightList();
	m_stats.numVisSLights = _RS_Cull_Camera_SpotLightList();
	
	// MESH INST/LIGHT RELATIONS
	_RS_Compile_MeshLists();
	
	// RENDERING BEGINS
	m_dev->SetRenderState( D3DRS_ZENABLE, 1 );
	m_dev->SetRenderState( D3DRS_ZWRITEENABLE, 1 );
	
	// RENDER SHADOWS
	m_dev->SetRenderTarget( 1, NULL );
	m_dev->SetRenderTarget( 2, NULL );
	_RS_Render_Shadows();
	
	// RENDER PREP
	if( m_enablePostProcessing )
	{
		m_dev->SetRenderTarget( 0, postproc_get_ocol( &m_drd ) );
		m_dev->SetRenderTarget( 1, postproc_get_parm( &m_drd ) );
		m_dev->SetRenderTarget( 2, postproc_get_depth( &m_drd ) );
		m_dev->SetDepthStencilSurface( postproc_get_dss( &m_drd ) );
	}
	else
	{
		m_dev->SetRenderTarget( 0, RTOUT.CS );
		m_dev->SetRenderTarget( 1, NULL );
		m_dev->SetRenderTarget( 2, RTOUT.DS );
		m_dev->SetDepthStencilSurface( RTOUT.DSS );
	}
	m_dev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	m_dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	
	Viewport_Apply( 1 );
	
	m_dev->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0 // 0xff00ff00
		, 1.0f, 0 );
	
	/* upload unchanged data */
	VS_SetMat4( 0, CAM.mView );
	VS_SetMat4( 4, CAM.mProj );
	PS_SetMat4( 0, CAM.mInvView );
	PS_SetMat4( 4, CAM.mProj );
	Vec4 campos4 = { CAM.position.x, CAM.position.y, CAM.position.z, 0 };
	PS_SetVec4( 4, campos4 );
	
	Vec4 skydata[ 1 ] =
	{
		{ scene->skyTexture ? 1 : 0, 0, 0, 0 },
	};
	PS_SetVec4Array( 11, skydata, 1 );
	Vec3 fogGamma = scene->fogColor.Pow( 2.0 );
	Vec4 fogdata[ 2 ] =
	{
		{ fogGamma.x, fogGamma.y, fogGamma.z, scene->fogHeightFactor },
		{ scene->fogDensity, scene->fogHeightDensity, scene->fogStartHeight, scene->fogMinDist },
	};
	PS_SetVec4Array( 12, fogdata, 2 );
	
	Vec3 dirLightViewDir = -CAM.mView.TransformNormal( scene->dirLightDir ).Normalized();
	Vec4 dirlight[ 3 ] =
	{
		{ scene->ambientLightColor.x, scene->ambientLightColor.y, scene->ambientLightColor.z, 1 },
		{ dirLightViewDir.x, dirLightViewDir.y, dirLightViewDir.z, 0 },
		{ scene->dirLightColor.x, scene->dirLightColor.y, scene->dirLightColor.z, 1 },
	};
	PS_SetVec4Array( 20, dirlight, 3 );
	
	// MAIN PASSES
	for( size_t pass_id = 0; pass_id < m_renderPasses.size(); ++pass_id )
	{
		const SGRX_RenderPass& pass = m_renderPasses[ pass_id ];
		
		if( pass.type == RPT_OBJECT ) _RS_RenderPass_Object( pass, pass_id );
		else if( pass.type == RPT_SCREEN ) _RS_RenderPass_Screen( pass, tx_depth, RTOUT );
	}
	
	m_dev->SetRenderState( D3DRS_ZENABLE, 0 );
	m_dev->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	
	if( postDraw )
	{
		m_dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE ); // TODO HACK!!!
		m_dev->SetRenderState( D3DRS_ALPHABLENDENABLE, 1 );
		postDraw->PostDraw();
		GR2D_GetBatchRenderer().Flush().Reset();
		m_dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	}
	
	// POST-PROCESSING
	m_dev->SetRenderState( D3DRS_ALPHABLENDENABLE, 0 );
	
	if( m_enablePostProcessing )
	{
		uint32_t pptexflags = TEXTURE_FLAGS_FULLSCREEN;
		
		postproc_resolve( m_dev, &m_drd );
		
		m_dev->SetRenderTarget( 1, NULL );
		m_dev->SetRenderTarget( 2, NULL );
		m_dev->SetDepthStencilSurface( RTOUT.DSS );
		
		m_dev->SetRenderTarget( 0, m_drd.RTS_BLOOM_DSHP );
		m_dev->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0 );
		_SetTextureInt( 0, m_drd.RTT_OCOL, pptexflags );
		SetShader( m_sh_pp_dshp );
		PostProcBlit( RTOUT.w, RTOUT.h, 4, 0 );
		
		m_dev->SetRenderTarget( 0, m_drd.RTS_BLOOM_BLUR1 );
		m_dev->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0 );
		_SetTextureInt( 0, m_drd.RTT_BLOOM_DSHP, pptexflags );
		SetShader( m_sh_pp_blur_h );
		PostProcBlit( RTOUT.w, RTOUT.h, 4, 0 );
		
		m_dev->SetRenderTarget( 0, m_drd.RTS_BLOOM_BLUR2 );
		m_dev->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0 );
		_SetTextureInt( 0, m_drd.RTT_BLOOM_BLUR1, pptexflags );
		SetShader( m_sh_pp_blur_v );
		PostProcBlit( RTOUT.w, RTOUT.h, 4, 0 );
		
		m_dev->SetRenderTarget( 0, RTOUT.CS );
	//	m_dev->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0 );
		SetShader( m_sh_pp_final );
		_SetTextureInt( 0, m_drd.RTT_OCOL, pptexflags );
		_SetTextureInt( 1, m_drd.RTT_PARM, pptexflags );
		_SetTextureInt( 2, m_drd.RTT_BLOOM_BLUR2, pptexflags );
		PostProcBlit( RTOUT.w, RTOUT.h, 1, 0 );
	}
	
	// MANUAL DEBUG DRAWING
	if( debugDraw )
		_RS_DebugDraw( debugDraw, m_enablePostProcessing ? postproc_get_dss( &m_drd ) : RTOUT.DSS, RTOUT.DSS );
	
	// POST-PROCESS DEBUGGING
	if( m_currentRT && m_dbg_rt )
	{
		RECT srcRect = { 0, 0, w, h };
		RECT srcRect2 = { 0, 0, w/4, h/4 };
		RECT dstRect = { 0, h/8, w/8, h/4 };
		IDirect3DSurface9* surfs[] =
		{
			m_drd.RTS_OCOL,
			m_drd.RTS_PARM,
	//		m_drd.RTS_BLOOM_DSHP,
	//		m_drd.RTS_BLOOM_BLUR1,
			m_drd.RTS_BLOOM_BLUR2,
		};
		for( size_t i = 0; i < sizeof(surfs)/sizeof(surfs[0]); ++i )
		{
			RECT* sr = ( surfs[i] == m_drd.RTS_BLOOM_DSHP || surfs[i] == m_drd.RTS_BLOOM_BLUR1 || surfs[i] == m_drd.RTS_BLOOM_BLUR2 ) ? &srcRect2 : &srcRect;
			m_dev->StretchRect( surfs[i], sr, m_backbuf, &dstRect, D3DTEXF_POINT );
			dstRect.left += w/8;
			dstRect.right += w/8;
		}
	}
	
	// RENDERING ENDS
	SetShader( NULL );
	_SetTextureInt( 0, NULL, 0 );
	
	m_dev->SetRenderState( D3DRS_ALPHABLENDENABLE, 1 );
	m_dev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	m_dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	
	if( m_currentRT )
	{
		SAFE_RELEASE( RTOUT.CS );
		SAFE_RELEASE( RTOUT.DSS );
	}
	ResetViewport();
	
	// RESTORE STATE
	m_enablePostProcessing = false;
	m_viewport = NULL;
	m_currentScene = NULL;
}

uint32_t D3D9Renderer::_RS_Cull_Camera_MeshList()
{
	m_visible_meshes.clear();
	return SGRX_Cull_Camera_MeshList( m_visible_meshes, m_scratchMem, m_currentScene );
}

uint32_t D3D9Renderer::_RS_Cull_Camera_PointLightList()
{
	m_visible_point_lights.clear();
	return SGRX_Cull_Camera_PointLightList( m_visible_point_lights, m_scratchMem, m_currentScene );
}

uint32_t D3D9Renderer::_RS_Cull_Camera_SpotLightList()
{
	m_visible_spot_lights.clear();
	return SGRX_Cull_Camera_SpotLightList( m_visible_spot_lights, m_scratchMem, m_currentScene );
}

uint32_t D3D9Renderer::_RS_Cull_SpotLight_MeshList( SGRX_Light* L )
{
	m_visible_spot_meshes.clear();
	return SGRX_Cull_SpotLight_MeshList( m_visible_spot_meshes, m_scratchMem, m_currentScene, L );
}

static int sort_meshinstlight_by_light( const void* p1, const void* p2 )
{
	SGRX_MeshInstLight* mil1 = (SGRX_MeshInstLight*) p1;
	SGRX_MeshInstLight* mil2 = (SGRX_MeshInstLight*) p2;
	return mil1->L == mil2->L ? 0 : ( mil1->L < mil2->L ? -1 : 1 );
}

void D3D9Renderer::_RS_Compile_MeshLists()
{
	SGRX_Scene* scene = m_currentScene;
	
	for( size_t inst_id = 0; inst_id < scene->m_meshInstances.size(); ++inst_id )
	{
		SGRX_MeshInstance* MI = scene->m_meshInstances.item( inst_id ).key;
		
		MI->_lightbuf_begin = NULL;
		MI->_lightbuf_end = NULL;
		
		if( !MI->mesh || !MI->enabled || MI->mesh->m_dataFlags & MDF_UNLIT )
			continue;
		MI->_lightbuf_begin = (SGRX_MeshInstLight*) m_inst_light_buf.size();
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
		MI->_lightbuf_end = (SGRX_MeshInstLight*) m_inst_light_buf.size();
	}
	for( size_t inst_id = 0; inst_id < scene->m_meshInstances.size(); ++inst_id )
	{
		SGRX_MeshInstance* MI = scene->m_meshInstances.item( inst_id ).key;
		if( !MI->mesh || !MI->enabled || MI->mesh->m_dataFlags & MDF_UNLIT )
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

void D3D9Renderer::_RS_Render_Shadows()
{
	SGRX_Scene* scene = m_currentScene;
	
	for( size_t pass_id = 0; pass_id < m_renderPasses.size(); ++pass_id )
	{
		// Only shadow passes
		if( m_renderPasses[ pass_id ].type != RPT_SHADOWS )
			continue;
		
		for( size_t light_id = 0; light_id < scene->m_lights.size(); ++light_id )
		{
			Mat4 m_world_view, m_inv_view;
			
			SGRX_Light* L = scene->m_lights.item( light_id ).key;
			if( !L->enabled || !L->shadowTexture || !L->shadowTexture->m_isRenderTexture )
				continue;
			
			/* CULL */
			SGRX_Cull_SpotLight_Prepare( scene, L );
			_RS_Cull_SpotLight_MeshList( L );
			
			D3D9RenderTexture* RT = (D3D9RenderTexture*) L->shadowTexture.item;
			
			m_dev->SetRenderTarget( 0, RT->CS );
			m_dev->SetDepthStencilSurface( RT->DSS );
			m_dev->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xffffffff, 1.0f, 0 );
			
			VS_SetMat4( 4, L->projMatrix );
			m_inv_view = L->viewMatrix;
			m_inv_view.Transpose();
			PS_SetMat4( 0, m_inv_view );
			PS_SetMat4( 4, L->projMatrix );
			
			for( size_t miid = 0; miid < m_visible_spot_meshes.size(); ++miid )
			{
				SGRX_MeshInstance* MI = m_visible_spot_meshes[ miid ];
				
				if( !MI->mesh || !MI->enabled )
					continue; /* mesh not added / instance not enabled */
				
				D3D9Mesh* M = (D3D9Mesh*) MI->mesh.item;
				if( !M->m_vertexDecl )
					continue; /* mesh not initialized */
				
				D3D9VertexDecl* VD = (D3D9VertexDecl*) M->m_vertexDecl.item;
				
				/* if (transparent & want solid) or (solid & want transparent), skip */
				if( M->m_dataFlags & MDF_TRANSPARENT )
					continue;
				
				MI_ApplyConstants( MI );
				
				m_world_view.Multiply( MI->matrix, L->viewMatrix );
				VS_SetMat4( 0, m_world_view );
				
				m_dev->SetRenderState( D3DRS_CULLMODE, M->m_dataFlags & MDF_NOCULL ? D3DCULL_NONE : D3DCULL_CCW );
				m_dev->SetVertexDeclaration( VD->m_vdecl );
				m_dev->SetStreamSource( 0, M->m_VB, 0, VD->m_info.size );
				m_dev->SetIndices( M->m_IB );
				
				for( size_t part_id = 0; part_id < M->m_numParts; ++part_id )
				{
					SGRX_MeshPart* MP = &M->m_parts[ part_id ];
					
					ShaderHandle* shlist = MI->skin_matrices.size() ? MP->shaders_skin : MP->shaders;
					
					SetShader( shlist[ pass_id ] );
					for( int tex_id = 0; tex_id < NUM_MATERIAL_TEXTURES; ++tex_id )
						SetTexture( tex_id, MP->textures[ tex_id ] );
					
					if( MP->indexCount < 3 )
						continue;
					m_dev->DrawIndexedPrimitive(
						M->m_dataFlags & MDF_TRIANGLESTRIP ? D3DPT_TRIANGLESTRIP : D3DPT_TRIANGLELIST,
						MP->vertexOffset, 0, MP->vertexCount, MP->indexOffset, M->m_dataFlags & MDF_TRIANGLESTRIP ? MP->indexCount - 2 : MP->indexCount / 3 );
					m_stats.numDrawCalls++;
					m_stats.numSDrawCalls++;
				}
			}
		}
	}
}

void D3D9Renderer::_RS_RenderPass_Object( const SGRX_RenderPass& PASS, size_t pass_id )
{
	int obj_type = !!( PASS.flags & RPF_OBJ_STATIC ) - !!( PASS.flags & RPF_OBJ_DYNAMIC );
	int mtl_type = !!( PASS.flags & RPF_MTL_SOLID ) - !!( PASS.flags & RPF_MTL_TRANSPARENT );
	
	m_dev->SetRenderState( D3DRS_ZWRITEENABLE, mtl_type >= 0 );
	m_dev->SetRenderState( D3DRS_ALPHABLENDENABLE, mtl_type <= 0 );
	
	const SGRX_Camera& CAM = m_currentScene->camera;
	
	for( size_t inst_id = 0; inst_id < m_visible_meshes.size(); ++inst_id )
	{
		SGRX_MeshInstance* MI = m_visible_meshes[ inst_id ];
		if( !MI->mesh )
			continue;
		
		D3D9Mesh* M = (D3D9Mesh*) MI->mesh.item;
		if( !M->m_vertexDecl )
			continue;
		D3D9VertexDecl* VD = (D3D9VertexDecl*) M->m_vertexDecl.item;
		
		/* if (transparent & want solid) or (solid & want transparent), skip */
		if( ( ( M->m_dataFlags & MDF_TRANSPARENT ) && mtl_type > 0 ) || ( !( M->m_dataFlags & MDF_TRANSPARENT ) && mtl_type < 0 ) )
			continue;
		
		/* dynamic meshes */
		if( ( MI->dynamic && obj_type > 0 ) || ( !MI->dynamic && obj_type < 0 ) )
			continue;
		
		m_dev->SetRenderState( D3DRS_CULLMODE, M->m_dataFlags & MDF_NOCULL ? D3DCULL_NONE : D3DCULL_CCW );
		
		/* -------------------------------------- */
		do
		{
			/* WHILE THERE ARE LIGHTS IN A LIGHT OVERLAY PASS */
			int pl_count = 0, sl_count = 0;
			Vec4 lightdata[ 64 ];
			Vec4 *pldata_it = &lightdata[0];
			Vec4 *sldata_ps_it = &lightdata[32];
			Vec4 *sldata_vs_it = &lightdata[48];
			
			if( PASS.pointlight_count )
			{
				while( pl_count < PASS.pointlight_count && MI->_lightbuf_begin < MI->_lightbuf_end )
				{
					int found = 0;
					SGRX_MeshInstLight* plt = MI->_lightbuf_begin;
					while( plt < MI->_lightbuf_end )
					{
						if( plt->L->type == LIGHT_POINT )
						{
							SGRX_Light* light = plt->L;
							
							found = 1;
							
							// copy data
							Vec3 viewpos = CAM.mView.TransformPos( light->position );
							Vec4 newdata[2] =
							{
								{ viewpos.x, viewpos.y, viewpos.z, light->range },
								{ light->color.x, light->color.y, light->color.z, light->power }
							};
							memcpy( pldata_it, newdata, sizeof(Vec4)*2 );
							pldata_it += 2;
							pl_count++;
							
							// extract light from array
							if( plt > MI->_lightbuf_begin )
								*plt = *MI->_lightbuf_begin;
							MI->_lightbuf_begin++;
							
							break;
						}
						plt++;
					}
					if( !found )
						break;
				}
				PS_SetVec4Array( 56, &lightdata[0], 2 * pl_count );
			}
			if( PASS.spotlight_count )
			{
				while( sl_count < PASS.spotlight_count && MI->_lightbuf_begin < MI->_lightbuf_end )
				{
					int found = 0;
					SGRX_MeshInstLight* plt = MI->_lightbuf_begin;
					while( plt < MI->_lightbuf_end )
					{
						if( plt->L->type == LIGHT_SPOT )
						{
							SGRX_Light* light = plt->L;
							
							found = 1;
							
							// copy data
							Vec3 viewpos = CAM.mView.TransformPos( light->position );
							Vec3 viewdir = CAM.mView.TransformPos( light->direction ).Normalized();
							float tszx = 1, tszy = 1;
							if( light->shadowTexture )
							{
								const TextureInfo& texinfo = light->shadowTexture.GetInfo();
								tszx = texinfo.width;
								tszy = texinfo.height;
							}
							Vec4 newdata[4] =
							{
								{ viewpos.x, viewpos.y, viewpos.z, light->range },
								{ light->color.x, light->color.y, light->color.z, light->power },
								{ viewdir.x, viewdir.y, viewdir.z, DEG2RAD( light->angle ) },
								{ tszx, tszy, 1.0f / tszx, 1.0f / tszy },
							};
							memcpy( sldata_ps_it, newdata, sizeof(Vec4)*4 );
							Mat4 tmp;
							tmp.Multiply( MI->matrix, light->viewProjMatrix );
							memcpy( sldata_vs_it, &tmp, sizeof(Mat4) );
							sldata_ps_it += 4;
							sldata_vs_it += 4;
							
							SetTexture( 8 + sl_count * 2, light->cookieTexture );
							SetTexture( 8 + sl_count * 2 + 1, light->shadowTexture );
							sl_count++;
							
							// extract light from array
							if( plt > MI->_lightbuf_begin )
								*plt = *MI->_lightbuf_begin;
							MI->_lightbuf_begin++;
							
							break;
						}
						plt++;
					}
					if( !found )
						break;
				}
				VS_SetVec4Array( 24, &lightdata[48], 4 * sl_count );
				PS_SetVec4Array( 24, &lightdata[32], 4 * sl_count );
			}
			
			if( PASS.flags & RPF_LIGHTOVERLAY && pl_count + sl_count <= 0 )
				break;
			
			if( !( PASS.flags & RPF_LIGHTOVERLAY ) )
			{
				for( int i = 0; i < MAX_MI_TEXTURES; ++i )
					SetTexture( 8 + i, MI->textures[ i ] );
				m_dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
			}
			else
			{
				m_dev->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
				m_dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
			}
			
			MI_ApplyConstants( MI );
			
			Vec4 lightcounts = { pl_count, sl_count, 0, 0 };
			VS_SetVec4( 23, lightcounts );
			PS_SetVec4( 23, lightcounts );
			
			Mat4 m_world_view;
			m_world_view.Multiply( MI->matrix, CAM.mView );
			VS_SetMat4( 0, m_world_view );
			
			m_dev->SetVertexDeclaration( VD->m_vdecl );
			m_dev->SetStreamSource( 0, M->m_VB, 0, VD->m_info.size );
			m_dev->SetIndices( M->m_IB );
			
			for( size_t part_id = 0; part_id < M->m_numParts; ++part_id )
			{
				SGRX_MeshPart* MP = &M->m_parts[ part_id ];
				
				ShaderHandle* shlist = MI->skin_matrices.size() ? MP->shaders_skin : MP->shaders;
				if( !shlist[ pass_id ] )
					continue;
				
				SetShader( shlist[ pass_id ] );
				for( size_t tex_id = 0; tex_id < NUM_MATERIAL_TEXTURES; ++tex_id )
					SetTexture( tex_id, MP->textures[ tex_id ] );
				
				if( MP->indexCount < 3 )
					continue;
				m_dev->DrawIndexedPrimitive(
					M->m_dataFlags & MDF_TRIANGLESTRIP ? D3DPT_TRIANGLESTRIP : D3DPT_TRIANGLELIST,
					MP->vertexOffset, 0, MP->vertexCount, MP->indexOffset, M->m_dataFlags & MDF_TRIANGLESTRIP ? MP->indexCount - 2 : MP->indexCount / 3 );
				m_stats.numDrawCalls++;
				m_stats.numMDrawCalls++;
			}
			
			/* -------------------------------------- */
		}
		while( PASS.flags & RPF_LIGHTOVERLAY );
	}
}

void D3D9Renderer::_RS_RenderPass_Screen( const SGRX_RenderPass& pass, IDirect3DBaseTexture9* tx_depth, const RTOutInfo& RTOUT )
{
	const SGRX_Camera& CAM = m_currentScene->camera;
	
	m_dev->SetRenderState( D3DRS_ALPHABLENDENABLE, 1 );
	m_dev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	m_dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	m_dev->SetRenderState( D3DRS_ZENABLE, 0 );
	m_dev->SetRenderTarget( 1, NULL );
	m_dev->SetRenderTarget( 2, NULL );
	m_dev->SetDepthStencilSurface( NULL );
	m_dev->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	
	_SetTextureInt( 0, tx_depth, TEXTURE_FLAGS_FULLSCREEN );
	SetTexture( 4, m_currentScene->skyTexture );
	
	SetShader( pass._shader );
	Vec4 campos4 = { CAM.position.x, CAM.position.y, CAM.position.z, 0 };
	PS_SetVec4( 4, campos4 );
	PostProcBlit( RTOUT.w, RTOUT.h, 1, -1 );
	
	if( m_enablePostProcessing )
	{
		m_dev->SetRenderTarget( 1, postproc_get_parm( &m_drd ) );
		m_dev->SetRenderTarget( 2, postproc_get_depth( &m_drd ) );
		m_dev->SetDepthStencilSurface( postproc_get_dss( &m_drd ) );
	}
	else
	{
		m_dev->SetRenderTarget( 2, RTOUT.DS );
		m_dev->SetDepthStencilSurface( RTOUT.DSS );
	}
	
	Viewport_Apply( 1 );
	
	m_dev->SetRenderState( D3DRS_ZENABLE, 1 );
}

void D3D9Renderer::_RS_DebugDraw( SGRX_DebugDraw* debugDraw, IDirect3DSurface9* test_dss, IDirect3DSurface9* orig_dss )
{
	const SGRX_Camera& CAM = m_currentScene->camera;
	m_inDebugDraw = true;
	
	SetShader( m_sh_debug_draw );
	Mat4 worldMatrix, viewProjMatrix;
	worldMatrix.SetIdentity();
	viewProjMatrix.Multiply( CAM.mView, CAM.mProj );
	VS_SetMat4( 0, worldMatrix );
	VS_SetMat4( 4, viewProjMatrix );
	SetTexture( 0, NULL );
	
	m_dev->SetRenderState( D3DRS_ZENABLE, 0 );
	m_dev->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
	m_dev->SetDepthStencilSurface( test_dss );
	m_dev->SetRenderState( D3DRS_ALPHABLENDENABLE, 1 );
	m_dev->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
	m_dev->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
	
	m_dev->SetTexture( 0, NULL );
	SetShader( NULL );
	m_dev->SetTransform( D3DTS_VIEW, (D3DMATRIX*) &CAM.mView );
	m_dev->SetTransform( D3DTS_PROJECTION, (D3DMATRIX*) &CAM.mProj );
	debugDraw->DebugDraw();
	GR2D_GetBatchRenderer().Flush().Reset();
	
	m_dev->SetTransform( D3DTS_VIEW, (D3DMATRIX*) &m_view );
	m_dev->SetTransform( D3DTS_PROJECTION, (D3DMATRIX*) &m_proj );
	m_dev->SetRenderState( D3DRS_ZENABLE, 0 );
	m_dev->SetDepthStencilSurface( orig_dss );
	m_dev->SetRenderState( D3DRS_ALPHABLENDENABLE, 0 );
	
	m_inDebugDraw = false;
}


void D3D9Renderer::PostProcBlit( int w, int h, int downsample, int ppdata_location )
{
	/* assuming these are validated: */
	SGRX_Scene* scene = m_currentScene;
	const SGRX_Camera& CAM = scene->camera;
	
	float invQW = 2.0f, invQH = 2.0f, offX = -1.0f, offY = -1.0f, t0x = 0, t0y = 0, t1x = 1, t1y = 1;
	if( m_viewport )
	{
		SGRX_Viewport* VP = m_viewport;
		t0x = (float) VP->x1 / (float) w;
		t0y = (float) VP->y1 / (float) h;
		t1x = (float) VP->x2 / (float) w;
		t1y = (float) VP->y2 / (float) h;
		Viewport_Apply( downsample );
	}
	
	w /= downsample;
	h /= downsample;
	
	float hpox = 0.5f / w;
	float hpoy = 0.5f / h;
	float fsx = 1.0f / CAM.mProj.m[0][0];
	float fsy = 1.0f / CAM.mProj.m[1][1];
	ScreenSpaceVtx ssVertices[] =
	{
		{ offX, offY, 0, t0x+hpox, t1y+hpoy, -fsx, -fsy },
		{ invQW + offX, offY, 0, t1x+hpox, t1y+hpoy, +fsx, -fsy },
		{ invQW + offX, invQH + offY, 0, t1x+hpox, t0y+hpoy, +fsx, +fsy },
		{ offX, invQH + offY, 0, t0x+hpox, t0y+hpoy, -fsx, +fsy },
	};
	
	if( ppdata_location >= 0 )
	{
		Vec4 ppdata = { w, h, 1.0f / w, 1.0f / h };
		PS_SetVec4( ppdata_location, ppdata );
	}
	
	m_dev->SetFVF( D3DFVF_XYZ | D3DFVF_TEX2 );
	m_dev->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, ssVertices, sizeof(*ssVertices) );
	
	m_stats.numDrawCalls++;
	m_stats.numPDrawCalls++;
}


bool D3D9Renderer::ResetDevice()
{
	D3DPRESENT_PARAMETERS npp;
	
	for( size_t i = 0; i < m_ownMeshes.size(); ++i )
	{
		D3D9Mesh* mesh = m_ownMeshes.item( i ).key;
		if( !mesh->OnDeviceLost() )
		{
			LOG_ERROR << "Failed to prepare for resetting mesh " << mesh << " (" << mesh->m_key << ")";
		}
	}
	for( size_t i = 0; i < m_ownTextures.size(); ++i )
	{
		D3D9Texture* tex = m_ownTextures.item( i ).key;
		if( !tex->OnDeviceLost() )
		{
			LOG_ERROR << "Failed to prepare for resetting texture " << tex << " (" << tex->m_key << ")";
		}
	}
	
	/* reset */
	npp = m_params;
	
	if( FAILED( m_dev->Reset( &npp ) ) )
	{
		LOG_ERROR << "Failed to reset D3D9 device";
		return false;
	}
	
	for( size_t i = 0; i < m_ownMeshes.size(); ++i )
	{
		D3D9Mesh* mesh = m_ownMeshes.item( i ).key;
		if( !mesh->OnDeviceReset() )
		{
			LOG_ERROR << "Failed to restore after resetting mesh " << mesh << " (" << mesh->m_key << ")";
		}
	}
	for( size_t i = 0; i < m_ownTextures.size(); ++i )
	{
		D3D9Texture* tex = m_ownTextures.item( i ).key;
		if( !tex->OnDeviceReset() )
		{
			LOG_ERROR << "Failed to restore after resetting texture " << tex << " (" << tex->m_key << ")";
		}
	}
	
	_ss_reset_states( m_dev, m_params.BackBufferWidth, m_params.BackBufferHeight );
	return true;
}

void D3D9Renderer::ResetViewport()
{
	D3DVIEWPORT9 vp = { 0, 0, m_currSettings.width, m_currSettings.height, 0.0, 1.0 };
	m_dev->SetViewport( &vp );
}

void D3D9Renderer::_SetTextureInt( int slot, IDirect3DBaseTexture9* tex, uint32_t flags )
{
	m_dev->SetTexture( slot, tex );
	if( tex )
	{
		m_dev->SetSamplerState( slot, D3DSAMP_MAGFILTER, ( flags & TEXFLAGS_LERP_X ) ? D3DTEXF_LINEAR : D3DTEXF_POINT );
		m_dev->SetSamplerState( slot, D3DSAMP_MINFILTER, ( flags & TEXFLAGS_LERP_Y ) ? D3DTEXF_LINEAR : D3DTEXF_POINT );
		m_dev->SetSamplerState( slot, D3DSAMP_MIPFILTER, ( flags & TEXFLAGS_HASMIPS ) ? D3DTEXF_LINEAR : D3DTEXF_NONE );
		m_dev->SetSamplerState( slot, D3DSAMP_ADDRESSU, ( flags & TEXFLAGS_CLAMP_X ) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP );
		m_dev->SetSamplerState( slot, D3DSAMP_ADDRESSV, ( flags & TEXFLAGS_CLAMP_Y ) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP );
		m_dev->SetSamplerState( slot, D3DSAMP_SRGBTEXTURE, ( flags & TEXFLAGS_SRGB ) ? 1 : 0 );
	}
}

void D3D9Renderer::SetTexture( int slot, SGRX_ITexture* tex )
{
	SGRX_CAST( D3D9Texture*, T, tex ); 
	if( T )
		_SetTextureInt( slot, T->m_ptr.base, T->m_info.flags );
	else
		_SetTextureInt( slot, NULL, 0 );
}

void D3D9Renderer::SetShader( SGRX_IShader* shd )
{
	SGRX_CAST( D3D9Shader*, S, shd );
	m_dev->SetPixelShader( S ? S->m_PS : NULL );
	m_dev->SetVertexShader( S ? S->m_VS : NULL );
}

void D3D9Renderer::MI_ApplyConstants( SGRX_MeshInstance* MI )
{
	if( MI->skin_matrices.size() )
		VS_SetVec4Array( 40, (Vec4*) &MI->skin_matrices[0], MI->skin_matrices.size() * 4 );
	PS_SetVec4Array( 100, &MI->constants[0], 16 );
}

void D3D9Renderer::Viewport_Apply( int downsample )
{
	if( m_viewport )
	{
		SGRX_Viewport* VP = m_viewport;
		D3DVIEWPORT9 d3dvp =
		{
			VP->x1 / downsample,
			VP->y1 / downsample,
			( VP->x2 - VP->x1 ) / downsample,
			( VP->y2 - VP->y1 ) / downsample,
			0.0f,
			1.0f
		};
		m_dev->SetViewport( &d3dvp );
	}
}

