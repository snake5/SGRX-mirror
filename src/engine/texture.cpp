

#include "engine_int.hpp"

extern IRenderer* g_Renderer;
extern TextureHashTable* g_Textures;
extern RenderTargetTable* g_RenderTargets;
extern DepthStencilSurfTable* g_DepthStencilSurfs;



void ParseDefaultTextureFlags( const StringView& flags, uint32_t& outusageflags, uint8_t& outlod )
{
	if( flags.contains( ":nosrgb" ) ) outusageflags &= ~TEXFLAGS_SRGB;
	if( flags.contains( ":srgb" ) ) outusageflags |= TEXFLAGS_SRGB;
	if( flags.contains( ":wrapx" ) ) outusageflags &= ~TEXFLAGS_CLAMP_X;
	if( flags.contains( ":wrapy" ) ) outusageflags &= ~TEXFLAGS_CLAMP_Y;
	if( flags.contains( ":wrapz" ) ) outusageflags &= ~TEXFLAGS_CLAMP_Z;
	if( flags.contains( ":clampx" ) ) outusageflags |= TEXFLAGS_CLAMP_X;
	if( flags.contains( ":clampy" ) ) outusageflags |= TEXFLAGS_CLAMP_Y;
	if( flags.contains( ":clampz" ) ) outusageflags |= TEXFLAGS_CLAMP_Z;
	if( flags.contains( ":nolerp" ) ) outusageflags &= ~TEXFLAGS_LERP;
	if( flags.contains( ":lerp" ) ) outusageflags |= TEXFLAGS_LERP;
	if( flags.contains( ":nomips" ) ) outusageflags &= ~TEXFLAGS_HASMIPS;
	if( flags.contains( ":mips" ) ) outusageflags |= TEXFLAGS_HASMIPS;
	
	size_t pos = flags.find_first_at( ":lod" );
	if( pos != NOT_FOUND )
	{
		outlod = (uint8_t) String_ParseInt( flags.part( pos + 4 ) );
	}
}

TextureHandle OnCreateSysTexture( const StringView& key )
{
	if( key == "sys:black2d" )
	{
		uint32_t data[1] = { 0xff000000 };
		return GR_CreateTexture( 1, 1, TEXFMT_RGBA8, TEXFLAGS_LERP, 1, data );
	}
	if( key == "sys:blackt2d" )
	{
		uint32_t data[1] = { 0x00000000 };
		return GR_CreateTexture( 1, 1, TEXFMT_RGBA8, TEXFLAGS_LERP, 1, data );
	}
	if( key == "sys:white2d" )
	{
		uint32_t data[1] = { 0xffffffff };
		return GR_CreateTexture( 1, 1, TEXFMT_RGBA8, TEXFLAGS_LERP, 1, data );
	}
	if( key == "sys:normal2d" )
	{
		uint32_t data[1] = { 0xffff7f7f };
		return GR_CreateTexture( 1, 1, TEXFMT_RGBA8, TEXFLAGS_LERP, 1, data );
	}
	if( key == "sys:lut_default" )
	{
		uint32_t data[8] =
		{
			COLOR_RGB(0,0,0), COLOR_RGB(255,0,0), COLOR_RGB(0,255,0), COLOR_RGB(255,255,0),
			COLOR_RGB(0,0,255), COLOR_RGB(255,0,255), COLOR_RGB(0,255,255), COLOR_RGB(255,255,255),
		};
		return GR_CreateTexture3D( 2, 2, 2, TEXFMT_RGBA8,
			TEXFLAGS_LERP | TEXFLAGS_CLAMP_X | TEXFLAGS_CLAMP_Y | TEXFLAGS_CLAMP_Z, 1, data );
	}
	if( key.starts_with( "sys:rgba:" ) )
	{
		StringView it = key.after( "sys:rgba:" );
		int r = TCLAMP( (int) it.until( ":" ).parse_int(), 0, 255 );
		it = it.after( ":" );
		int g = TCLAMP( (int) it.until( ":" ).parse_int(), 0, 255 );
		it = it.after( ":" );
		int b = TCLAMP( (int) it.until( ":" ).parse_int(), 0, 255 );
		it = it.after( ":" );
		int a = it.size() ? TCLAMP( (int) it.parse_int(), 0, 255 ) : 255;
		
		uint32_t data[1] = { COLOR_RGBA( r, g, b, a ) };
		return GR_CreateTexture( 1, 1, TEXFMT_RGBA8, TEXFLAGS_LERP, 1, data );
	}
	if( key.starts_with( "sys:rgbaf:" ) )
	{
		StringView it = key.after( "sys:rgbaf:" );
		float r = it.until( ":" ).parse_float();
		it = it.after( ":" );
		float g = it.until( ":" ).parse_float();
		it = it.after( ":" );
		float b = it.until( ":" ).parse_float();
		it = it.after( ":" );
		float a = it.size() ? it.parse_float() : 1;
		
		uint32_t data[1] = { Vec4ToCol32( V4( r, g, b, a ) ) };
		return GR_CreateTexture( 1, 1, TEXFMT_RGBA8, TEXFLAGS_LERP, 1, data );
	}
	if( key.starts_with( "sys:gray:" ) )
	{
		StringView it = key.after( "sys:gray:" );
		int g = TCLAMP( (int) it.until( ":" ).parse_int(), 0, 255 );
		it = it.after( ":" );
		int a = it.size() ? TCLAMP( (int) it.parse_int(), 0, 255 ) : 255;
		
		uint32_t data[1] = { COLOR_RGBA( g, g, g, a ) };
		return GR_CreateTexture( 1, 1, TEXFMT_RGBA8, TEXFLAGS_LERP, 1, data );
	}
	if( key.starts_with( "sys:grayf:" ) )
	{
		StringView it = key.after( "sys:grayf:" );
		float g = it.until( ":" ).parse_float();
		it = it.after( ":" );
		float a = it.size() ? it.parse_float() : 1;
		
		uint32_t data[1] = { Vec4ToCol32( V4( g, g, g, a ) ) };
		return GR_CreateTexture( 1, 1, TEXFMT_RGBA8, TEXFLAGS_LERP, 1, data );
	}
	
	return NULL;
}

HFileReader OnLoadTexture( const StringView& key, uint32_t& outusageflags, uint8_t& outlod )
{
	LOG_FUNCTION;
	
	if( !key )
		return NULL;
	
	outusageflags = TEXFLAGS_HASMIPS | TEXFLAGS_LERP;
	if( key.ch() == '\0' )
		return FS_OpenBinaryFile( key );
	
	StringView path = key.until( ":", 1 );
	HFileReader out = FS_OpenBinaryFile( path );
	if( !out )
		return NULL;
	
	StringView flags = key.from( ":", 1 );
	ParseDefaultTextureFlags( flags, outusageflags, outlod );
	
	return out;
}



SGRX_ITexture::SGRX_ITexture() : m_rtkey(0)
{
}

SGRX_ITexture::~SGRX_ITexture()
{
	if( VERBOSE ) LOG << "Deleted texture: " << m_key;
	g_Textures->unset( m_key );
	if( m_rtkey )
		g_RenderTargets->unset( m_rtkey );
}

const TextureInfo& TextureHandle::GetInfo() const
{
	static TextureInfo dummy_info = {0};
	if( !item )
		return dummy_info;
	return item->m_info;
}

Vec2 TextureHandle::GetInvSize( Vec2 def ) const
{
	const TextureInfo& TI = GetInfo();
	return V2( TI.width ? 1.0f / TI.width : def.x, TI.height ? 1.0f / TI.height : def.y );
}

bool TextureHandle::UploadRGBA8Part( void* data, int mip, int w, int h, int x, int y )
{
	LOG_FUNCTION;
	
	if( !item )
		return false;
	
	const TextureInfo& TI = item->m_info;
	
	if( mip < 0 || mip >= TI.mipcount )
	{
		LOG_ERROR << "Cannot UploadRGBA8Part - mip count out of bounds (" << mip << "/" << TI.mipcount << ")";
		return false;
	}
	
	TextureInfo mti;
	if( !TI.GetMipInfo( mip, &mti ) )
	{
		LOG_ERROR << "Cannot UploadRGBA8Part - failed to get mip info (" << mip << ")";
		return false;
	}
	
	if( w < 0 ) w = mti.width;
	if( h < 0 ) h = mti.height;
	
	return item->UploadRGBA8Part( data, mip, x, y, w, h );
}

bool TextureHandle::UploadRGBA8Part3D( void* data, int mip, int w, int h, int d, int x, int y, int z )
{
	LOG_FUNCTION;
	
	if( !item )
		return false;
	
	const TextureInfo& TI = item->m_info;
	
	if( mip < 0 || mip >= TI.mipcount )
	{
		LOG_ERROR << "Cannot UploadRGBA8Part - mip count out of bounds (" << mip << "/" << TI.mipcount << ")";
		return false;
	}
	
	TextureInfo mti;
	if( !TI.GetMipInfo( mip, &mti ) )
	{
		LOG_ERROR << "Cannot UploadRGBA8Part - failed to get mip info (" << mip << ")";
		return false;
	}
	
	if( w < 0 ) w = mti.width;
	if( h < 0 ) h = mti.height;
	if( d < 0 ) d = mti.depth;
	
	return item->UploadRGBA8Part3D( data, mip, x, y, z, w, h, d );
}



int GR_CalcMipCount( int width, int height, int depth )
{
	int v = TMAX( width, TMAX( height, depth ) );
	int count = 0;
	while( v > 0 )
	{
		v /= 2;
		count++;
	}
	return count;
}

TextureHandle GR_CreateTexture( int width, int height, SGRX_TextureFormat format, uint32_t flags, int mips, const void* data )
{
	LOG_FUNCTION;
	
	TextureInfo ti = { TEXTYPE_2D, mips, width, height, 1, format, flags };
	SGRX_ITexture* tex = g_Renderer->CreateTexture( &ti, data );
	if( !tex )
	{
		// error is already printed
		return TextureHandle();
	}
	
	tex->m_info = ti;
	
	if( VERBOSE )
		LOG << "Created 2D texture: " << width << "x" << height
		<< ", format=" << format << ", mips=" << mips;
	return tex;
}

TextureHandle GR_CreateTexture3D( int width, int height, int depth, SGRX_TextureFormat format, uint32_t flags, int mips, const void* data )
{
	LOG_FUNCTION;
	
	TextureInfo ti = { TEXTYPE_VOLUME, mips, width, height, depth, format, flags };
	SGRX_ITexture* tex = g_Renderer->CreateTexture( &ti, data );
	if( !tex )
	{
		// error is already printed
		return TextureHandle();
	}
	
	tex->m_info = ti;
	
	if( VERBOSE )
		LOG << "Created 3D texture: " << width << "x" << height << "x" << depth
		<< ", format=" << format << ", mips=" << mips;
	return tex;
}

TextureHandle GR_CreateTextureFromBytes( ByteView bytes )
{
	LOG_FUNCTION;
	
	double t0 = sgrx_hqtime();
	TextureData texdata;
	memset( &texdata.info, 0, sizeof(texdata.info) );
	texdata.info.flags = TEXFLAGS_LERP;
	ByteFileReader br( bytes );
	if( !TextureData_Load( &texdata, &br, "<bytes>", 0 ) )
	{
		// error is already printed
		return TextureHandle();
	}
	
	TextureHandle tex = g_Renderer->CreateTexture( &texdata.info, texdata.data.data() );
	if( !tex )
	{
		// error is already printed
		return TextureHandle();
	}
	tex->m_info = texdata.info;
	
	if( VERBOSE )
		LOG << "Created texture from " << bytes.size() << " bytes (time=" << ( sgrx_hqtime() - t0 ) << ")";
	return tex;
}

TextureHandle GR_GetTexture( StringView path )
{
	TextureHandle tex;
	LOG_FUNCTION_ARG( path );
	
	StringView origpath = path;
	FS_RESOLVE_PATH( path );
	
	tex = g_Textures->getcopy( origpath );
	if( tex )
		return tex;
	
	double t0 = sgrx_hqtime();
	tex = OnCreateSysTexture( path );
	if( !tex )
	{
		// it's a regular texture
		uint32_t usageflags = 0;
		uint8_t lod = 0;
		HFileReader fr = OnLoadTexture( path, usageflags, lod );
		if( fr == NULL )
		{
			if( VERBOSE || path != "" )
				LOG_ERROR << LOG_DATE << "  Could not find texture: " << path;
			return TextureHandle();
		}
		
		TextureData texdata;
		memset( &texdata.info, 0, sizeof(texdata.info) );
		texdata.info.flags = usageflags;
		if( !TextureData_Load( &texdata, fr, path, lod ) )
		{
			// error is already printed
			return TextureHandle();
		}
		
		tex = g_Renderer->CreateTexture( &texdata.info, texdata.data.data() );
		if( !tex )
		{
			// error is already printed
			return TextureHandle();
		}
		tex->m_info = texdata.info;
	}
	
	tex->m_key.append( origpath.data(), origpath.size() );
	g_Textures->set( tex->m_key, tex );
	
	if( VERBOSE )
		LOG << "Loaded texture: " << path << " (time=" << ( sgrx_hqtime() - t0 ) << ")";
	return tex;
}

TextureHandle GR_CreateRenderTexture( int width, int height, SGRX_TextureFormat format, int mips )
{
	LOG_FUNCTION;
	
	if( mips == SGRX_MIPS_ALL )
		mips = GR_CalcMipCount( width, height );
	else if( mips == 0 )
		mips = 1;
	TextureInfo ti = { TEXTYPE_2D, mips, width, height, 1, format,
		TEXFLAGS_LERP | TEXFLAGS_CLAMP_X | TEXFLAGS_CLAMP_Y };
	SGRX_ITexture* tex = g_Renderer->CreateRenderTexture( &ti );
	if( !tex )
	{
		// error is already printed
		return TextureHandle();
	}
	
	tex->m_info = ti;
	
	if( VERBOSE )
	{
		LOG << "Created renderable texture: " << width << "x"
			<< height << ", format=" << format << ", mips=" << mips;
	}
	return tex;
}

TextureHandle GR_CreateCubeRenderTexture( int width, SGRX_TextureFormat format, int mips )
{
	LOG_FUNCTION;
	
	if( mips == SGRX_MIPS_ALL )
		mips = GR_CalcMipCount( width );
	else if( mips == 0 )
		mips = 1;
	TextureInfo ti = { TEXTYPE_CUBE, mips, width, width, 1, format,
		TEXFLAGS_LERP | TEXFLAGS_CLAMP_X | TEXFLAGS_CLAMP_Y };
	SGRX_ITexture* tex = g_Renderer->CreateRenderTexture( &ti );
	if( !tex )
	{
		// error is already printed
		return TextureHandle();
	}
	
	tex->m_info = ti;
	
	if( VERBOSE )
	{
		LOG << "Created renderable cube texture: " << width
			<< ", format=" << format << ", mips=" << mips;
	}
	return tex;
}

TextureHandle GR_GetRenderTarget( int width, int height, SGRX_TextureFormat format, int extra )
{
	ASSERT( width && height && format );
	LOG_FUNCTION;
	
	uint64_t key = (uint64_t(width&0xffff)<<48) | (uint64_t(height&0xffff)<<32) | (uint64_t(format&0xffff)<<16) | (uint64_t(extra&0xffff));
	SGRX_ITexture* rtt = g_RenderTargets->getcopy( key );
	if( rtt )
		return rtt;
	
	TextureHandle rtth = GR_CreateRenderTexture( width, height, format );
	rtth->m_rtkey = key;
	g_RenderTargets->set( key, rtth );
	return rtth;
}



SGRX_IDepthStencilSurface::SGRX_IDepthStencilSurface() : m_width(0), m_height(0), m_format(0), m_key(0)
{
}

SGRX_IDepthStencilSurface::~SGRX_IDepthStencilSurface()
{
	if( VERBOSE ) LOG << "Deleted depth/stencil surface: " << m_width << "x" << m_height << ", format=" << m_format;
	if( m_key )
		g_DepthStencilSurfs->unset( m_key );
}

DepthStencilSurfHandle GR_CreateDepthStencilSurface( int width, int height, SGRX_TextureFormat format )
{
	ASSERT( width && height && format );
	LOG_FUNCTION;
	
	SGRX_IDepthStencilSurface* dss = g_Renderer->CreateDepthStencilSurface( width, height, format );
	if( dss == NULL )
	{
		// valid outcome
		return NULL;
	}
	dss->m_width = width;
	dss->m_height = height;
	dss->m_format = format;
	
	if( VERBOSE ) LOG << "Created depth/stencil surface: " << width << "x" << height << ", format=" << format;
	return dss;
}

DepthStencilSurfHandle GR_GetDepthStencilSurface( int width, int height, SGRX_TextureFormat format, int extra )
{
	ASSERT( width && height && format );
	LOG_FUNCTION;
	
	uint64_t key = (uint64_t(width&0xffff)<<48) | (uint64_t(height&0xffff)<<32) | (uint64_t(format&0xffff)<<16) | (uint64_t(extra&0xffff));
	SGRX_IDepthStencilSurface* dss = g_DepthStencilSurfs->getcopy( key );
	if( dss )
		return dss;
	
	DepthStencilSurfHandle dssh = GR_CreateDepthStencilSurface( width, height, format );
	if( dssh == NULL )
	{
		// valid outcome
		return NULL;
	}
	dssh->m_key = key;
	g_DepthStencilSurfs->set( key, dssh );
	return dssh;
}

