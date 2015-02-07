

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <d3d9.h>
#include <d3dx9.h>

#define USE_VEC3
#define USE_MAT4
#define USE_ARRAY
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
	
	~D3D9Texture()
	{
		SAFE_RELEASE( m_ptr.base );
	}
	
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
	~D3D9Mesh()
	{
		SAFE_RELEASE( m_VB );
		SAFE_RELEASE( m_IB );
	}
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


RendererInfo g_D3D9RendererInfo =
{
	true, // swap R/B
	true, // compile shaders
	"d3d9", // shader type
};

struct D3D9Renderer : IRenderer
{
	void Destroy();
	const RendererInfo& GetInfo(){ return g_D3D9RendererInfo; }
	
	void Swap();
	void Modify( const RenderSettings& settings );
	void SetCurrent(){} // does nothing since there's no thread context pointer
	void Clear( float* color_v4f, bool clear_zbuffer );
	
	void SetWorldMatrix( const Mat4& mtx );
	void SetViewMatrix( const Mat4& mtx );
	
	SGRX_ITexture* CreateTexture( TextureInfo* texinfo, void* data = NULL );
	bool CompileShader( const StringView& code, ByteArray& outcomp, String& outerrors );
	SGRX_IShader* CreateShader( ByteArray& code );
	SGRX_IVertexDecl* CreateVertexDecl( const VDeclInfo& vdinfo );
	SGRX_IMesh* CreateMesh();
	
	void DrawBatchVertices( BatchRenderer::Vertex* verts, uint32_t count, EPrimitiveType pt, SGRX_ITexture* tex );
	
	void PostProcBlit( RTOutInfo* outinfo, int downsample, int ppdata_location );
	
	bool ResetDevice();
	void ResetViewport();
	void SetTexture( int i, SGRX_ITexture* tex );
	void SetShader( SGRX_IShader* shd );
	
	FINLINE int GetWidth() const { return m_params.BackBufferWidth; }
	FINLINE int GetHeight() const { return m_params.BackBufferHeight; }
	
	IDirect3DDevice9* m_dev;
	D3DPRESENT_PARAMETERS m_params;
	RenderSettings m_currSettings;
	IDirect3DSurface9* m_backbuf;
	IDirect3DSurface9* m_dssurf;
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
	d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
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
	
	d3ddev->BeginScene();
	
	return R;
}


void D3D9Renderer::Destroy()
{
	SAFE_RELEASE( m_backbuf );
	SAFE_RELEASE( m_dssurf );
	IDirect3DResource9_Release( m_dev );
	delete this;
}

void D3D9Renderer::Swap()
{
	// TODO RT
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


void D3D9Renderer::SetWorldMatrix( const Mat4& mtx )
{
	m_dev->SetTransform( D3DTS_WORLD, (const D3DMATRIX*) &mtx );
}

void D3D9Renderer::SetViewMatrix( const Mat4& mtx )
{
	float w = GetWidth();
	float h = GetHeight();
	m_dev->SetTransform( D3DTS_VIEW, (D3DMATRIX*) &Mat4::Identity );
	Mat4 mfx = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  w ? -1.0f / w : 0, h ? 1.0f / h : 0, 0, 1 };
	m_dev->SetTransform( D3DTS_PROJECTION, (D3DMATRIX*) Mat4().Multiply( mtx, mfx ).a );
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
		return T;
	}
	
	LOG_ERROR << "TODO [reached a part of not-yet-defined behavior]";
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
	
	D3D9Shader S( this );
	
	hr = m_dev->CreateVertexShader( (const DWORD*) vsbuf, &S.m_VS );
	if( FAILED( hr ) )
		{ LOG << "Failed to create a D3D9 vertex shader"; goto cleanup; }
	hr = D3DXGetShaderConstantTable( (const DWORD*) vsbuf, &S.m_VSCT );
	if( FAILED( hr ) )
		{ LOG << "Failed to create a constant table for vertex shader"; goto cleanup; }
	
	hr = m_dev->CreatePixelShader( (const DWORD*) psbuf, &S.m_PS );
	if( FAILED( hr ) )
		{ LOG << "Failed to create a D3D9 pixel shader"; goto cleanup; }
	hr = D3DXGetShaderConstantTable( (const DWORD*) psbuf, &S.m_PSCT );
	if( FAILED( hr ) )
		{ LOG << "Failed to create a constant table for pixel shader"; goto cleanup; }
	
	return new D3D9Shader( S );
	
cleanup:
	SAFE_RELEASE( S.m_VSCT );
	SAFE_RELEASE( S.m_PSCT );
	SAFE_RELEASE( S.m_VS );
	SAFE_RELEASE( S.m_PS );
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
	vdecl->m_renderer = this;
	return vdecl;
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
	
	return 1;
}

bool D3D9Mesh::UpdateIndexData( const void* data, size_t size )
{
	void* ib_data;
	
	if( size > m_vertexDataSize )
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
	
	return 1;
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
	return mesh;
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

void D3D9Renderer::DrawBatchVertices( BatchRenderer::Vertex* verts, uint32_t count, EPrimitiveType pt, SGRX_ITexture* tex )
{
	SetTexture( 0, tex );
	m_dev->SetFVF( D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE );
	m_dev->DrawPrimitiveUP( conv_prim_type( pt ), get_prim_count( pt, count ), verts, sizeof( *verts ) );
}


void D3D9Renderer::PostProcBlit( RTOutInfo* outinfo, int downsample, int ppdata_location )
{
	int w = outinfo->w, h = outinfo->h;
	
	/* assuming these are validated: */
	SGRX_Scene* scene = m_currentScene;
	SGRX_Camera* cam = scene->camera;
	
	float invQW = 2.0f, invQH = 2.0f, offX = -1.0f, offY = -1.0f, t0x = 0, t0y = 0, t1x = 1, t1y = 1;
	// TODO
//	if( R->inh.viewport )
//	{
//		SS3D_Viewport* VP = (SS3D_Viewport*) R->inh.viewport->data;
//		t0x = (float) VP->x1 / (float) w;
//		t0y = (float) VP->y1 / (float) h;
//		t1x = (float) VP->x2 / (float) w;
//		t1y = (float) VP->y2 / (float) h;
//		viewport_apply( R, downsample );
//	}
	
	w /= downsample;
	h /= downsample;
	
	float hpox = 0.5f / w;
	float hpoy = 0.5f / h;
	float fsx = 1.0f / cam->mProj.m[0][0];
	float fsy = 1.0f / cam->mProj.m[1][1];
	ScreenSpaceVtx ssVertices[] =
	{
		{ offX, offY, 0, t0x+hpox, t1y+hpoy, -fsx, -fsy },
		{ invQW + offX, offY, 0, t1x+hpox, t1y+hpoy, +fsx, -fsy },
		{ invQW + offX, invQH + offY, 0, t1x+hpox, t0y+hpoy, +fsx, +fsy },
		{ offX, invQH + offY, 0, t0x+hpox, t0y+hpoy, -fsx, +fsy },
	};
	
	// TODO
//	if( ppdata_location >= 0 )
//	{
//		VEC4 ppdata;
//		VEC4_Set( ppdata, w, h, 1.0f / w, 1.0f / h );
//		pshc_set_vec4array( R, ppdata_location, ppdata, 1 );
//	}
	
	m_dev->SetFVF( D3DFVF_XYZ | D3DFVF_TEX2 );
	m_dev->DrawPrimitiveUP( D3DPT_TRIANGLEFAN, 2, ssVertices, sizeof(*ssVertices) );
	// TODO
//	R->inh.stat_numDrawCalls++;
//	R->inh.stat_numPDrawCalls++;
}


bool D3D9Renderer::ResetDevice()
{
	D3DPRESENT_PARAMETERS npp;
	
	/* reset */
	npp = m_params;
	
	if( FAILED( m_dev->Reset( &npp ) ) )
	{
		LOG_ERROR << "Failed to reset D3D9 device";
	}
	
	_ss_reset_states( m_dev, m_params.BackBufferWidth, m_params.BackBufferHeight );
}

void D3D9Renderer::ResetViewport()
{
	D3DVIEWPORT9 vp = { 0, 0, m_currSettings.width, m_currSettings.height, 0.0, 1.0 };
	m_dev->SetViewport( &vp );
}

void D3D9Renderer::SetTexture( int slot, SGRX_ITexture* tex )
{
	SGRX_CAST( D3D9Texture*, T, tex ); 
	m_dev->SetTexture( slot, T ? T->m_ptr.base : NULL );
	if( T )
	{
		uint32_t flags = T->m_info.flags;
		m_dev->SetSamplerState( slot, D3DSAMP_MAGFILTER, ( flags & TEXFLAGS_LERP_X ) ? D3DTEXF_LINEAR : D3DTEXF_POINT );
		m_dev->SetSamplerState( slot, D3DSAMP_MINFILTER, ( flags & TEXFLAGS_LERP_Y ) ? D3DTEXF_LINEAR : D3DTEXF_POINT );
		m_dev->SetSamplerState( slot, D3DSAMP_MIPFILTER, ( flags & TEXFLAGS_HASMIPS ) ? D3DTEXF_NONE : D3DTEXF_LINEAR );
		m_dev->SetSamplerState( slot, D3DSAMP_ADDRESSU, ( flags & TEXFLAGS_CLAMP_X ) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP );
		m_dev->SetSamplerState( slot, D3DSAMP_ADDRESSV, ( flags & TEXFLAGS_CLAMP_Y ) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP );
		m_dev->SetSamplerState( slot, D3DSAMP_SRGBTEXTURE, ( flags & TEXFLAGS_SRGB ) ? 1 : 0 );
	}
}

void D3D9Renderer::SetShader( SGRX_IShader* shd )
{
	SGRX_CAST( D3D9Shader*, S, shd );
	m_dev->SetPixelShader( S ? S->m_PS : NULL );
	m_dev->SetVertexShader( S ? S->m_VS : NULL );
}

