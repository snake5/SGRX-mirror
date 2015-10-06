

#include "assetcomp.hpp"

#include <include/FreeImage.h>



static const char* assetimgfiltype_string_table[] =
{
	"resize",
	"sharpen",
	"to_linear",
	"from_linear",
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

bool SGRX_ImageFilter_Resize::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "WIDTH" )
			width = String_ParseInt( value );
		else if( key == "HEIGHT" )
			height = String_ParseInt( value );
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
		"  HEIGHT %d\n",
		width, height );
	out.append( bfr );
}

bool SGRX_ImageFilter_Sharpen::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "FACTOR" )
			factor = String_ParseInt( value );
		else if( key == "FILTER_END" )
			return true;
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
		"  FACTOR %g\n",
		factor );
	out.append( bfr );
}

bool SGRX_ImageFilter_Linear::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "FILTER_END" )
			return true;
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
}

static const char* texoutfmt_string_table[] =
{
	"PNG/RGBA32",
	"STX/RGBA32",
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

bool SGRX_TextureAsset::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "SOURCE" )
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
		else if( key == "FILTER" )
		{
			SGRX_ImgFilterHandle IF;
			if( value == "resize" )
				IF = new SGRX_ImageFilter_Resize;
			else if( value == "sharpen" )
				IF = new SGRX_ImageFilter_Sharpen;
			else if( value == "to_linear" )
				IF = new SGRX_ImageFilter_Linear( false );
			else if( value == "from_linear" )
				IF = new SGRX_ImageFilter_Linear( true );
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
	out.append( " SOURCE " ); out.append( sourceFile ); out.append( "\n" );
	out.append( " OUTPUT_CATEGORY " ); out.append( outputCategory ); out.append( "\n" );
	out.append( " OUTPUT_NAME " ); out.append( outputName ); out.append( "\n" );
	out.append( " OUTPUT_TYPE " );
	out.append( SGRX_TextureOutputFormat_ToString( outputType ) ); out.append( "\n" );
	out.append( " IS_SRGB " ); out.append( isSRGB ? "true" : "false" ); out.append( "\n" );
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

void SGRX_TextureAsset::GetDesc( String& out )
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "%s/%s [%s]",
		StackString<100>(outputCategory).str,
		StackString<100>(outputName).str,
		SGRX_TextureOutputFormat_ToString(outputType) );
	out = bfr;
}

bool SGRX_MeshAsset::Parse( ConfigReader& cread )
{
	StringView key, value;
	while( cread.Read( key, value ) )
	{
		if( key == "SOURCE" )
			sourceFile = value;
		else if( key == "OUTPUT_CATEGORY" )
			outputCategory = value;
		else if( key == "OUTPUT_NAME" )
			outputName = value;
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
	out.append( " SOURCE " ); out.append( sourceFile ); out.append( "\n" );
	out.append( " OUTPUT_CATEGORY " ); out.append( outputCategory ); out.append( "\n" );
	out.append( " OUTPUT_NAME " ); out.append( outputName ); out.append( "\n" );
	out.append( "MESH_END\n" );
}

void SGRX_MeshAsset::GetDesc( String& out )
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
			textureAssets.push_back( SGRX_TextureAsset() );
			if( textureAssets.last().Parse( cread ) == false )
				return false;
		}
		else if( key == "MESH" )
		{
			meshAssets.push_back( SGRX_MeshAsset() );
			if( meshAssets.last().Parse( cread ) == false )
				return false;
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
	
	for( size_t i = 0; i < textureAssets.size(); ++i )
	{
		textureAssets[ i ].Generate( out );
	}
	
	for( size_t i = 0; i < meshAssets.size(); ++i )
	{
		meshAssets[ i ].Generate( out );
	}
}

bool SGRX_AssetScript::Load( const StringView& path )
{
	String data;
	if( FS_LoadTextFile( path, data ) == false )
		return false;
	
	ConfigReader cread( data );
	return Parse( cread );
}

bool SGRX_AssetScript::Save( const StringView& path )
{
	String data;
	Generate( data );
	
	return FS_SaveTextFile( path, data );
}



fi_handle g_load_address;

inline unsigned _stdcall SGRXFI_ReadProc(void *buffer, unsigned size, unsigned count, fi_handle handle)
{
	BYTE *tmp = (BYTE *)buffer;

	for (unsigned c = 0; c < count; c++)
	{
		memcpy(tmp, g_load_address, size);
		
		g_load_address = (BYTE *)g_load_address + size;
		
		tmp += size;
	}
	
	return count;
}

inline unsigned _stdcall SGRXFI_WriteProc(void *buffer, unsigned size, unsigned count, fi_handle handle)
{
	return size;
}

inline int _stdcall SGRXFI_SeekProc(fi_handle handle, long offset, int origin)
{
	ASSERT(origin != SEEK_END);
	
	if (origin == SEEK_SET)
		g_load_address = (BYTE *)handle + offset;
	else
		g_load_address = (BYTE *)g_load_address + offset;
	
	return 0;
}

inline long _stdcall SGRXFI_TellProc(fi_handle handle)
{
	ASSERT((int)handle > (int)g_load_address);

	return ((int)g_load_address - (int)handle);
}

SGRX_IFP32Handle SGRX_LoadImage( const StringView& path )
{
	FreeImageIO io;
	io.read_proc  = SGRXFI_ReadProc;
	io.write_proc = SGRXFI_WriteProc;
	io.tell_proc  = SGRXFI_TellProc;
	io.seek_proc  = SGRXFI_SeekProc;
	
	ByteArray data;
	if( FS_LoadBinaryFile( path, data ) == false )
	{
		LOG_ERROR << "Failed to load image file: " << path;
		return NULL;
	}
	
	FIBITMAP *dib = FreeImage_LoadFromHandle( FIF_TIFF, &io, (fi_handle)data.data() );
	FreeImage_Unload(dib);
	return NULL;
}

void SGRX_ProcessAssets( const SGRX_AssetScript& script )
{
	puts( "processing assets...");
}
