

#define ASSIMP_IMPORTER_TYPE Assimp::Importer
#define ASSIMP_SCENE_TYPE aiScene
#define ASSIMP_NODE_TYPE aiNode
#define ASSIMP_MESH_TYPE aiMesh
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "assetcomp.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_DXT_IMPLEMENTATION
#include <stb_dxt.h>
#include <libpng/png.h>
extern "C" {
#include <dds.c>
}


SGRX_IFP32Handle SGRX_ResizeImage( SGRX_ImageFP32* image, int width, int height, int depth )
{
	if( width < 1 || width > 4096 ||
		height < 1 || height > 4096 ||
		depth < 1 || depth > 4096 )
	{
		printf( "ERROR: resize - size out of bounds" );
		return NULL;
	}
	if( image->GetWidth() == width &&
		image->GetHeight() == height &&
		image->GetDepth() == depth )
		return image;
	
	float fw = width, fh = height, fd = depth;
	float xoff = TMAX( 0.0f, image->GetWidth() / fw - 1.0f ) / ( image->GetWidth() * 2 );
	float yoff = TMAX( 0.0f, image->GetHeight() / fh - 1.0f ) / ( image->GetHeight() * 2 );
	float zoff = TMAX( 0.0f, image->GetDepth() / fh - 1.0f ) / ( image->GetDepth() * 2 );
	SGRX_IFP32Handle out = new SGRX_ImageFP32( width, height, depth, image->GetSides() );
	for( int s = 0; s < image->GetSides(); ++s )
	{
		if( image->GetDepth() == 1 )
		{
			// lerp into slice 1
			for( int y = 0; y < height; ++y )
			{
				for( int x = 0; x < width; ++x )
				{
					out->Pixel( x, y, 0, s ) = image->GetLerpXY(
						xoff + x / fw,
						yoff + y / fh,
						s );
				}
			}
			// copy to other slices if they exist
			for( int z = 1; z < depth; ++z )
			{
				for( int y = 0; y < height; ++y )
				{
					for( int x = 0; x < width; ++x )
					{
						out->Pixel( x, y, z, s ) = out->Pixel( x, y, 0, s );
					}
				}
			}
		}
		else
		{
			for( int z = 0; z < depth; ++z )
			{
				for( int y = 0; y < height; ++y )
				{
					for( int x = 0; x < width; ++x )
					{
						out->Pixel( x, y, z, s ) = image->GetLerpXYZ(
							xoff + x / fw,
							yoff + y / fh,
							zoff + z / fd,
							s );
					}
				}
			}
		}
	}
	return out;
}

SGRX_IFP32Handle SGRX_ImagePower( SGRX_ImageFP32* image, float power )
{
	SGRX_IFP32Handle out = image->CreateUninitializedCopy();
	for( size_t i = 0; i < image->Size(); ++i )
	{
		Vec4 col =
		{
			powf( (*image)[ i ].x, power ),
			powf( (*image)[ i ].y, power ),
			powf( (*image)[ i ].z, power ),
			powf( (*image)[ i ].w, power ),
		};
		(*out)[ i ] = col;
	}
	return out;
}


static const char* assetimgfiltype_string_table[] =
{
	"resize",
	"rearrange",
	"sharpen",
	"to_linear",
	"from_linear",
	"expand_range",
	"bcp",
};

const char* SGRX_AssetImgFilterType_ToString( SGRX_AssetImageFilterType aift )
{
	int fid = aift;
	if( fid <= 0 || fid >= SGRX_AIF__COUNT )
		return "Unknown";
	return assetimgfiltype_string_table[ fid - 1 ];
}

SGRX_AssetImageFilterType SGRX_AssetImgFilterType_FromString( const StringView& sv )
{
	for( int i = 1; i < SGRX_AIF__COUNT; ++i )
	{
		if( sv == assetimgfiltype_string_table[ i - 1 ] )
			return (SGRX_AssetImageFilterType) i;
	}
	return SGRX_AIF_Unknown;
}

bool SGRX_ImageFilter::ParseCMFParam( StringView key, StringView value )
{
	if( key == "CMF_COLORS" )
	{
		colors = String_ParseInt( value ) & 0xf;
		return true;
	}
	if( key == "CMF_BLEND" )
	{
		blend = String_ParseFloat( value );
		return true;
	}
	if( key == "CMF_CLAMP" )
	{
		cclamp = String_ParseBool( value );
		return true;
	}
	return false;
}

void SGRX_ImageFilter::GenerateCMFParams( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256,
		"  CMF_BLEND %g\n"
		"  CMF_CLAMP %s\n"
		"  CMF_COLORS 0x%x\n",
		blend, cclamp ? "true" : "false", int(colors) );
	out.append( bfr );
}

void SGRX_ImageFilter::CMFBlend( SGRX_ImageFP32* src, SGRX_ImageFP32* dst )
{
	if( src->GetWidth() != dst->GetWidth() || src->GetHeight() != dst->GetHeight() )
		return;
	for( size_t i = 0; i < src->Size(); ++i )
	{
		Vec4 srccol = (*src)[ i ];
		Vec4& dstcol = (*dst)[ i ];
		dstcol = TLERP( srccol, dstcol, blend );
		if( cclamp )
		{
			dstcol.x = clamp( dstcol.x, 0, 1 );
			dstcol.y = clamp( dstcol.y, 0, 1 );
			dstcol.z = clamp( dstcol.z, 0, 1 );
			dstcol.w = clamp( dstcol.w, 0, 1 );
		}
		if( ~colors & 0x1 ) dstcol.x = srccol.x;
		if( ~colors & 0x2 ) dstcol.y = srccol.y;
		if( ~colors & 0x4 ) dstcol.z = srccol.z;
		if( ~colors & 0x8 ) dstcol.w = srccol.w;
	}
}

bool SGRX_ImageFilter_Resize::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "WIDTH" )
			width = String_ParseInt( value );
		else if( key == "HEIGHT" )
			height = String_ParseInt( value );
		else if( key == "DEPTH" )
			depth = String_ParseInt( value );
		else if( key == "SRGB" )
			srgb = String_ParseBool( value );
		else if( key == "FILTER_END" )
			return true;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/ImgFilter(resize) command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/ImgFilter(resize) data";
	return false;
}

void SGRX_ImageFilter_Resize::Generate( String& out )
{
	char bfr[ 128 ];
	sgrx_snprintf( bfr, 128,
		"  WIDTH %d\n"
		"  HEIGHT %d\n"
		"  DEPTH %d\n"
		"  SRGB %s\n",
		width, height, depth, srgb ? "true" : "false" );
	out.append( bfr );
}

SGRX_IFP32Handle SGRX_ImageFilter_Resize::Process( SGRX_ImageFP32* image, SGRX_ImageFilterState& ifs )
{
	SGRX_IFP32Handle out = image;
	if( srgb )
		out = SGRX_ImagePower( out, 2.2f );
	out = SGRX_ResizeImage( out, width, height, depth );
	if( srgb )
		out = SGRX_ImagePower( out, 1.0f / 2.2f );
	return out;
}

static const char* imgfltrearrange_string_table[] =
{
	"SlicesToVolume",
	"TurnCubemapYZ",
	"BlenderCubemap",
};

const char* SGRX_ImgFltRearrange_ToString( SGRX_ImgFltRearrange_Mode ifrm )
{
	int fid = ifrm;
	if( fid < 0 || fid >= SGRX_IFR__COUNT )
		fid = 0;
	return imgfltrearrange_string_table[ fid ];
}

SGRX_ImgFltRearrange_Mode SGRX_ImgFltRearrange_FromString( const StringView& sv )
{
	for( int i = 0; i < SGRX_IFR__COUNT; ++i )
	{
		if( sv == imgfltrearrange_string_table[ i ] )
			return (SGRX_ImgFltRearrange_Mode) i;
	}
	return SGRX_IFR_SlicesToVolume;
}

bool SGRX_ImageFilter_Rearrange::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "MODE" )
			mode = SGRX_ImgFltRearrange_FromString( value );
		else if( key == "WIDTH" )
			width = String_ParseInt( value );
		else if( key == "FILTER_END" )
			return true;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/ImgFilter(resize) command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/ImgFilter(resize) data";
	return false;
}

void SGRX_ImageFilter_Rearrange::Generate( String& out )
{
	char bfr[ 128 ];
	sgrx_snprintf( bfr, 128,
		"  MODE %s\n"
		"  WIDTH %d\n",
		SGRX_ImgFltRearrange_ToString( mode ),
		width );
	out.append( bfr );
}

SGRX_IFP32Handle SGRX_ImageFilter_Rearrange::Process( SGRX_ImageFP32* image, SGRX_ImageFilterState& ifs )
{
	SGRX_IFP32Handle out = image;
	if( mode == SGRX_IFR_SlicesToVolume )
	{
		int numslices = image->GetWidth() / width;
		printf( "%d slices; ", numslices );
		if( numslices >= 1 )
		{
			out = new SGRX_ImageFP32( width, image->GetHeight(), numslices, 1 );
			for( int i = 0; i < numslices; ++i )
			{
				for( int y = 0; y < image->GetHeight(); ++y )
				{
					for( int x = 0; x < width; ++x )
					{
						out->Pixel( x, y, i, 0 ) = image->Pixel( x + i * width, y, 0, 0 );
					}
				}
			}
		}
	}
	else if( mode == SGRX_IFR_TurnCubemapYZ )
	{
		if( image->GetSides() == 6 &&
			image->GetWidth() == image->GetHeight() &&
			image->GetDepth() <= 1 )
		{
			out = image->CreateUninitializedCopy();
			int width = image->GetWidth();
			static int sliceremap[6] = {0,1,5,4,2,3};
			for( int dstslice = 0; dstslice < 6; ++dstslice )
			{
				int srcslice = sliceremap[ dstslice ];
				switch( dstslice )
				{
				case 0: // +X -- rotate by 90 degrees (dir.1)
					for( int y = 0; y < width; ++y )
						for( int x = 0; x < width; ++x )
							out->Pixel( x, y, 0, dstslice ) =
								image->Pixel( width - 1 - y, x, 0, srcslice );
					break;
				case 1: // -X -- rotate by 90 degrees (dir.2)
					for( int y = 0; y < width; ++y )
						for( int x = 0; x < width; ++x )
							out->Pixel( x, y, 0, dstslice ) =
								image->Pixel( y, width - 1 - x, 0, srcslice );
					break;
				case 2: case 5: // +Y, -Z -- rotate by 180 degrees (flip on both axes)
					for( int y = 0; y < width; ++y )
						for( int x = 0; x < width; ++x )
							out->Pixel( x, y, 0, dstslice ) =
								image->Pixel( width - 1 - x, width - 1 - y, 0, srcslice );
					break;
				default:
					for( int y = 0; y < width; ++y )
						for( int x = 0; x < width; ++x )
							out->Pixel( x, y, 0, dstslice ) = image->Pixel( x, y, 0, srcslice );
					break;
				}
			}
		}
	}
	else if( mode == SGRX_IFR_BlenderCubemap )
	{
		if( image->GetSides() == 1 &&
			image->GetWidth() * 2 == image->GetHeight() * 3 &&
			image->GetDepth() <= 1 )
		{
			int width = image->GetWidth() / 3;
			out = image->CreateUninitializedCopy();
			out->Resize( width, width, 1, 6 );
			static int sliceremap[6][2] =
			{
				{2,0}, {0,0},
				{1,0}, {2,1},
				{1,1}, {0,1},
			};
			for( int dstslice = 0; dstslice < 6; ++dstslice )
			{
				int sxo = sliceremap[ dstslice ][0] * width;
				int syo = sliceremap[ dstslice ][1] * width;
				switch( dstslice )
				{
				case 0: // +X -- rotate by 90 degrees (dir.1)
					for( int y = 0; y < width; ++y )
						for( int x = 0; x < width; ++x )
							out->Pixel( x, y, 0, dstslice ) =
								image->Pixel( sxo + width - 1 - y, syo + x, 0, 0 );
					break;
				case 1: // -X -- rotate by 90 degrees (dir.2)
					for( int y = 0; y < width; ++y )
						for( int x = 0; x < width; ++x )
							out->Pixel( x, y, 0, dstslice ) =
								image->Pixel( sxo + y, syo + width - 1 - x, 0, 0 );
					break;
				case 2: case 5: // +Y, -Z -- rotate by 180 degrees (flip on both axes)
					for( int y = 0; y < width; ++y )
						for( int x = 0; x < width; ++x )
							out->Pixel( x, y, 0, dstslice ) =
								image->Pixel( sxo + width - 1 - x, syo + width - 1 - y, 0, 0 );
					break;
				default:
					for( int y = 0; y < width; ++y )
						for( int x = 0; x < width; ++x )
							out->Pixel( x, y, 0, dstslice ) =
								image->Pixel( sxo + x, syo + y, 0, 0 );
					break;
				}
			}
		}
	}
	return out;
}

static const char* imgfltsharpen_string_table[] =
{
	"0-1",
	"1-1",
	"1-2",
};

const char* SGRX_ImgFltSharpen_ToString( SGRX_ImgFltSharpen_Mode ifsm )
{
	int fid = ifsm;
	if( fid < 0 || fid >= SGRX_IFS__COUNT )
		return "0-1";
	return imgfltsharpen_string_table[ fid ];
}

SGRX_ImgFltSharpen_Mode SGRX_ImgFltSharpen_FromString( const StringView& sv )
{
	for( int i = 0; i < SGRX_IFS__COUNT; ++i )
	{
		if( sv == imgfltsharpen_string_table[ i ] )
			return (SGRX_ImgFltSharpen_Mode) i;
	}
	return SGRX_IFS_0_1;
}

bool SGRX_ImageFilter_Sharpen::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "FACTOR" )
			factor = String_ParseFloat( value );
		else if( key == "MODE" )
			mode = SGRX_ImgFltSharpen_FromString( value );
		else if( key == "FILTER_END" )
			return true;
		else if( ParseCMFParam( key, value ) )
			continue;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/ImgFilter(resize) command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/ImgFilter(sharpen) data";
	return false;
}

void SGRX_ImageFilter_Sharpen::Generate( String& out )
{
	char bfr[ 128 ];
	sgrx_snprintf( bfr, 128,
		"  FACTOR %g\n"
		"  MODE %s\n",
		factor,
		SGRX_ImgFltSharpen_ToString( mode ) );
	out.append( bfr );
	GenerateCMFParams( out );
}

template< int w, int h, int d >
SGRX_IFP32Handle SGRX_ImageConvolutionFilter(
	SGRX_ImageFP32* image, float* kernel )
{
	SGRX_IFP32Handle out = image->CreateUninitializedCopy();
	int hw = w / 2, hh = h / 2, hd = d / 2;
	for( int s = 0; s < image->GetSides(); ++s )
	{
		for( int z = 0; z < image->GetDepth(); ++z )
		{
			for( int y = 0; y < image->GetHeight(); ++y )
			{
				for( int x = 0; x < image->GetWidth(); ++x )
				{
					Vec4 col = V4(0);
					for( int kz = 0; kz < d; ++kz )
					{
						for( int ky = 0; ky < h; ++ky )
						{
							for( int kx = 0; kx < w; ++kx )
							{
								col += image->GetClamped( x - hw + kx, y - hh + ky, z - hd + kz, s )
									* kernel[ kx + ky * w + kz * w * h ];
							}
						}
					}
					out->Pixel( x, y, z, s ) = col;
				}
			}
		}
	}
	return out;
}

SGRX_IFP32Handle SGRX_ImageFilter_Sharpen::Process( SGRX_ImageFP32* image, SGRX_ImageFilterState& ifs )
{
	if( factor == 0 )
		return image;
	
	float a = 0, b = -1, c = 4;
	switch( mode )
	{
	default:
	case SGRX_IFS_0_1: a = 0; b = -1; c = 4; break;
	case SGRX_IFS_1_1: a = -1; b = -1; c = 8; break;
	case SGRX_IFS_1_2: a = -1; b = -2; c = 12; break;
	}
	a *= factor;
	b *= factor;
	c *= factor;
	c += 1;
	float filter[9] =
	{
		a, b, a,
		b, c, b,
		a, b, a,
	};
	SGRX_IFP32Handle dst = SGRX_ImageConvolutionFilter<3,3,1>( image, filter );
	CMFBlend( image, dst );
	return dst;
}

bool SGRX_ImageFilter_Linear::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "FILTER_END" )
			return true;
		else if( ParseCMFParam( key, value ) )
			continue;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/ImgFilter(resize) command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/ImgFilter("
		<< ( inverse ? "from_linear" : "to_linear" ) << ") data";
	return false;
}

void SGRX_ImageFilter_Linear::Generate( String& out )
{
	GenerateCMFParams( out );
}

SGRX_IFP32Handle SGRX_ImageFilter_Linear::Process( SGRX_ImageFP32* image, SGRX_ImageFilterState& ifs )
{
	if( ifs.isSRGB == false )
		return image;
	SGRX_IFP32Handle out = SGRX_ImagePower( image, inverse ? 1.0f / 2.2f : 2.2f );
	CMFBlend( image, out );
	return out;
}


bool SGRX_ImageFilter_ExpandRange::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "MIN" )
			vmin = String_ParseVec4( value );
		else if( key == "MAX" )
			vmax = String_ParseVec4( value );
		else if( key == "FILTER_END" )
			return true;
		else if( ParseCMFParam( key, value ) )
			continue;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/ImgFilter(expand_range) command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/ImgFilter(expand_range) data";
	return false;
}

void SGRX_ImageFilter_ExpandRange::Generate( String& out )
{
	char bfr[ 2048 ];
	sgrx_snprintf( bfr, 2048,
		"  MIN %g;%g;%g;%g\n"
		"  MAX %g;%g;%g;%g\n",
		vmin.x, vmin.y, vmin.z, vmin.w,
		vmax.x, vmax.y, vmax.z, vmax.w );
	out.append( bfr );
	GenerateCMFParams( out );
}

SGRX_IFP32Handle SGRX_ImageFilter_ExpandRange::Process( SGRX_ImageFP32* image, SGRX_ImageFilterState& ifs )
{
	Vec4 cmin = V4( HSV( vmin.ToVec3() ), vmin.w );
	Vec4 cmax = V4( HSV( vmax.ToVec3() ), vmax.w );
	Vec4 diff = cmax - cmin;
	if( diff.x == 0 || diff.y == 0 || diff.z == 0 || diff.w == 0 )
		return image;
	SGRX_IFP32Handle out = image->CreateUninitializedCopy();
	for( size_t i = 0; i < image->Size(); ++i )
	{
		Vec4 col =
		{
			( (*image)[ i ].x - cmin.x ) / diff.x,
			( (*image)[ i ].y - cmin.y ) / diff.y,
			( (*image)[ i ].z - cmin.z ) / diff.z,
			( (*image)[ i ].w - cmin.w ) / diff.w,
		};
		(*out)[ i ] = col;
	}
	CMFBlend( image, out );
	return out;
}


bool SGRX_ImageFilter_BCP::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "APPLY_BC1" )
			apply_bc1 = String_ParseBool( value );
		else if( key == "BRIGHTNESS" )
			brightness = String_ParseFloat( value );
		else if( key == "CONTRAST" )
			contrast = String_ParseFloat( value );
		else if( key == "APPLY_POW" )
			apply_pow = String_ParseBool( value );
		else if( key == "POWER" )
			power = String_ParseFloat( value );
		else if( key == "APPLY_BC2" )
			apply_bc2 = String_ParseBool( value );
		else if( key == "BRIGHTNESS_2" )
			brightness_2 = String_ParseFloat( value );
		else if( key == "CONTRAST_2" )
			contrast_2 = String_ParseFloat( value );
		else if( key == "FILTER_END" )
			return true;
		else if( ParseCMFParam( key, value ) )
			continue;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/ImgFilter(expand_range) command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/ImgFilter(expand_range) data";
	return false;
}

void SGRX_ImageFilter_BCP::Generate( String& out )
{
	char bfr[ 4096 ];
	sgrx_snprintf( bfr, 4096,
		"  APPLY_BC1 %s\n"
		"  BRIGHTNESS %g\n"
		"  CONTRAST %g\n"
		"  APPLY_POW %s\n"
		"  POWER %g\n"
		"  APPLY_BC2 %s\n"
		"  BRIGHTNESS_2 %g\n"
		"  CONTRAST_2 %g\n",
		apply_bc1 ? "true" : "false", brightness, contrast,
		apply_pow ? "true" : "false", power,
		apply_bc2 ? "true" : "false", brightness_2, contrast_2 );
	out.append( bfr );
	GenerateCMFParams( out );
}

SGRX_IFP32Handle SGRX_ImageFilter_BCP::Process( SGRX_ImageFP32* image, SGRX_ImageFilterState& ifs )
{
	SGRX_IFP32Handle out = image->CreateUninitializedCopy();
	for( size_t i = 0; i < image->Size(); ++i )
		(*out)[ i ] = (*image)[ i ];
	if( apply_bc1 )
	{
		for( size_t i = 0; i < image->Size(); ++i )
			(*out)[ i ] = (*out)[ i ] * V4(contrast) + V4(brightness);
	}
	if( apply_pow )
	{
		for( size_t i = 0; i < image->Size(); ++i )
		{
			Vec4 col = (*out)[ i ];
			(*out)[ i ] = V4
			(
				pow( fabsf( col.x ), power ) * sign( col.x ),
				pow( fabsf( col.y ), power ) * sign( col.y ),
				pow( fabsf( col.z ), power ) * sign( col.z ),
				pow( fabsf( col.w ), power ) * sign( col.w )
			);
		}
	}
	if( apply_bc2 )
	{
		for( size_t i = 0; i < image->Size(); ++i )
			(*out)[ i ] = (*out)[ i ] * V4(contrast_2) + V4(brightness_2);
	}
	CMFBlend( image, out );
	return out;
}


static const char* texoutfmt_string_table[] =
{
	"PNG/RGBA32",
	"STX/RGBA32",
	"STX/DXT1",
	"STX/DXT5",
	"STX/3DC",
};
static const char* texoutfmt_ext_string_table[] =
{
	"png",
	"stx",
	"stx",
	"stx",
	"stx",
};

const char* SGRX_TextureOutputFormat_ToString( SGRX_TextureOutputFormat fmt )
{
	int fid = fmt;
	if( fid <= 0 || fid >= SGRX_TOF__COUNT )
		return "Unknown";
	return texoutfmt_string_table[ fid - 1 ];
}

SGRX_TextureOutputFormat SGRX_TextureOutputFormat_FromString( const StringView& sv )
{
	for( int i = 1; i < SGRX_TOF__COUNT; ++i )
	{
		if( sv == texoutfmt_string_table[ i - 1 ] )
			return (SGRX_TextureOutputFormat) i;
	}
	return SGRX_TOF_Unknown;
}

const char* SGRX_TextureOutputFormat_Ext( SGRX_TextureOutputFormat fmt )
{
	int fid = fmt;
	if( fid <= 0 || fid >= SGRX_TOF__COUNT )
		return "";
	return texoutfmt_ext_string_table[ fid - 1 ];
}

SGRX_TextureAsset::SGRX_TextureAsset() :
	SGRX_Asset( SGRX_AT_Texture ),
	outputType(SGRX_TOF_PNG_RGBA32),
	isSRGB(true), mips(true), lerp(true), clampx(false), clampy(false)
{
}

SGRX_Asset* SGRX_TextureAsset::Clone()
{
	SGRX_TextureAsset* TA = new SGRX_TextureAsset( *this );
	for( size_t i = 0; i < TA->filters.size(); ++i )
	{
		TA->filters[ i ] = TA->filters[ i ]->Clone();
	}
	return TA;
}

bool SGRX_TextureAsset::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "GUID" )
			assetGUID = SGRX_GUID::ParseString( value );
		else if( key == "SOURCE" )
			sourceFile = value;
		else if( key == "OUTPUT_CATEGORY" )
			outputCategory = value;
		else if( key == "OUTPUT_NAME" )
			outputName = value;
		else if( key == "OUTPUT_TYPE" )
		{
			outputType = SGRX_TextureOutputFormat_FromString( value );
			if( outputType == SGRX_TOF_Unknown )
			{
				LOG_WARNING << "Unknown texture output type: " << value << ", changed to PNG/RGBA32";
				outputType = SGRX_TOF_PNG_RGBA32;
			}
		}
		else if( key == "IS_SRGB" )
			isSRGB = String_ParseBool( value );
		else if( key == "MIPS" )
			mips = String_ParseBool( value );
		else if( key == "LERP" )
			lerp = String_ParseBool( value );
		else if( key == "CLAMPX" )
			clampx = String_ParseBool( value );
		else if( key == "CLAMPY" )
			clampy = String_ParseBool( value );
		else if( key == "FILTER" )
		{
			SGRX_ImgFilterHandle IF;
			if( value == "resize" )
				IF = new SGRX_ImageFilter_Resize;
			else if( value == "rearrange" )
				IF = new SGRX_ImageFilter_Rearrange;
			else if( value == "sharpen" )
				IF = new SGRX_ImageFilter_Sharpen;
			else if( value == "to_linear" )
				IF = new SGRX_ImageFilter_Linear( false );
			else if( value == "from_linear" )
				IF = new SGRX_ImageFilter_Linear( true );
			else if( value == "expand_range" )
				IF = new SGRX_ImageFilter_ExpandRange;
			else if( value == "bcp" )
				IF = new SGRX_ImageFilter_BCP;
			else
			{
				LOG_ERROR << "Unrecognized ImgFilter: " << value;
				return false;
			}
			if( IF->Parse( cread ) == false )
				return false;
			filters.push_back( IF );
		}
		else if( key == "TEXTURE_END" )
			return true;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/Texture command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/Texture data";
	return false;
}

void SGRX_TextureAsset::Generate( String& out )
{
	out.append( "TEXTURE\n" );
	out.append( " GUID " ); out.append( assetGUID.ToString() ); out.append( "\n" );
	out.append( " SOURCE " ); out.append( sourceFile ); out.append( "\n" );
	out.append( " OUTPUT_CATEGORY " ); out.append( outputCategory ); out.append( "\n" );
	out.append( " OUTPUT_NAME " ); out.append( outputName ); out.append( "\n" );
	out.append( " OUTPUT_TYPE " );
	out.append( SGRX_TextureOutputFormat_ToString( outputType ) ); out.append( "\n" );
	out.append( " IS_SRGB " ); out.append( isSRGB ? "true" : "false" ); out.append( "\n" );
	out.append( " MIPS " ); out.append( mips ? "true" : "false" ); out.append( "\n" );
	out.append( " LERP " ); out.append( lerp ? "true" : "false" ); out.append( "\n" );
	out.append( " CLAMPX " ); out.append( clampx ? "true" : "false" ); out.append( "\n" );
	out.append( " CLAMPY " ); out.append( clampy ? "true" : "false" ); out.append( "\n" );
	for( size_t i = 0; i < filters.size(); ++i )
	{
		out.append( " FILTER " );
		out.append( filters[ i ]->GetName() );
		out.append( "\n" );
		filters[ i ]->Generate( out );
		out.append( " FILTER_END\n" );
	}
	out.append( "TEXTURE_END\n" );
}

void SGRX_TextureAsset::GetFullName( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%s/%s",
		StackString<100>(outputCategory).str,
		StackString<100>(outputName).str );
	out = bfr;
}

void SGRX_TextureAsset::GetDesc( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%s/%s [%s]",
		StackString<100>(outputCategory).str,
		StackString<100>(outputName).str,
		SGRX_TextureOutputFormat_ToString(outputType) );
	out = bfr;
}


bool SGRX_MeshAssetPart::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "MESH_NAME" )
			meshName = value;
		else if( key == "SHADER" )
			shader = value;
		else if( key == "TEXTURE0" ) textures[ 0 ] = value;
		else if( key == "TEXTURE1" ) textures[ 1 ] = value;
		else if( key == "TEXTURE2" ) textures[ 2 ] = value;
		else if( key == "TEXTURE3" ) textures[ 3 ] = value;
		else if( key == "TEXTURE4" ) textures[ 4 ] = value;
		else if( key == "TEXTURE5" ) textures[ 5 ] = value;
		else if( key == "TEXTURE6" ) textures[ 6 ] = value;
		else if( key == "TEXTURE7" ) textures[ 7 ] = value;
		else if( key == "MTL_FLAGS" )
			mtlFlags = String_ParseInt( value );
		else if( key == "MTL_BLENDMODE" )
			mtlBlendMode = String_ParseInt( value );
		else if( key == "OPT_TRANSFORM" )
			optTransform = String_ParseInt( value );
		else if( key == "PART_END" )
			return true;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/Mesh/Part command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/Mesh/Part data";
	return false;
}

void SGRX_MeshAssetPart::Generate( String& out )
{
	out.append( " PART\n" );
	out.append( "  MESH_NAME " ); out.append( meshName ); out.append( "\n" );
	out.append( "  SHADER " ); out.append( shader ); out.append( "\n" );
	out.append( "  TEXTURE0 " ); out.append( textures[ 0 ] ); out.append( "\n" );
	out.append( "  TEXTURE1 " ); out.append( textures[ 1 ] ); out.append( "\n" );
	out.append( "  TEXTURE2 " ); out.append( textures[ 2 ] ); out.append( "\n" );
	out.append( "  TEXTURE3 " ); out.append( textures[ 3 ] ); out.append( "\n" );
	out.append( "  TEXTURE4 " ); out.append( textures[ 4 ] ); out.append( "\n" );
	out.append( "  TEXTURE5 " ); out.append( textures[ 5 ] ); out.append( "\n" );
	out.append( "  TEXTURE6 " ); out.append( textures[ 6 ] ); out.append( "\n" );
	out.append( "  TEXTURE7 " ); out.append( textures[ 7 ] ); out.append( "\n" );
	char bfr[ 128 ];
	sgrx_snprintf( bfr, 128,
		"  MTL_FLAGS %d\n"
		"  MTL_BLENDMODE %d\n"
		"  OPT_TRANSFORM %d\n",
		int(mtlFlags),
		int(mtlBlendMode),
		int(optTransform) );
	out.append( bfr );
	out.append( " PART_END\n" );
}

void SGRX_MeshAssetPart::GetDesc( int i, String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%d: %s", i, StackString<200>(meshName).str );
	out = bfr;
}

SGRX_Asset* SGRX_MeshAsset::Clone()
{
	SGRX_MeshAsset* MA = new SGRX_MeshAsset( *this );
	for( size_t i = 0; i < MA->parts.size(); ++i )
	{
		MA->parts[ i ] = new SGRX_MeshAssetPart( *MA->parts[ i ] );
	}
	return MA;
}

bool SGRX_MeshAsset::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "GUID" )
			assetGUID = SGRX_GUID::ParseString( value );
		else if( key == "SOURCE" )
			sourceFile = value;
		else if( key == "OUTPUT_CATEGORY" )
			outputCategory = value;
		else if( key == "OUTPUT_NAME" )
			outputName = value;
		else if( key == "ROTATE_Y2Z" )
			rotateY2Z = String_ParseBool( value );
		else if( key == "FLIP_UVY" )
			flipUVY = String_ParseBool( value );
		else if( key == "TRANSFORM" )
			transform = String_ParseBool( value );
		else if( key == "PART" )
		{
			SGRX_MeshAPHandle mph = new SGRX_MeshAssetPart;
			if( mph->Parse( cread ) == false )
				return false;
			parts.push_back( mph );
		}
		else if( key == "MESH_END" )
			return true;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/Mesh command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/Mesh data";
	return false;
}

void SGRX_MeshAsset::Generate( String& out )
{
	out.append( "MESH\n" );
	out.append( " GUID " ); out.append( assetGUID.ToString() ); out.append( "\n" );
	out.append( " SOURCE " ); out.append( sourceFile ); out.append( "\n" );
	out.append( " OUTPUT_CATEGORY " ); out.append( outputCategory ); out.append( "\n" );
	out.append( " OUTPUT_NAME " ); out.append( outputName ); out.append( "\n" );
	out.append( " ROTATE_Y2Z " ); out.append( rotateY2Z ? "true" : "false" ); out.append( "\n" );
	out.append( " FLIP_UVY " ); out.append( flipUVY ? "true" : "false" ); out.append( "\n" );
	out.append( " TRANSFORM " ); out.append( transform ? "true" : "false" ); out.append( "\n" );
	for( size_t i = 0; i < parts.size(); ++i )
		parts[ i ]->Generate( out );
	out.append( "MESH_END\n" );
}

void SGRX_MeshAsset::GetFullName( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%s/%s",
		StackString<100>(outputCategory).str,
		StackString<100>(outputName).str );
	out = bfr;
}

void SGRX_MeshAsset::GetDesc( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%s/%s",
		StackString<100>(outputCategory).str,
		StackString<100>(outputName).str );
	out = bfr;
}

void SGRX_ABAnimSource::GetDesc( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%s (prefix:%s)",
		file.size() ? StackString<100>(file).str : "<no file>",
		prefix.size() ? StackString<100>(prefix).str : "<none>" );
	out = bfr;
}

void SGRX_ABAnimation::GetDesc( String& out )
{
	char startbfr[ 32 ] = "~";
	char endbfr[ 32 ] = "~";
	if( startFrame != -1 )
		sgrx_snprintf( startbfr, 32, "%d", startFrame );
	if( endFrame != -1 )
		sgrx_snprintf( endbfr, 32, "%d", endFrame );
	char bfr[ 512 ];
	StringView src = SV(source).after_last(":");
	if( !src )
		src = source;
	sgrx_snprintf( bfr, 512, "%s (nm.ovr:%s, start:%s, end:%s)",
		src.size() ? StackString<100>(src).str : "<unspecified>",
		name.size() ? StackString<100>(name).str : "<none>",
		startbfr, endbfr );
	out = bfr;
}

SGRX_Asset* SGRX_AnimBundleAsset::Clone()
{
	return new SGRX_AnimBundleAsset( *this );
}

bool SGRX_AnimBundleAsset::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "GUID" )
			assetGUID = SGRX_GUID::ParseString( value );
		else if( key == "OUTPUT_CATEGORY" )
			outputCategory = value;
		else if( key == "OUTPUT_NAME" )
			outputName = value;
		else if( key == "BUNDLE_PREFIX" )
			bundlePrefix = value;
		else if( key == "PREVIEW_MESH" )
			previewMesh = value;
		else if( key == "ANIM_BUNDLE_END" )
			return true;
		else if( key == "SOURCE" )
		{
			SGRX_ABAnimSource src = { value.str(), "" };
			sources.push_back( src );
		}
		else if( key == "SRC_PREFIX" )
		{
			if( sources.size() == 0 )
			{
				LOG_ERROR << "SRC_PREFIX requires at least one SOURCE before it";
				return false;
			}
			sources.last().prefix = value;
		}
		else if( key == "ANIM" )
		{
			SGRX_ABAnimation anim;
			anim.source = value;
			anims.push_back( anim );
		}
		else if( key == "ANIM_NAME" || key == "ANIM_START_FRAME" || key == "ANIM_END_FRAME" )
		{
			if( anims.size() == 0 )
			{
				LOG_ERROR << "SRC_PREFIX requires at least one SOURCE before it";
				return false;
			}
			if( key == "ANIM_NAME" )
				anims.last().name = value;
			else if( key == "ANIM_START_FRAME" )
				anims.last().startFrame = value.size() ? String_ParseInt( value ) : -1;
			else if( key == "ANIM_END_FRAME" )
				anims.last().endFrame = value.size() ? String_ParseInt( value ) : -1;
		}
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/Mesh command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/Mesh data";
	return false;
}

void SGRX_AnimBundleAsset::Generate( String& out )
{
	out.append( "ANIM_BUNDLE\n" );
	out.append( " GUID " ); out.append( assetGUID.ToString() ); out.append( "\n" );
	out.append( " OUTPUT_CATEGORY " ); out.append( outputCategory ); out.append( "\n" );
	out.append( " OUTPUT_NAME " ); out.append( outputName ); out.append( "\n" );
	out.append( " BUNDLE_PREFIX " ); out.append( bundlePrefix ); out.append( "\n" );
	out.append( " PREVIEW_MESH " ); out.append( previewMesh ); out.append( "\n" );
	for( size_t i = 0; i < sources.size(); ++i )
	{
		out.append( " SOURCE " );
		out.append( sources[ i ].file );
		out.append( "\n SRC_PREFIX " );
		out.append( sources[ i ].prefix );
		out.append( "\n" );
	}
	for( size_t i = 0; i < anims.size(); ++i )
	{
		char bfr[ 32 ];
		out.append( " ANIM " );
		out.append( anims[ i ].source );
		out.append( "\n ANIM_NAME " );
		out.append( anims[ i ].name );
		out.append( "\n ANIM_START_FRAME " );
		if( anims[ i ].startFrame != -1 )
		{
			sgrx_snprintf( bfr, 32, "%d", anims[ i ].startFrame );
			out.append( bfr );
		}
		out.append( "\n ANIM_END_FRAME " );
		if( anims[ i ].endFrame != -1 )
		{
			sgrx_snprintf( bfr, 32, "%d", anims[ i ].endFrame );
			out.append( bfr );
		}
		out.append( "\n" );
	}
	out.append( "ANIM_BUNDLE_END\n" );
}

void SGRX_AnimBundleAsset::GetFullName( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%s/%s",
		StackString<100>(outputCategory).str,
		StackString<100>(outputName).str );
	out = bfr;
}

void SGRX_AnimBundleAsset::GetDesc( String& out )
{
	char bfr[ 512 ];
	sgrx_snprintf( bfr, 512, "%s/%s (bundle prefix:%s)",
		StackString<100>(outputCategory).str,
		StackString<100>(outputName).str,
		bundlePrefix.size() ? StackString<100>(bundlePrefix) : "<none>" );
	out = bfr;
}

uint32_t SGRX_AnimBundleAsset::LastSourceModTime()
{
	uint32_t t = 0;
	for( size_t i = 0; i < sources.size(); ++i )
		t = TMAX( t, FS_FileModTime( sources[ i ].file ) );
	return t;
}


SGRX_Asset* SGRX_FileAsset::Clone()
{
	return new SGRX_FileAsset( *this );
}

bool SGRX_FileAsset::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "GUID" )
			assetGUID = SGRX_GUID::ParseString( value );
		else if( key == "SOURCE" )
			sourceFile = value;
		else if( key == "OUTPUT_CATEGORY" )
			outputCategory = value;
		else if( key == "OUTPUT_NAME" )
			outputName = value;
		else if( key == "FILE_END" )
			return true;
		else
		{
			LOG_ERROR << "Unrecognized AssetScript/File command: " << key << "=" << value;
			return false;
		}
	}
	LOG_ERROR << "Incomplete AssetScript/File data";
	return false;
}

void SGRX_FileAsset::Generate( String& out )
{
	out.append( "FILE\n" );
	out.append( " GUID " ); out.append( assetGUID.ToString() ); out.append( "\n" );
	out.append( " SOURCE " ); out.append( sourceFile ); out.append( "\n" );
	out.append( " OUTPUT_CATEGORY " ); out.append( outputCategory ); out.append( "\n" );
	out.append( " OUTPUT_NAME " ); out.append( outputName ); out.append( "\n" );
	out.append( "FILE_END\n" );
}

void SGRX_FileAsset::GetFullName( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%s/%s",
		StackString<100>(outputCategory).str,
		StackString<100>(outputName).str );
	out = bfr;
}

void SGRX_FileAsset::GetDesc( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%s/%s",
		StackString<100>(outputCategory).str,
		StackString<100>(outputName).str );
	out = bfr;
}


bool SGRX_AssetScript::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "CATEGORY" )
		{
			categories.set( value.until("="), value.after("=") );
		}
		else if( key == "TEXTURE" )
		{
			AssetHandle A = new SGRX_TextureAsset;
			if( A->Parse( cread ) == false )
				return false;
			A->CheckGUID();
			assets.push_back( A );
		}
		else if( key == "MESH" )
		{
			AssetHandle A = new SGRX_MeshAsset;
			if( A->Parse( cread ) == false )
				return false;
			A->CheckGUID();
			assets.push_back( A );
		}
		else if( key == "ANIM_BUNDLE" )
		{
			AssetHandle A = new SGRX_AnimBundleAsset;
			if( A->Parse( cread ) == false )
				return false;
			A->CheckGUID();
			assets.push_back( A );
		}
		else if( key == "FILE" )
		{
			AssetHandle A = new SGRX_FileAsset;
			if( A->Parse( cread ) == false )
				return false;
			A->CheckGUID();
			assets.push_back( A );
		}
		else
		{
			LOG_ERROR << "Unrecognized AssetScript command: " << key << "=" << value;
			return false;
		}
	}
	return true;
}

void SGRX_AssetScript::Generate( String& out )
{
	for( size_t i = 0; i < categories.size(); ++i )
	{
		out.append( "CATEGORY " );
		out.append( categories.item( i ).key );
		out.append( "=" );
		out.append( categories.item( i ).value );
		out.append( "\n" );
	}
	
	for( size_t i = 0; i < assets.size(); ++i )
	{
		assets[ i ]->Generate( out );
	}
}

bool SGRX_AssetScript::Load( const StringView& path )
{
	String data;
	if( FS_LoadTextFile( path, data ) == false )
		return false;
	
	ConfigReader cread( data );
	bool ret = Parse( cread );
	if( ret )
	{
		// check GUIDs and regenerate duplicates
		HashTable< SGRX_GUID, NoValue > guids;
		for( size_t i = 0; i < assets.size(); ++i )
		{
			SGRX_GUID& guid = assets[ i ]->assetGUID;
			if( guid.IsNull() )
			{
				printf( "asset %i GUID is NULL, it will be regenerated\n", int(i) );
				guid.SetGenerated();
			}
			if( guids.isset( guid ) )
			{
				char guidstr[ GUID_STRING_LENGTH + 1 ];
				guid.ToCharArray( guidstr );
				printf( "asset %i GUID %s conflicts with another, it will be regenerated\n",
					int(i), guidstr );
				guid.SetGenerated();
			}
			guids.set( guid, NoValue() );
		}
	}
	return ret;
}

bool SGRX_AssetScript::Save( const StringView& path )
{
	String data;
	Generate( data );
	
	return FS_SaveTextFile( path, data );
}

StringView SGRX_AssetScript::GetCategoryPath( const StringView& name )
{
	return categories.getcopy( name );
}

SGRX_Asset* SGRX_AssetScript::FindAsset( SGRX_AssetType type, StringView cat, StringView name )
{
	for( size_t i = 0; i < assets.size(); ++i )
	{
		SGRX_Asset* A = assets[ i ];
		if( A->assetType == type &&
			A->outputCategory == cat &&
			A->outputName == name )
			return A;
	}
	return NULL;
}

enum EAssetType
{
	AT_Unknown = 0,
	AT_Texture,
	AT_Mesh,
	AT_AnimBundle,
	AT_File,
};

struct AssetInfoItem
{
	uint8_t asset_type;
	String category;
	String name;
	uint32_t rev_asset;
	
	template< class T > void Serialize( T& arch )
	{
		arch << asset_type;
		arch << category;
		arch << name;
		arch << rev_asset;
	}
};

struct AssetOutputItem
{
	uint8_t asset_type;
	String category;
	String name;
	uint32_t ts_source;
	uint32_t ts_output;
	uint32_t rev_output;
	
	template< class T > void Serialize( T& arch )
	{
		arch << asset_type;
		arch << category;
		arch << name;
		arch << ts_source;
		arch << ts_output;
		arch << rev_output;
	}
};

uint8_t AssetTypeToItem( SGRX_AssetType t )
{
	switch( t )
	{
	case SGRX_AT_Texture: return AT_Texture;
	case SGRX_AT_Mesh: return AT_Mesh;
	case SGRX_AT_AnimBundle: return AT_AnimBundle;
	case SGRX_AT_File: return AT_File;
	default: return AT_Unknown;
	}
}

bool SGRX_AssetScript::LoadAssetInfo( const StringView& path )
{
	String data;
	if( FS_LoadTextFile( path, data ) == false )
		return false;
	
	TextReader tr( &data );
	tr.marker( "AS_ASSET_INFO" );
	Array< AssetInfoItem > items;
	tr << items;
	for( size_t i = 0; i < items.size(); ++i )
	{
		const AssetInfoItem& item = items[ i ];
		for( size_t j = 0; j < assets.size(); ++j )
		{
			SGRX_Asset* A = assets[ j ];
			if( AssetTypeToItem( A->assetType ) != item.asset_type ||
				A->outputCategory != item.category ||
				A->outputName != item.name )
				continue;
			A->ri.rev_asset = item.rev_asset;
		}
	}
	
	return tr.error == false;
}

bool SGRX_AssetScript::SaveAssetInfo( const StringView& path )
{
	String data;
	TextWriter tw( &data );
	tw.marker( "AS_ASSET_INFO" );
	Array< AssetInfoItem > items;
	for( size_t j = 0; j < assets.size(); ++j )
	{
		const SGRX_Asset* A = assets[ j ];
		AssetInfoItem item = { AssetTypeToItem( A->assetType ),
			A->outputCategory, A->outputName, A->ri.rev_asset };
		items.push_back( item );
	}
	tw << items;
	
	return FS_SaveTextFile( path, data );
}

bool SGRX_AssetScript::LoadOutputInfo( const StringView& path )
{
	String data;
	if( FS_LoadTextFile( path, data ) == false )
		return false;
	
	TextReader tr( &data );
	tr.marker( "AS_OUTPUT_INFO" );
	Array< AssetOutputItem > items;
	tr << items;
	for( size_t i = 0; i < items.size(); ++i )
	{
		const AssetOutputItem& item = items[ i ];
		for( size_t j = 0; j < assets.size(); ++j )
		{
			SGRX_Asset* A = assets[ j ];
			if( AssetTypeToItem( A->assetType ) != item.asset_type ||
				A->outputCategory != item.category ||
				A->outputName != item.name )
				continue;
			A->ri.ts_source = item.ts_source;
			A->ri.ts_output = item.ts_output;
			A->ri.rev_output = item.rev_output;
		}
	}
	
	return tr.error == false;
}

bool SGRX_AssetScript::SaveOutputInfo( const StringView& path )
{
	String data;
	TextWriter tw( &data );
	tw.marker( "AS_OUTPUT_INFO" );
	Array< AssetOutputItem > items;
	for( size_t j = 0; j < assets.size(); ++j )
	{
		const SGRX_Asset* A = assets[ j ];
		AssetOutputItem item = { AssetTypeToItem( A->assetType ),
			A->outputCategory, A->outputName,
			A->ri.ts_source, A->ri.ts_output, A->ri.rev_output };
		items.push_back( item );
	}
	tw << items;
	
	return FS_SaveTextFile( path, data );
}



static void aiNode_GetMeshList( StringView path, const aiScene* S, aiNode* N,
	AIMeshTable& out, HashTable< String, int >& counts )
{
	String subpath = path.str();
	subpath.append( "/" );
	subpath.append( N->mName.C_Str() );
	
	String meshpath = subpath;
	for( unsigned i = 0; i < N->mNumMeshes; ++i )
	{
		unsigned mid = N->mMeshes[ i ];
		aiMesh* M = S->mMeshes[ mid ];
		meshpath = subpath;
		meshpath.append( ":" );
		meshpath.append( M->mName.C_Str() );
		
		int curcount = counts.getcopy( meshpath, 0 );
		if( curcount )
		{
			char bfr[ 32 ];
			sgrx_snprintf( bfr, 32, "#%d", curcount );
			meshpath.append( bfr );
		}
		counts.set( meshpath, curcount + 1 );
		
		AIMeshInfo info = { N, M };
		out.set( meshpath, info );
	}
	
	for( unsigned i = 0; i < N->mNumChildren; ++i )
	{
		aiNode_GetMeshList( subpath, S, N->mChildren[ i ], out, counts );
	}
}

SGRX_Scene3D::SGRX_Scene3D( const StringView& path, SceneImportOptimizedFor siof ) :
	m_path( path.str() ), m_imp( NULL ), m_scene( NULL )
{
	ByteArray data;
	if( FS_LoadBinaryFile( path, data ) == false )
	{
		printf( "Could not load 3D scene file: %s\n", StackPath(path).str );
		return;
	}
	
	if( data.size() > 8 && memcmp( data.data(), "SGRXANBD", 8 ) == 0 )
	{
		ByteReader br( data );
		br << m_animBundle;
		if( br.error )
		{
			printf( "Could not load animation bundle (SGRXANBD): %s\n", StackPath(path).str );
			m_animBundle.anims.clear();
		}
		return;
	}
	
	if( data.size() > 8 && memcmp( data.data(), "SS3DMESH", 8 ) == 0 )
	{
		m_mesh = GR_GetMesh( path, true );
		if( !m_mesh )
		{
			printf( "Could not load SS3DMESH: %s\n", StackPath(path).str );
		}
		return;
	}
	
	m_imp = new Assimp::Importer;
	int flags =
		aiProcess_CalcTangentSpace |
	//	aiProcess_JoinIdenticalVertices |
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_FlipWindingOrder;
	m_scene = m_imp->ReadFileFromMemory( data.data(), data.size(),
		flags, StackString<32>(path.after_all(".")) );
	if( m_scene == NULL )
	{
		delete m_imp;
		m_imp = NULL;
		return;
	}
	else
	{
		HashTable< String, int > nameCounts;
		aiNode_GetMeshList( "", m_scene, m_scene->mRootNode, m_aiMeshTable, nameCounts );
	}
}

SGRX_Scene3D::~SGRX_Scene3D()
{
	if( m_imp )
	{
		delete m_imp;
		m_imp = NULL;
	}
	m_scene = NULL;
}

void SGRX_Scene3D::GetMeshList( Array< String >& out )
{
	if( m_mesh )
	{
		for( size_t i = 0; i < m_mesh->m_meshParts.size(); ++i )
		{
			out.push_back( m_mesh->m_meshParts[ i ].name );
		}
		return;
	}
	
	if( m_scene == NULL )
		return;
	
	for( size_t i = 0; i < m_aiMeshTable.size(); ++i )
	{
		out.push_back( m_aiMeshTable.item( i ).key );
	}
}

void SGRX_Scene3D::GetAnimList( Array< String >& out )
{
	if( m_animBundle.anims.size() )
	{
		for( size_t i = 0; i < m_animBundle.anims.size(); ++i )
		{
			out.push_back( m_path );
			out.last().append( ":" );
			out.last().append( m_animBundle.anims[ i ]->m_key );
		}
		return;
	}
	
	if( m_scene == NULL )
		return;
	for( unsigned i = 0; i < m_scene->mNumAnimations; ++i )
	{
		aiAnimation* anim = m_scene->mAnimations[ i ];
		out.push_back( m_path );
		out.last().append( ":" );
		out.last().append( anim->mName.C_Str() );
	}
}

AIMeshInfo SGRX_Scene3D::FindAssimpMesh( StringView name )
{
	AIMeshInfo* info = m_aiMeshTable.getptr( name.str() );
	if( info )
		return *info;
	AIMeshInfo out = { NULL, NULL };
	return out;
}



SGRX_IFP32Handle SGRX_LoadImage( const StringView& path )
{
	ByteArray data;
	if( FS_LoadBinaryFile( path, data ) == false )
	{
		LOG_ERROR << "Failed to load image file: " << path;
		return NULL;
	}
	
	if( path.ends_with( ".dds" ) )
	{
		static const dds_u32 dds_supfmt[] = { DDS_FMT_R8G8B8A8, DDS_FMT_B8G8R8A8, DDS_FMT_B8G8R8X8 /*, DDS_FMT_DXT1, DDS_FMT_DXT3, DDS_FMT_DXT5 */, 0 };
		dds_info ddsinfo;
		int err = dds_load_from_memory( data.data(), data.size(), &ddsinfo, dds_supfmt );
		if( err == DDS_SUCCESS || err == DDS_ENOTSUP )
		{
			if( err == DDS_ENOTSUP )
			{
				LOG_ERROR << "Failed to load texture " << path << " - unsupported DDS image format";
				return NULL;
			}
			dds_u32 cmf = ddsinfo.flags & DDS_CUBEMAP_FULL;
			if( cmf && cmf != DDS_CUBEMAP_FULL )
			{
				dds_close( &ddsinfo );
				LOG_ERROR << "  Failed to load texture " << path << " - incomplete cubemap";
				return NULL;
			}
			SGRX_IFP32Handle ih = new SGRX_ImageFP32(
				ddsinfo.image.width, ddsinfo.image.height,
				ddsinfo.image.depth, ddsinfo.flags & DDS_CUBEMAP ? 6 : 1 );
			dds_byte* srcbytes = dds_read_all( &ddsinfo );
			Vec4* dstdata = ih->GetData();
			switch( ddsinfo.image.format )
			{
			case DDS_FMT_R8G8B8A8:
				for( int s = 0; s < ih->GetSides(); ++s )
				{
					dds_byte* srcdata = srcbytes + ddsinfo.sideoffsets[ s ];
					for( int z = 0; z < ih->GetDepth(); ++z )
					{
						for( int y = 0; y < ih->GetHeight(); ++y )
						{
							for( int x = 0; x < ih->GetWidth(); ++x )
							{
								uint8_t r = srcdata[0];
								uint8_t g = srcdata[1];
								uint8_t b = srcdata[2];
								uint8_t a = srcdata[3];
								srcdata += 4;
								*dstdata++ = V4( r, g, b, a ) * INV_255F;
							}
						}
					}
				}
				break;
			case DDS_FMT_B8G8R8A8:
				for( int s = 0; s < ih->GetSides(); ++s )
				{
					dds_byte* srcdata = srcbytes + ddsinfo.sideoffsets[ s ];
					for( int z = 0; z < ih->GetDepth(); ++z )
					{
						for( int y = 0; y < ih->GetHeight(); ++y )
						{
							for( int x = 0; x < ih->GetWidth(); ++x )
							{
								uint8_t b = srcdata[0];
								uint8_t g = srcdata[1];
								uint8_t r = srcdata[2];
								uint8_t a = srcdata[3];
								srcdata += 4;
								*dstdata++ = V4( r, g, b, a ) * INV_255F;
							}
						}
					}
				}
				break;
			case DDS_FMT_B8G8R8X8:
				for( int s = 0; s < ih->GetSides(); ++s )
				{
					dds_byte* srcdata = srcbytes + ddsinfo.sideoffsets[ s ];
					for( int z = 0; z < ih->GetDepth(); ++z )
					{
						for( int y = 0; y < ih->GetHeight(); ++y )
						{
							for( int x = 0; x < ih->GetWidth(); ++x )
							{
								uint8_t b = srcdata[0];
								uint8_t g = srcdata[1];
								uint8_t r = srcdata[2];
								uint8_t a = 255;
								srcdata += 4;
								*dstdata++ = V4( r, g, b, a ) * INV_255F;
							}
						}
					}
				}
				break;
			}
			free( srcbytes );
			return ih;
		}
		LOG_ERROR << "Failed to load texture " << path << " - DDS load error " << err;
		return NULL;
	}
	else
	{
		int w = 0, h = 0, nc = 0;
		stbi_ldr_to_hdr_gamma( 1.0f );
		float* coldata = stbi_loadf_from_memory(
			data.data(), data.size(), &w, &h, &nc, 4 );
		if( coldata == NULL )
		{
			LOG_ERROR << "Failed to parse image file: " << path;
			return NULL;
		}
		SGRX_IFP32Handle ih = new SGRX_ImageFP32( w, h, 1, 1 );
		memcpy( ih->GetData(), coldata, sizeof(Vec4) * w * h );
		
		stbi_image_free( coldata );
		return ih;
	}
}

inline int modup( int x, int d ){ int o = x % d; return o ? o : d; }
static void Img_ReadBlock( SGRX_ImageFP32* image, int bx, int by, int z, int side, uint8_t block[ 64 ] )
{
	const int W = image->GetWidth();
	const int H = image->GetHeight();
	const int xend = modup( W - bx * 4, 4 );
	const int yend = modup( H - by * 4, 4 );
	const size_t sideOff = side * image->SideSize();
	const size_t inBaseOff = sideOff + bx * 4 + by * 4 * W + z * W * H;
	
	int blockLineOff = 0;
	const int blockLineDataEnd = yend * 4 * 4;
	const int blockLineOutEnd = 4 * 4 * 4;
	size_t inLineOff = inBaseOff;
	for( ; blockLineOff < blockLineDataEnd; blockLineOff += 4 * 4 )
	{
		int blockPxOff = blockLineOff;
		const int blockPxDataEnd = blockPxOff + xend * 4;
		const int blockPxOutEnd = blockPxOff + 4 * 4;
		size_t inPxOff = inLineOff;
		for( ; blockPxOff < blockPxDataEnd; blockPxOff += 4 )
		{
			const Vec4& color = (*image)[ inPxOff++ ];
			block[ blockPxOff + 0 ] = clamp( color.x, 0, 1 ) * 255;
			block[ blockPxOff + 1 ] = clamp( color.y, 0, 1 ) * 255;
			block[ blockPxOff + 2 ] = clamp( color.z, 0, 1 ) * 255;
			block[ blockPxOff + 3 ] = clamp( color.w, 0, 1 ) * 255;
		}
		for( ; blockPxOff < blockPxOutEnd; blockPxOff += 4 )
			memcpy( &block[ blockPxOff ], &block[ blockPxOff - 4 ], 4 );
		
		inLineOff += W;
	}
	for( ; blockLineOff < blockLineOutEnd; blockLineOff += 4 * 4 )
		memcpy( &block[ blockLineOff ], &block[ blockLineOff - 4 * 4 ], 4 * 4 );
}

void SGRX_ImageF32ToRGBA8( SGRX_ImageFP32* image, ByteArray& outdata, uint16_t fmt, int side = 0 )
{
	size_t off = outdata.size(), pxcount = image->SideSize();
	if( IS_FLAG_SET( fmt, TEXFF_BLOCKFMT ) )
	{
		size_t blocksize = 0;
		switch( fmt )
		{
		case TEXFMT_DXT1: blocksize = 8; break;
		case TEXFMT_DXT3: blocksize = 16; break;
		case TEXFMT_DXT5: blocksize = 16; break;
		case TEXFMT_3DC: blocksize = 16; break;
		}
		int blocknumx = divideup( image->GetWidth(), 4 );
		int blocknumy = divideup( image->GetHeight(), 4 );
		size_t blockcount = blocknumx * blocknumy * image->GetDepth();
		outdata.resize( off + blockcount * blocksize );
		size_t writeOff = off;
		for( int z = 0; z < image->GetDepth(); ++z )
		{
			for( int by = 0; by < blocknumy; ++by )
			{
				for( int bx = 0; bx < blocknumx; ++bx )
				{
					uint8_t block[ 64 ];
					uint8_t dxtblock[ 16 ];
					Img_ReadBlock( image, bx, by, z, side, block );
					if( fmt == TEXFMT_3DC )
					{
						stb__CompressAlphaBlock( dxtblock, block - 3, 0 );
						stb__CompressAlphaBlock( dxtblock + 8, block - 2, 0 );
					}
					else
						stb_compress_dxt_block( dxtblock, block, fmt == TEXFMT_DXT5, STB_DXT_NORMAL );
					memcpy( &outdata[ writeOff ], dxtblock, blocksize );
					writeOff += blocksize;
				}
			}
		}
	}
	else
	{
		outdata.resize( off + pxcount * 4 );
		uint8_t* data = &outdata[ off ];
		for( size_t i = 0; i < pxcount; ++i )
		{
			const Vec4& color = (*image)[ i + side * pxcount ];
			data[ 0 ] = clamp( color.x, 0, 1 ) * 255;
			data[ 1 ] = clamp( color.y, 0, 1 ) * 255;
			data[ 2 ] = clamp( color.z, 0, 1 ) * 255;
			data[ 3 ] = clamp( color.w, 0, 1 ) * 255;
			data += 4;
		}
	}
}

static void sgrx_png_write_data( png_structp png_ptr, png_bytep data, png_size_t length )
{
	ByteArray* p = (ByteArray*) png_get_io_ptr( png_ptr );
	p->append( data, length );
}

int dumpimg( ByteArray& out, uint8_t* buffer, int width, int height )
{
	int code = 0;
	png_structp png_ptr;
	png_infop info_ptr;
	
	// Initialize write structure
	png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( png_ptr == NULL )
	{
		printf( "Could not allocate write struct\n" );
		return 1;
	}
	
	// Initialize info structure
	info_ptr = png_create_info_struct( png_ptr );
	if( info_ptr == NULL )
	{
		printf( "Could not allocate info struct\n" );
		return 1;
	}
	// Setup Exception handling
	if( setjmp( png_jmpbuf( png_ptr ) ) )
	{
		printf( "Error during png creation\n" );
		code = 1;
		goto fail;
	}
	
	png_set_write_fn( png_ptr, &out, sgrx_png_write_data, NULL );
	
	// Write header (8 bit colour depth)
	png_set_IHDR( png_ptr, info_ptr, width, height,
		8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE );
	
	png_write_info( png_ptr, info_ptr );
	for( int y = 0; y < height; ++y )
	{
		png_write_row( png_ptr, buffer );
		buffer += width * 4;
	}
	png_write_end( png_ptr, NULL );
	
fail:
	if( info_ptr != NULL ) png_free_data( png_ptr, info_ptr, PNG_FREE_ALL, -1 );
	if( png_ptr != NULL ) png_destroy_write_struct( &png_ptr, (png_infopp) NULL );
	return code;
}

static uint16_t STXGetTextureFormat( const SGRX_TextureAsset& TA )
{
	switch( TA.outputType )
	{
	case SGRX_TOF_STX_RGBA32: return TEXFMT_RGBA8;
	case SGRX_TOF_STX_DXT1: return TEXFMT_DXT1;
	case SGRX_TOF_STX_DXT5: return TEXFMT_DXT5;
	case SGRX_TOF_STX_3DC: return TEXFMT_3DC;
	default: return TEXFMT_RGBA8;
	}
}

static void STXPreprocess( const SGRX_TextureAsset& TA, SGRX_ImageFP32* image,
	uint16_t& flags, Array< SGRX_IFP32Handle >& mips )
{
//	if( TA.isSRGB )
//		flags |= TEXFLAGS_SRGB;
	if( TA.lerp )
		flags |= TEXFLAGS_LERP;
	if( TA.clampx )
		flags |= TEXFLAGS_CLAMP_X;
	if( TA.clampy )
		flags |= TEXFLAGS_CLAMP_Y;
	
	mips.push_back( image );
	if( TA.mips )
	{
		flags |= TEXFLAGS_HASMIPS;
		while( mips.last()->GetWidth() != 1
			|| mips.last()->GetHeight() != 1
			|| mips.last()->GetDepth() != 1 )
		{
			int w1 = TMAX( mips.last()->GetWidth() / 2, 1 );
			int h1 = TMAX( mips.last()->GetHeight() / 2, 1 );
			int d1 = TMAX( mips.last()->GetDepth() / 2, 1 );
			mips.push_back( SGRX_ResizeImage( mips.last(), w1, h1, d1 ) );
		}
	}
}

static void STXFinalize( const SGRX_TextureAsset& TA, SGRX_ImageFP32* image, uint16_t flags, const Array< SGRX_IFP32Handle >& mips, ByteArray& filedata )
{
	uint16_t fmt = STXGetTextureFormat( TA );
	
	ByteArray imagedata;
	for( int s = 0; s < image->GetSides(); ++s )
	{
		for( size_t i = 0; i < mips.size(); ++i )
			SGRX_ImageF32ToRGBA8( mips[ i ], imagedata, fmt, s );
	}
	filedata.append( "STX\0", 4 );
	int type = TEXTYPE_2D;
	if( image->GetSides() > 1 )
		type = TEXTYPE_CUBE;
	if( image->GetDepth() > 1 )
		type = TEXTYPE_VOLUME;
	TextureInfo info = { type, mips.size(),
		image->GetWidth(), image->GetHeight(),
		image->GetDepth(), fmt, flags };
	filedata.append( &info, sizeof(info) );
	uint32_t datasize = imagedata.size();
	filedata.append( &datasize, sizeof(datasize) );
	filedata.append( imagedata );
}

bool SGRX_SaveImage( const StringView& path, SGRX_ImageFP32* image, const SGRX_TextureAsset& TA )
{
	ByteArray filedata;
	ByteArray imagedata;
	
	switch( TA.outputType )
	{
	case SGRX_TOF_PNG_RGBA32:
		if( image->GetSides() != 1 )
		{
			printf( "ERROR: PNG does not support cubemaps\n" );
			return false;
		}
		SGRX_ImageF32ToRGBA8( image, imagedata, TEXFMT_RGBA8 );
		if( dumpimg( filedata, imagedata.data(), image->GetWidth(), image->GetHeight() ) )
		{
			printf( "ERROR: failed to encode PNG\n" );
			return false;
		}
		break;
	case SGRX_TOF_STX_RGBA32:
	case SGRX_TOF_STX_DXT1:
	case SGRX_TOF_STX_DXT5:
	case SGRX_TOF_STX_3DC:
		{
			uint16_t flags = 0;
			Array< SGRX_IFP32Handle > mips;
			STXPreprocess( TA, image, flags, mips );
			STXFinalize( TA, image, flags, mips, filedata );
		}
		break;
	default:
		printf( "ERROR: invalid output format: %d\n", TA.outputType );
		break;
	}
	
	if( FS_SaveBinaryFile( path, filedata.data(), filedata.size() ) == false )
	{
		printf( "ERROR: failed to save the texture file: %s\n", StackString<256>(path).str );
		return false;
	}
	return true;
}

SGRX_IFP32Handle SGRX_ProcessTextureAsset( const SGRX_TextureAsset* TA )
{
	printf( "| %s => [%s] %s\n",
		StackString<256>(TA->sourceFile).str,
		StackString<256>(TA->outputCategory).str,
		StackString<256>(TA->outputName).str );
	SGRX_IFP32Handle image = SGRX_LoadImage( TA->sourceFile );
	if( image == NULL )
		return NULL;
	
	SGRX_ImageFilterState ifs =
	{
		TA->isSRGB,
	};
	for( size_t fid = 0; fid < TA->filters.size(); ++fid )
	{
		SGRX_ImageFilter* IF = TA->filters[ fid ];
		printf( "|-- filter: %s... ", IF->GetName() );
		image = IF->Process( image, ifs );
		printf( "%s\n", image ? "OK" : "ERROR" );
		if( image == NULL )
			break;
	}
	
	return image;
}

TextureHandle SGRX_FP32ToTexture( SGRX_ImageFP32* image, const SGRX_TextureAsset* TA )
{
	if( !image )
		return NULL;
	uint16_t flags = 0;
	Array< SGRX_IFP32Handle > mips;
	STXPreprocess( *TA, image, flags, mips );
	/*
	TextureHandle tex = GR_CreateTexture( image->GetWidth(), image->GetHeight(),
		TEXFMT_RGBA8, flags, mips.size(), NULL );
	ByteArray imagedata;
	for( size_t i = 0; i < mips.size(); ++i )
	{
		imagedata.clear();
		SGRX_ImageF32ToRGBA8( mips[ i ], imagedata, TEXFMT_RGBA8 );
		tex.UploadRGBA8Part( imagedata.data(), i );
	}
	return tex;*/
	ByteArray filedata;
	STXFinalize( *TA, image, flags, mips, filedata );
	return GR_CreateTextureFromBytes( filedata );
}

struct VertexSkin
{
	uint8_t weights[4];
	uint8_t indices[4];
};

struct VertexWeight
{
	uint8_t boneID;
	float weight;
	
	static int RevSort( const void* a, const void* b )
	{
		SGRX_CAST( VertexWeight*, vwa, a );
		SGRX_CAST( VertexWeight*, vwb, b );
		return vwa->weight == vwb->weight ? 0 : ( vwa->weight > vwb->weight ? -1 : 1 );
	}
};

struct MeshAssetInputs
{
	bool pos;
	bool nrm;
	bool tng;
	bool col;
	bool tx0;
	bool tx1;
	bool tx2;
	bool tx3;
	bool skin;
	// additional flags
	bool y2z;
	bool flip_uvy;
	// matrices
	Mat4 tf_pos;
	Mat4 tf_nrm;
	// skinning data
	VertexSkin* vertexSkin;
};

static size_t _InsertVertex( ByteArray& out,
	const MeshAssetInputs& fmt, const aiMesh* mesh, unsigned idx, size_t from )
{
	uint8_t bfr[ 256 ];
	uint8_t* p = bfr;
	
	if( fmt.pos )
	{
		aiVector3D v = mesh->mVertices ? mesh->mVertices[ idx ] : aiVector3D(0);
		Vec3 ov = { v.x, v.y, v.z };
		ov = fmt.tf_pos.TransformPos( ov );
		if( fmt.y2z )
			ov = V3( ov.x, -ov.z, ov.y );
		memcpy( p, &ov, sizeof(ov) );
		p += sizeof(ov);
	}
	
	if( fmt.nrm )
	{
		aiVector3D v = mesh->mNormals ? mesh->mNormals[ idx ] : aiVector3D(0);
		Vec3 ov = { v.x, v.y, v.z };
		ov = fmt.tf_nrm.TransformNormal( ov );
		if( fmt.y2z )
			ov = V3( ov.x, -ov.z, ov.y );
		memcpy( p, &ov, sizeof(ov) );
		p += sizeof(ov);
	}
	
	if( fmt.tng )
	{
		Vec4 ov = { 0, 0, 1, 1 };
		if( mesh->mNormals && mesh->mTangents && mesh->mBitangents )
		{
			aiVector3D n = mesh->mNormals[ idx ];
			aiVector3D t = mesh->mTangents[ idx ];
			aiVector3D b = mesh->mBitangents[ idx ];
			Vec3 vn = { n.x, n.y, n.z };
			Vec3 vt = { t.x, t.y, t.z };
			Vec3 vb = { b.x, b.y, b.z };
			Vec3 tt = vt;
			if( fmt.flip_uvy )
				vb = -vb;
			if( fmt.y2z )
				tt = V3( tt.x, -tt.z, tt.y );
			ov = V4( tt, sign( Vec3Dot( Vec3Cross( vn, vt ), vb ) ) );
		}
		memcpy( p, &ov, sizeof(ov) );
		p += sizeof(ov);
	}
	
	if( fmt.col )
	{
		aiColor4D v = mesh->mColors[0] ? mesh->mColors[0][ idx ] : aiColor4D(1);
		Vec4 oc = { v.r, v.g, v.b, v.a };
		uint32_t ov = Vec4ToCol32( oc );
		memcpy( p, &ov, sizeof(ov) );
		p += sizeof(ov);
	}
	
	if( fmt.tx0 )
	{
		aiVector3D v = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][ idx ] : aiVector3D(0);
		Vec2 ov = { v.x, v.y };
		if( fmt.flip_uvy )
			ov.y = 1 - ov.y;
		memcpy( p, &ov, sizeof(ov) );
		p += sizeof(ov);
	}
	
	if( fmt.tx1 )
	{
		aiVector3D v = mesh->mTextureCoords[1] ? mesh->mTextureCoords[1][ idx ] : aiVector3D(0);
		Vec2 ov = { v.x, v.y };
		if( fmt.flip_uvy )
			ov.y = 1 - ov.y;
		memcpy( p, &ov, sizeof(ov) );
		p += sizeof(ov);
	}
	
	if( fmt.tx2 )
	{
		aiVector3D v = mesh->mTextureCoords[2] ? mesh->mTextureCoords[2][ idx ] : aiVector3D(0);
		Vec2 ov = { v.x, v.y };
		if( fmt.flip_uvy )
			ov.y = 1 - ov.y;
		memcpy( p, &ov, sizeof(ov) );
		p += sizeof(ov);
	}
	
	if( fmt.tx3 )
	{
		aiVector3D v = mesh->mTextureCoords[3] ? mesh->mTextureCoords[3][ idx ] : aiVector3D(0);
		Vec2 ov = { v.x, v.y };
		if( fmt.flip_uvy )
			ov.y = 1 - ov.y;
		memcpy( p, &ov, sizeof(ov) );
		p += sizeof(ov);
	}
	
	if( fmt.skin )
	{
		if( fmt.vertexSkin )
			memcpy( p, &fmt.vertexSkin[ idx ], 8 );
		else
			memset( p, 0, 8 );
		p += 8;
	}
	
	return out.find_or_add_bytes( bfr, p - bfr, from );
}

static String SGRX_TexIDToPath( const SGRX_AssetScript* AS, const StringView& texid )
{
	// system texture
	if( texid.starts_with( "sys:" ) )
		return texid.str();
	
	StringView cat = texid.until( "/" );
	StringView name = texid.after( "/" );
	
	for( size_t i = 0; i < AS->assets.size(); ++i )
	{
		SGRX_Asset* A = AS->assets[ i ];
		if( A->assetType == SGRX_AT_Texture &&
			A->outputCategory == cat &&
			A->outputName == name )
		{
			return A->GetUserPath();
		}
	}
	if( texid != "" )
		LOG_ERROR << "Could not map texture ID: " << texid;
	return "";
}

struct NodeParentInfo
{
	StringView name;
	Mat4 totalXF;
	int depth;
};

static void _EnumNodes( const char* base, const Mat4& ptf, const aiNode* N,
	HashTable<String, Mat4>& out, HashTable<StringView, NodeParentInfo>& outPN, int depth = 0 )
{
	Mat4 local = Mat4::CreateFromPtr( N->mTransformation[0] );
	local.Transpose();
	Mat4 mytf = local * ptf;
	char buf[ 1024 ];
	sgrx_snprintf( buf, 1024, "%s/%s", base, N->mName.C_Str() );
	out.set( buf, mytf );
	
	NodeParentInfo NPI = { "", mytf, depth };
	if( N->mParent )
		NPI.name = N->mParent->mName.C_Str();
	outPN.set( N->mName.C_Str(), NPI );
	
	for( unsigned i = 0; i < N->mNumChildren; ++i )
		_EnumNodes( buf, mytf, N->mChildren[ i ], out, outPN, depth + 1 );
}

struct BoneInfo
{
	BoneInfo() : parentID( -1 ), offset( Mat4::Identity ), depth( 0 ){}
	bool operator == ( const BoneInfo& o ) const { return name == o.name; }
	
	static int DepthSort( const void* a, const void* b )
	{
		SGRX_CAST( BoneInfo*, bia, a );
		SGRX_CAST( BoneInfo*, bib, b );
		return bia->depth == bib->depth ? 0 : ( bia->depth < bib->depth ? -1 : 1 );
	}
	
	StringView name;
	StringView parentName;
	int parentID;
	Mat4 offset;
	int depth;
};

static const SGRX_MeshPart* _FindMeshPartByName( const SGRX_IMesh* M, StringView name )
{
	for( size_t i = 0; i < M->m_meshParts.size(); ++i )
	{
		if( M->m_meshParts[ i ].name == name )
			return &M->m_meshParts[ i ];
	}
	return NULL;
}

MeshHandle SGRX_ProcessMeshAsset( const SGRX_AssetScript* AS, const SGRX_MeshAsset& MA )
{
	if( MA.parts.size() < 1 || MA.parts.size() > 16 )
	{
		puts( "Mesh part count not in range [1;16]" );
		return NULL;
	}
	
	printf( "| %s => [%s] %s\n",
		StackString<256>(MA.sourceFile).str,
		StackString<256>(MA.outputCategory).str,
		StackString<256>(MA.outputName).str );
	
	ImpScene3DHandle scene = new SGRX_Scene3D( MA.sourceFile );
	
	if( scene->m_mesh )
	{
		ByteArray vdata;
		ByteArray idata;
		Array< SGRX_MeshPart > mparts;
		
		SGRX_IMesh* srcM = scene->m_mesh;
		VertexDeclHandle vdh = srcM->m_vertexDecl;
		size_t vsize = vdh.GetInfo().size;
		size_t isize = ( srcM->m_dataFlags & MDF_INDEX_32 ) ? 4 : 2;
		for( size_t i = 0; i < MA.parts.size(); ++i )
		{
			SGRX_MeshAssetPart* MP = MA.parts[ i ];
			const SGRX_MeshPart* srcMP = _FindMeshPartByName( srcM, MP->meshName );
			if( srcMP == NULL )
			{
				printf( "Mesh part \"%s\" was not found!\n", StackPath( MP->meshName ).str );
				return NULL;
			}
			
			size_t voff = vdata.size() / vsize;
			size_t ioff = idata.size() / isize;
			
			vdata.append( &srcM->m_vdata[ srcMP->vertexOffset * vsize ], srcMP->vertexCount * vsize );
			idata.append( &srcM->m_idata[ srcMP->indexOffset * isize ], srcMP->indexCount * isize );
			
			bool transform = MP->optTransform ? MP->optTransform > 0 : MA.transform;
			if( transform )
			{
				vdh.GetInfo().TransformVertices(
					srcMP->nodeTransform,
					&vdata[ voff * vsize ],
					srcMP->vertexCount
				);
			}
			
			SGRX_MeshPart outMP;
			outMP.vertexOffset = voff;
			outMP.indexOffset = ioff;
			outMP.vertexCount = vdata.size() / vsize - voff;
			outMP.indexCount = idata.size() / isize - ioff;
			outMP.shader = MP->shader;
			for( int t = 0; t < 8; ++t )
				outMP.textures[ t ] = SGRX_TexIDToPath( AS, MP->textures[ t ] );
			outMP.mtlFlags = MP->mtlFlags;
			outMP.mtlBlendMode = MP->mtlBlendMode;
			mparts.push_back( outMP );
			
			printf( "| part %d: mesh=%p vc=%d ic=%d shader=%s\n",
				i, srcMP, int(outMP.vertexCount), int(outMP.indexCount),
				StackPath(outMP.shader).str );
		}
		
		if( MA.flipUVY )
		{
			vdh.GetInfo().TransformTexcoords(
				V4( 1, -1, 1, 1 ),
				V4( 0, 1, 0, 0 ),
				vdata.data(),
				vdata.size() / vsize );
		}
		if( MA.rotateY2Z )
		{
			vdh.GetInfo().TransformVertices(
				Mat4::CreateRotationX( -M_PI / 2 ),
				vdata.data(),
				vdata.size() / vsize );
		}
		
		MeshHandle dstMesh = GR_CreateMesh();
		dstMesh->SetVertexData( vdata.data(), vdata.size(), vdh );
		dstMesh->SetAABBFromVertexData( vdata.data(), vdata.size(), vdh );
		dstMesh->SetIndexData( idata.data(), idata.size(), isize == 4 );
		dstMesh->SetPartData( mparts.data(), mparts.size() );
		dstMesh->m_vdata = vdata;
		dstMesh->m_idata = idata;
		dstMesh->m_dataFlags = MDF_MTLINFO | ( isize == 4 ? MDF_INDEX_32 : 0 );
		
		if( srcM->m_dataFlags & MDF_SKINNED )
		{
			for( size_t i = 0; i < SGRX_ARRAY_SIZE( srcM->m_bones ); ++i )
			{
				dstMesh->m_bones[ i ] = srcM->m_bones[ i ];
			}
			dstMesh->m_numBones = srcM->m_numBones;
			dstMesh->m_dataFlags |= MDF_SKINNED;
		}
		
		return dstMesh;
	}
	
	if( scene->m_scene == NULL )
	{
		printf( "Failed to load %s\n", StackString<256>(MA.sourceFile).str );
		return NULL;
	}
	const aiScene* S = scene->m_scene;
	
	HashTable<String, Mat4> nodes;
	HashTable<StringView, NodeParentInfo> parentInfo;
	_EnumNodes( "", Mat4::Identity, S->mRootNode, nodes, parentInfo );
	
	// enumerate mesh data
	Mat4 part_xfs[ 16 ];
	const aiMesh* part_ptrs[ 16 ] = {0};
	MeshAssetInputs fmt = {0};
	fmt.y2z = MA.rotateY2Z;
	fmt.flip_uvy = MA.flipUVY;
	int numparts = MA.parts.size();
	Array< BoneInfo > boneInfo;
	HashTable< StringView, NoValue > boneNames;
	for( int i = 0; i < numparts; ++i )
	{
		SGRX_MeshAssetPart* MP = MA.parts[ i ];
		const aiMesh* mesh = scene->FindAssimpMesh( MP->meshName ).mesh;
		if( mesh == NULL )
		{
			printf( "Mesh part \"%s\" was not found!\n", StackPath( MP->meshName ).str );
			return NULL;
		}
		if( mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE )
		{
			printf( "Mesh has wrong primitive type (expected TRIANGLE)\n" );
			return NULL;
		}
		part_ptrs[ i ] = mesh;
		part_xfs[ i ] = nodes.getcopy( SV( MP->meshName ).until(":").str(), Mat4::Identity );
		// update format
		if( mesh->mVertices ) fmt.pos = true;
		if( mesh->mNormals ) fmt.nrm = true;
		if( mesh->mNormals && mesh->mTangents && mesh->mBitangents ) fmt.tng = true;
		if( mesh->mColors[0] ) fmt.col = true;
		if( mesh->mTextureCoords[0] ) fmt.tx0 = true;
		if( mesh->mTextureCoords[1] ) fmt.tx1 = true;
		if( mesh->mTextureCoords[2] ) fmt.tx2 = true;
		if( mesh->mTextureCoords[3] ) fmt.tx3 = true;
		if( mesh->mBones )
		{
			fmt.skin = true;
			for( unsigned b = 0; b < mesh->mNumBones; ++b )
			{
				BoneInfo tmpl;
				tmpl.name = mesh->mBones[ b ]->mName.C_Str();
				Mat4 off = Mat4::CreateFromPtr( mesh->mBones[ b ]->mOffsetMatrix[0] );
				off.Transpose();
				off.InvertTo( tmpl.offset );
				if( MP->optTransform ? MP->optTransform > 0 : MA.transform )
					tmpl.offset = tmpl.offset * part_xfs[ i ];
				boneInfo.find_or_add( tmpl );
				boneNames.set( tmpl.name, NoValue() );
			}
		}
	}
	
	if( fmt.skin )
	{
		// check if total bone count is valid
		unsigned MAX_BONES = SGRX_MAX_MESH_BONES;
		if( boneInfo.size() > MAX_BONES )
		{
			printf( "CANNOT SKIN THE MESH - TOO MANY BONES USED (%d, max=%d)\n",
				int(boneInfo.size()), MAX_BONES );
			fmt.skin = false;
		}
		
		// gather bone hierarchy info
		int numParentless = 0;
		for( size_t b = 0; b < boneInfo.size(); ++b )
		{
			BoneInfo& BI = boneInfo[ b ];
			NodeParentInfo* NPI = parentInfo.getptr( BI.name );
			while( NPI && NPI->name.size() && boneNames.isset( NPI->name ) == false )
				NPI = parentInfo.getptr( NPI->name );
			if( NPI == NULL )
			{
				printf( "CANNOT SKIN THE MESH - BONE NAME NOT FOUND IN HIERARCHY (%s)\n",
					StackPath(BI.name).str );
				fmt.skin = false;
				break;
			}
			BI.parentName = NPI->name;
			BI.offset = BI.offset;
			BI.depth = NPI->depth;
			
			numParentless += NPI->name.size() == 0;
		}
		if( numParentless != 1 )
		{
			printf( "CANNOT SKIN THE MESH - INVALID NUMBER OF PARENTLESS BONES (%d, req=1)\n",
				numParentless );
			fmt.skin = false;
		}
	}
	
	// calculate order & parent IDs
	qsort( boneInfo.data(), boneInfo.size(), sizeof(BoneInfo), BoneInfo::DepthSort );
	for( size_t b = 0; b < boneInfo.size(); ++b )
	{
		BoneInfo& BI = boneInfo[ b ];
		for( size_t pb = 0; pb < b; ++pb )
		{
			if( boneInfo[ pb ].name == BI.parentName )
			{
				BI.parentID = pb;
				break;
			}
		}
	}
	
	// generate vdecl
	char vfmt_bfr[ 64 ];
	size_t vsize = 0;
	size_t isize = 2;
	{
		char* p = vfmt_bfr;
		if( fmt.pos ){ p[0] = 'p'; p[1] = 'f'; p[2] = '3'; p += 3; vsize += 12; }
		if( fmt.nrm ){ p[0] = 'n'; p[1] = 'f'; p[2] = '3'; p += 3; vsize += 12; }
		if( fmt.tng ){ p[0] = 't'; p[1] = 'f'; p[2] = '4'; p += 3; vsize += 16; }
		if( fmt.col ){ p[0] = 'c'; p[1] = 'b'; p[2] = '4'; p += 3; vsize += 4; }
		if( fmt.tx0 ){ p[0] = '0'; p[1] = 'f'; p[2] = '2'; p += 3; vsize += 8; }
		if( fmt.tx1 ){ p[0] = '1'; p[1] = 'f'; p[2] = '2'; p += 3; vsize += 8; }
		if( fmt.tx2 ){ p[0] = '2'; p[1] = 'f'; p[2] = '2'; p += 3; vsize += 8; }
		if( fmt.tx3 ){ p[0] = '3'; p[1] = 'f'; p[2] = '2'; p += 3; vsize += 8; }
		if( fmt.skin )
		{
			*p++ = 'w'; *p++ = 'b'; *p++ = '4';
			*p++ = 'i'; *p++ = 'b'; *p++ = '4';
			vsize += 8;
		}
		*p = 0;
	}
	VertexDeclHandle vdh = GR_GetVertexDecl( vfmt_bfr );
	if( vdh == NULL )
	{
		puts( "Failed to create vertex decl." );
		return NULL;
	}
	
	Array< VertexSkin > vertexSkin;
	
	// generate vertex/index/part data
	ByteArray vdata;
	ByteArray idata;
	SGRX_MeshPart mparts[ 16 ];
	for( int i = 0; i < numparts; ++i )
	{
		SGRX_MeshAssetPart* MP = MA.parts[ i ];
		const aiMesh* mesh = part_ptrs[ i ];
		bool transform = MP->optTransform ? MP->optTransform > 0 : MA.transform;
		
		if( transform )
		{
			Mat4 tf_pos = part_xfs[ i ];
			Mat4 tf_nrm = Mat4::Identity;
			tf_pos.InvertTo( tf_nrm );
			tf_nrm.Transpose();
			
			fmt.tf_pos = tf_pos;
			fmt.tf_nrm = tf_nrm;
		}
		else
		{
			fmt.tf_pos = Mat4::Identity;
			fmt.tf_nrm = Mat4::Identity;
		}
		
		fmt.vertexSkin = NULL;
		if( fmt.skin )
		{
			Array< Array< VertexWeight > > vertexWeights;
			vertexWeights.resize( mesh->mNumVertices );
			
			// gather vertex influences
			for( unsigned b = 0; b < mesh->mNumBones; ++b )
			{
				aiBone* B = mesh->mBones[ b ];
				for( unsigned w = 0; w < B->mNumWeights; ++w )
				{
					const aiVertexWeight& W = B->mWeights[ w ];
					BoneInfo test;
					test.name = B->mName.C_Str();
					VertexWeight weight = { boneInfo.find_first_at( test ), W.mWeight };
					vertexWeights[ W.mVertexId ].push_back( weight );
				}
			}
			
			// sort, filter, normalize, move, pad influences
			vertexSkin.clear();
			vertexSkin.resize( vertexWeights.size() );
			for( size_t v = 0; v < vertexWeights.size(); ++v )
			{
				Array< VertexWeight >& VW = vertexWeights[ v ];
				// sort
				qsort( VW.data(), VW.size(),
					sizeof(VertexWeight), VertexWeight::RevSort );
				// filter
				if( VW.size() > 4 )
					VW.resize( 4 );
				// normalize
				if( VW.size() )
				{
					float total = 0;
					for( size_t w = 0; w < VW.size(); ++w )
						total += VW[ w ].weight;
					if( total != 0 )
					{
						for( size_t w = 0; w < VW.size(); ++w )
							VW[ w ].weight /= total;
					}
				}
				// move
				VertexSkin vsk;
				size_t w = 0;
				for( ; w < VW.size(); ++w )
				{
					vsk.weights[ w ] = VW[ w ].weight * 255;
					vsk.indices[ w ] = VW[ w ].boneID;
				}
				// pad
				for( ; w < 4; ++w )
				{
					vsk.weights[ w ] = 0;
					vsk.indices[ w ] = 0;
				}
				vertexSkin[ v ] = vsk;
			}
			
			fmt.vertexSkin = vertexSkin.data();
		}
		
		size_t voff = vdata.size() / vsize;
		size_t ioff = idata.size() / isize;
		for( unsigned fid = 0; fid < mesh->mNumFaces; ++fid )
		{
			aiFace& F = mesh->mFaces[ fid ];
			ASSERT( F.mNumIndices == 3 );
			
			unsigned idx0 = F.mIndices[0];
			unsigned idx1 = F.mIndices[1];
			unsigned idx2 = F.mIndices[2];
			
			size_t t0 = _InsertVertex( vdata, fmt, mesh, idx0, voff );
			size_t t1 = _InsertVertex( vdata, fmt, mesh, idx1, voff );
			size_t t2 = _InsertVertex( vdata, fmt, mesh, idx2, voff );
			
			uint16_t idcs[3] = { t0 - voff, t1 - voff, t2 - voff };
			idata.append( &idcs, sizeof(idcs) );
		}
		
		SGRX_MeshPart& outMP = mparts[ i ];
		outMP.vertexOffset = voff;
		outMP.vertexCount = vdata.size() / vsize - voff;
		outMP.indexOffset = ioff;
		outMP.indexCount = idata.size() / isize - ioff;
		outMP.shader = MP->shader;
		for( int t = 0; t < 8; ++t )
			outMP.textures[ t ] = SGRX_TexIDToPath( AS, MP->textures[ t ] );
		outMP.mtlFlags = MP->mtlFlags;
		outMP.mtlBlendMode = MP->mtlBlendMode;
		
		printf( "| part %d: mesh=%p src-vc=%d src-fc=%d dst-vc=%d dst-ic=%d shader=%s\n",
			i, mesh, mesh->mNumVertices, mesh->mNumFaces,
			int(outMP.vertexCount), int(outMP.indexCount),
			StackString<256>(outMP.shader).str );
	}
	
	MeshHandle dstMesh = GR_CreateMesh();
	dstMesh->SetVertexData( vdata.data(), vdata.size(), vdh );
	dstMesh->SetAABBFromVertexData( vdata.data(), vdata.size(), vdh );
	dstMesh->SetIndexData( idata.data(), idata.size(), false );
	dstMesh->SetPartData( mparts, numparts );
	dstMesh->m_vdata = vdata;
	dstMesh->m_idata = idata;
	dstMesh->m_dataFlags = MDF_MTLINFO;
	
	if( fmt.skin )
	{
		// generate bone data
		for( size_t b = 0; b < boneInfo.size(); ++b )
		{
			SGRX_MeshBone& MB = dstMesh->m_bones[ b ];
			MB.name = boneInfo[ b ].name;
			MB.skinOffset = boneInfo[ b ].offset;
			MB.invSkinOffset = Mat4::Identity;
			MB.skinOffset.InvertTo( MB.invSkinOffset );
			MB.parent_id = boneInfo[ b ].parentID;
			if( MB.parent_id >= 0 )
				MB.boneOffset = MB.skinOffset * dstMesh->m_bones[ MB.parent_id ].invSkinOffset;
			else
				MB.boneOffset = MB.skinOffset;
		}
		dstMesh->m_numBones = boneInfo.size();
		
		dstMesh->m_dataFlags |= MDF_SKINNED;
	}
	
	return dstMesh;
}




template< class T > typename T::elem_type _InterpKeys( T* keys, unsigned size, double at )
{
	Assimp::Interpolator<T> lerper;
	
	ASSERT( size != 0 );
	
	if( keys[ 0 ].mTime >= at )
		return keys[ 0 ].mValue;
	
	for( unsigned i = 1; i < size; ++i )
	{
		if( keys[ i ].mTime > at )
		{
			typename T::elem_type out;
			lerper( out, keys[ i - 1 ], keys[ i ],
				TREVLERP<double>( keys[ i - 1 ].mTime, keys[ i ].mTime, at ) );
			return out;
		}
	}
	
	return keys[ size - 1 ].mValue;
}

struct AnimProcessor
{
	AnimProcessor( const SGRX_AnimBundleAsset& ABA ) : m_ABA( ABA ){}
	
	ImpScene3DHandle _GetSourceData( StringView name )
	{
		for( size_t i = 0; i < m_ABA.sources.size(); ++i )
		{
			if( m_ABA.sources[ i ].file == name )
				return _GetSourceData( i );
		}
		puts( "ERROR: source file not registered" );
		return NULL;
	}
	ImpScene3DHandle _GetSourceData( int id )
	{
		ImpScene3DHandle h = m_sourceData.getcopy( id );
		if( h )
			return h;
		h = new SGRX_Scene3D( m_ABA.sources[ id ].file, SIOF_Anims );
		if( !h->m_scene && !h->m_animBundle.anims.size() )
		{
			printf( "ERROR: failed to load source: %s\n", StackPath(h->m_path).str );
			return NULL;
		}
		m_sourceData.set( id, h );
		return h;
	}
	
	void _EnumInvTransforms( aiNode* N, HashTable< StringView, Mat4 >& invXFs )
	{
		Mat4 mytf = Mat4::CreateFromPtr( N->mTransformation[0] );
		mytf.Transpose();
		Mat4 invtf = Mat4::Identity;
		mytf.InvertTo( invtf );
		
		LOG << "NODE XF " << N->mName.C_Str() << ": " << mytf;
		
		invXFs.set( N->mName.C_Str(), invtf );
		
		for( unsigned i = 0; i < N->mNumChildren; ++i )
			_EnumInvTransforms( N->mChildren[ i ], invXFs );
	}
	
	static void _AddAnimTracks( AnimHandle out, StringView name, Vec3SAV trkPos, QuatSAV trkRot, Vec3SAV trkScl )
	{
		bool diffPos = false;
		bool diffRot = false;
		bool diffScl = false;
		
		for( unsigned f = 0; f < out->frameCount; ++f )
		{
			diffScl = diffScl || ( f >= 1 && trkRot[ f ] != trkRot[ f - 1 ] );
			diffRot = diffRot || ( f >= 1 && trkRot[ f ] != trkRot[ f - 1 ] );
			diffPos = diffPos || ( f >= 1 && trkPos[ f ] != trkPos[ f - 1 ] );
		}
		
		if( !diffPos )
			trkPos = trkPos.part( 0, 1 );
		if( !diffRot )
			trkRot = trkRot.part( 0, 1 );
		if( !diffScl )
			trkScl = trkScl.part( 0, 1 );
		
		out->AddTrack( name, trkPos, trkRot, trkScl );
	}
	
	AnimHandle ProcessAnim( int i )
	{
		if( i < 0 || i >= (int) m_ABA.anims.size() )
			return NULL;
		const SGRX_ABAnimation& AN = m_ABA.anims[ i ];
		
		StringView source = AN.source;
		StringView animFile = source.until_last( ":" );
		StringView animName = source.after_last( ":" );
		
		ImpScene3DHandle scene = _GetSourceData( animFile );
		if( !scene )
			return NULL;
		
		Array< Vec3 > outPos;
		Array< Quat > outRot;
		Array< Vec3 > outScl;
		
		if( scene->m_animBundle.anims.size() )
		{
			size_t anim_id = NOT_FOUND;
			for( size_t i = 0; i < scene->m_animBundle.anims.size(); ++i )
			{
				if( scene->m_animBundle.anims[ i ]->m_key == animName )
				{
					anim_id = i;
					break;
				}
			}
			if( anim_id == NOT_FOUND )
			{
				printf( "ERROR: animation not found in file: %s\n", StackPath(source).str );
				return NULL;
			}
			AnimHandle srcAnim = scene->m_animBundle.anims[ anim_id ];
			
			printf( "# anim name: %s, channels: %d, length (ticks): %d, ticks/sec: %g\n",
				StackPath(srcAnim->m_key).str, (int) srcAnim->tracks.size(),
				(int) srcAnim->frameCount - 1, srcAnim->speed );
			
			unsigned startFrame = AN.startFrame >= 0 ? AN.startFrame : 0;
			unsigned endFrame = AN.endFrame >= 0 ? AN.endFrame : unsigned( srcAnim->frameCount - 1 );
			
			AnimHandle out = new SGRX_Animation;
			out->m_key = srcAnim->m_key;
			out->frameCount = endFrame - startFrame + 1;
			out->speed = srcAnim->speed;
			
			// add tracks
			for( size_t t = 0; t < srcAnim->tracks.size(); ++t )
			{
				outPos.clear();
				outRot.clear();
				outScl.clear();
				outPos.resize( out->frameCount );
				outRot.resize( out->frameCount );
				outScl.resize( out->frameCount );
				
				for( unsigned f = 0; f < out->frameCount; ++f )
				{
					srcAnim->GetState( t, f + startFrame, outPos[ f ], outRot[ f ], outScl[ f ] );
				}
				
				_AddAnimTracks( out, srcAnim->tracks[ t ].name, outPos, outRot, outScl );
			}
			
			// add markers
			for( size_t m = 0; m < srcAnim->markers.size(); ++m )
			{
				SGRX_Animation::Marker M = srcAnim->markers[ m ];
				if( M.frame >= startFrame && M.frame <= endFrame )
				{
					M.frame -= startFrame;
					out->markers.push_back( M );
				}
			}
			
			return out;
		}
		
		if( !scene->m_scene )
		{
			printf( "ERROR: no scene!\n" );
			return NULL;
		}
		
		HashTable< StringView, Mat4 > invXF;
		_EnumInvTransforms( scene->m_scene->mRootNode, invXF );
		
		aiAnimation* anim = NULL;
		for( unsigned i = 0; i < scene->m_scene->mNumAnimations; ++i )
		{
			aiAnimation* a = scene->m_scene->mAnimations[ i ];
			if( StringView( a->mName.C_Str() ) == animName )
			{
				anim = a;
				break;
			}
		}
		if( !anim )
		{
			printf( "ERROR: animation not found in file: %s\n", StackPath(source).str );
			return NULL;
		}
		
		printf( "# anim name: %s, channels: %d, length (ticks): %g, ticks/sec: %g\n",
			anim->mName.C_Str(), anim->mNumChannels, anim->mDuration, anim->mTicksPerSecond );
		if( !anim->mNumChannels )
		{
			printf( "ERROR: no node animation channels found\n" );
			return NULL;
		}
		
		unsigned startFrame = AN.startFrame >= 0 ? AN.startFrame : 0;
		unsigned endFrame = AN.endFrame >= 0 ? AN.endFrame : unsigned( anim->mDuration );
		
		AnimHandle out = new SGRX_Animation;
		out->m_key = anim->mName.C_Str();
		out->frameCount = endFrame - startFrame + 1;
		out->speed = anim->mTicksPerSecond;
		
		for( unsigned i = 0; i < anim->mNumChannels; ++i )
		{
			aiNodeAnim* na = anim->mChannels[ i ];
			printf( "# track node: %s, pos.keys: %d, rot.keys: %d, scale keys: %d\n",
				na->mNodeName.C_Str(), na->mNumPositionKeys,
				na->mNumRotationKeys, na->mNumScalingKeys );
			
			outPos.clear();
			outRot.clear();
			outScl.clear();
			outPos.resize( out->frameCount );
			outRot.resize( out->frameCount );
			outScl.resize( out->frameCount );
			
			Mat4* p_invNodeXF = invXF.getptr( na->mNodeName.C_Str() );
			if( !p_invNodeXF )
			{
				printf( "ERROR: no node found with matching name: %s\n", na->mNodeName.C_Str() );
				return NULL;
			}
			Mat4 invNodeXF = *p_invNodeXF;
			
			for( unsigned f = 0; f < out->frameCount; ++f )
			{
				aiVector3D tmp( 0, 0, 0 );
				if( na->mNumPositionKeys )
					tmp = _InterpKeys( na->mPositionKeys, na->mNumPositionKeys, f + startFrame );
				outPos[ f ] = V3( tmp.x, tmp.y, tmp.z );
			}
			
			for( unsigned f = 0; f < out->frameCount; ++f )
			{
				aiQuaternion tmp( 1, 0, 0, 0 );
				if( na->mNumRotationKeys )
					tmp = _InterpKeys( na->mRotationKeys, na->mNumRotationKeys, f + startFrame );
				outRot[ f ] = QUAT( tmp.x, tmp.y, tmp.z, tmp.w ).Normalized();
			}
			
			for( unsigned f = 0; f < out->frameCount; ++f )
			{
				aiVector3D tmp( 1, 1, 1 );
				if( na->mNumScalingKeys )
					tmp = _InterpKeys( na->mScalingKeys, na->mNumScalingKeys, f + startFrame );
				outScl[ f ] = V3( tmp.x, tmp.y, tmp.z );
			}
			
			// make transform relative to bone
			for( unsigned f = 0; f < out->frameCount; ++f )
			{
				Mat4 orig = Mat4::CreateSRT( outScl[ f ], outRot[ f ], outPos[ f ] );
				
				Mat4 fxf = orig * invNodeXF;
				
				outScl[ f ] = fxf.GetScale();
				outRot[ f ] = fxf.GetRotationQuaternion();
				outPos[ f ] = fxf.GetTranslation();
			}
			
			_AddAnimTracks( out, na->mNodeName.C_Str(), outPos, outRot, outScl );
		}
		
		return out;
	}
	
	const SGRX_AnimBundleAsset& m_ABA;
	HashTable< int, ImpScene3DHandle > m_sourceData;
};

AnimHandle SGRX_ProcessSingleAnim( const SGRX_AnimBundleAsset& ABA, int i )
{
	AnimProcessor AP( ABA );
	return AP.ProcessAnim( i );
}

bool SGRX_ProcessAnimBundleAsset( const SGRX_AnimBundleAsset& ABA, SGRX_AnimBundle& out )
{
	AnimProcessor AP( ABA );
	for( size_t i = 0; i < ABA.anims.size(); ++i )
	{
		AnimHandle anim = AP.ProcessAnim( i );
		if( !anim )
			return false;
		
		const SGRX_ABAnimation& AN = ABA.anims[ i ];
		if( AN.name.size() )
			anim->m_key = AN.name;
		for( size_t i = 0; i < ABA.sources.size(); ++i )
		{
			if( ABA.sources[ i ].file == SV( AN.source ).until(":") )
			{
				anim->m_key.insert( 0, ABA.sources[ i ].prefix );
				break;
			}
		}
		anim->m_key.insert( 0, ABA.bundlePrefix );
		
		out.anims.push_back( anim );
	}
	return true;
}




static bool CanSkip( bool force, SGRX_Asset* A, uint32_t srcmodtime, RCString path )
{
	return force == false &&
		A->ri.rev_output == A->ri.rev_asset &&
		A->ri.ts_source != 0 &&
		A->ri.ts_output != 0 &&
		A->ri.ts_source == srcmodtime &&
		A->ri.ts_output == FS_FileModTime( path );
}
static const char* GetReasonForBuilding( bool force, SGRX_Asset* A, uint32_t srcmodtime, RCString path )
{
	if( force ) return "forced";
	if( A->ri.rev_output != A->ri.rev_asset ) return "rev. update";
	if( A->ri.ts_source == 0 ) return "no source timestamp";
	if( A->ri.ts_output == 0 ) return "no destination timestamp";
	if( A->ri.ts_source != srcmodtime ) return "source changed";
	if( A->ri.ts_output != FS_FileModTime( path ) ) return "destination changed";
	return "none??";
}

void SGRX_ProcessAssets( SGRX_AssetScript& script, bool force )
{
	puts( "processing assets...");
	
	FS_DirCreate( SGRXPATH_CACHE "/assets" );
	String mapping;
	String legacyMapping;
	
	for( size_t aid = 0; aid < script.assets.size(); ++aid )
	{
		SGRX_Asset* A = script.assets[ aid ];
		RCString path = A->GetPath();
		mapping.append( A->GetMappingString() );
		StringView catPath = script.categories.getcopy( A->outputCategory );
		char bfr[ 520 ], guid[ GUID_STRING_LENGTH + 1 ];
		A->assetGUID.ToCharArray( guid );
		
		switch( A->assetType )
		{
		case SGRX_AT_Texture: {
			SGRX_TextureAsset* TA = A->ToTexture();
			
			sgrx_snprintf( bfr, 520, "%s %s/%s.%s\n", guid,
				StackString<256>(catPath).str,
				StackString<256>(TA->outputName).str,
				SGRX_TextureOutputFormat_Ext( TA->outputType ) );
			legacyMapping.append( bfr );
			if( CanSkip( force, A, FS_FileModTime( TA->sourceFile ), path ) )
				continue;
			
			printf( "(reason: %s)\n",
				GetReasonForBuilding( force, A, FS_FileModTime( TA->sourceFile ), path ) );
			SGRX_IFP32Handle image = SGRX_ProcessTextureAsset( TA );
			if( image == NULL )
				continue;
			
			if( SGRX_SaveImage( path, image, *TA ) )
			{
				TA->ri.rev_output = TA->ri.rev_asset;
				TA->ri.ts_source = FS_FileModTime( TA->sourceFile );
				TA->ri.ts_output = FS_FileModTime( path );
				printf( "|----------- saved! [%dx%dx%d|%d]\n",
					image->GetWidth(), image->GetHeight(), image->GetDepth(), image->GetSides() );
			}
		} break;
		case SGRX_AT_Mesh: {
			SGRX_MeshAsset* MA = A->ToMesh();
			
			sgrx_snprintf( bfr, 520, "%s %s/%s.ssm\n", guid,
				StackString<256>(catPath).str,
				StackString<256>(MA->outputName).str );
			legacyMapping.append( bfr );
			if( CanSkip( force, A, FS_FileModTime( MA->sourceFile ), path ) )
				continue;
			
			MeshHandle mesh = SGRX_ProcessMeshAsset( &script, *MA );
			if( mesh == NULL )
				continue;
			
			ByteArray data;
			if( mesh->ToMeshData( data ) == false )
			{
				puts( "ERROR: failed to serialize mesh" );
				continue;
			}
			
			if( FS_SaveBinaryFile( path, data.data(), data.size() ) )
			{
				MA->ri.rev_output = MA->ri.rev_asset;
				MA->ri.ts_source = FS_FileModTime( MA->sourceFile );
				MA->ri.ts_output = FS_FileModTime( path );
				printf( "|----------- saved!\n" );
			}
			else
			{
				printf( "ERROR: failed to save file to %s\n", path.c_str() );
			}
		} break;
		case SGRX_AT_AnimBundle: {
			SGRX_AnimBundleAsset* ABA = A->ToAnimBundle();
			
			sgrx_snprintf( bfr, 520, "%s %s/%s.anb\n", guid,
				StackString<256>(catPath).str,
				StackString<256>(ABA->outputName).str );
			legacyMapping.append( bfr );
			if( CanSkip( force, A, ABA->LastSourceModTime(), path ) )
				continue;
			
			printf( "| multiple sources => [%s] %s (reason: %s)\n",
				StackString<256>(ABA->outputCategory).str,
				StackString<256>(ABA->outputName).str,
				GetReasonForBuilding( force, A, ABA->LastSourceModTime(), path ) );
			
			SGRX_AnimBundle bundle;
			if( SGRX_ProcessAnimBundleAsset( *ABA, bundle ) == false )
				continue;
			
			ByteArray data;
			ByteWriter bw( &data );
			bundle.Serialize( bw );
			
			if( FS_SaveBinaryFile( path, data.data(), data.size() ) )
			{
				ABA->ri.rev_output = ABA->ri.rev_asset;
				ABA->ri.ts_source = ABA->LastSourceModTime();
				ABA->ri.ts_output = FS_FileModTime( path );
				printf( "|----------- saved!\n" );
			}
			else
			{
				printf( "ERROR: failed to save file to %s\n", path.c_str() );
			}
		} break;
		case SGRX_AT_File: {
			SGRX_FileAsset* FA = A->ToFile();
			
			sgrx_snprintf( bfr, 520, "%s %s/%s\n", guid,
				StackString<256>(catPath).str,
				StackString<256>(FA->outputName).str );
			legacyMapping.append( bfr );
			if( CanSkip( force, A, FS_FileModTime( FA->sourceFile ), path ) )
				continue;
			
			printf( "| %s => [%s] %s (reason: %s)\n",
				StackString<256>(FA->sourceFile).str,
				StackString<256>(FA->outputCategory).str,
				StackString<256>(FA->outputName).str,
				GetReasonForBuilding( force, A, FS_FileModTime( FA->sourceFile ), path ) );
			
			ByteArray data;
			if( FS_LoadBinaryFile( StackPath(FA->sourceFile).str, data ) &&
				FS_SaveBinaryFile( path, data.data(), data.size() ) )
			{
				FA->ri.rev_output = FA->ri.rev_asset;
				FA->ri.ts_source = FS_FileModTime( FA->sourceFile );
				FA->ri.ts_output = FS_FileModTime( path );
				printf( "|----------- saved!\n" );
			}
			else
			{
				printf( "ERROR: failed to copy file to %s\n", path.c_str() );
			}
		} break;
		}
	}
	
	if( !FS_SaveTextFile( SGRXPATH_CACHE "/assets/mapping.txt", mapping ) )
	{
		puts( "ERROR: failed to save mapping string" );
	}
	if( !FS_SaveTextFile( SGRXPATH_CACHE "/assets/mapping.legacy.txt", legacyMapping ) )
	{
		puts( "ERROR: failed to save legacy mapping string" );
	}
}

void SGRX_RemoveAssets( SGRX_AssetScript& script, SGRX_AssetType type )
{
	for( size_t i = 0; i < script.assets.size(); ++i )
	{
		if( script.assets[ i ]->assetType != type )
			continue;
		RCString path = script.assets[ i ]->GetPath();
		String realpath;
		if( FS_FindRealPath( path, realpath ) )
		{
			RemoveFile( realpath );
			printf( "Removed file: %s\n", path.c_str() );
		}
		else
		{
			printf( "ERROR: failed to remove file: %s\n", path.c_str() );
		}
	}
}

