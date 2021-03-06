

#pragma once
#include "engine.hpp"
#include "renderer.hpp"


typedef HashTable< StringView, SGRX_ConvexPointSet* > ConvexPointSetHashTable;
typedef HashTable< StringView, SGRX_ITexture* > TextureHashTable;
typedef HashTable< uint64_t, SGRX_ITexture* > RenderTargetTable;
typedef HashTable< uint64_t, SGRX_IDepthStencilSurface* > DepthStencilSurfTable;
typedef HashTable< StringView, SGRX_IVertexShader* > VertexShaderHashTable;
typedef HashTable< StringView, SGRX_IPixelShader* > PixelShaderHashTable;
typedef HashTable< SGRX_RenderState, SGRX_IRenderState* > RenderStateHashTable;
typedef HashTable< StringView, SGRX_IVertexDecl* > VertexDeclHashTable;
typedef HashTable< SGRX_VtxInputMapKey, SGRX_IVertexInputMapping* > VtxInputMapHashTable;
typedef HashTable< StringView, SGRX_IMesh* > MeshHashTable;
typedef HashTable< StringView, FontHandle > FontHashTable;
typedef HashTable< GenericHandle, int > ResourcePreserveHashTable;


extern bool g_VerboseLogging;
#define VERBOSE g_VerboseLogging


void FS_ResolvePath( StringView& path, char resbfr[50] );
#define FS_RESOLVE_PATH( path ) char resbfr[ 50 ]; \
	FS_ResolvePath( path, resbfr );


template< class K, class V >
struct RasterCache
{
	struct Node
	{
		int32_t child0, child1;
		int32_t x0, y0, x1, y1;
		int32_t lastframe;
		int page;
		bool occupied;
		K key;
		V data;
	};
	struct Page
	{
		TextureHandle texture;
		Array< Node > tree;
		uint32_t framediff;
	};
	struct NodePos
	{
		uint32_t page;
		uint32_t node;
	};
	
	Node* _NodeAlloc( int page, int32_t startnode, int32_t w, int32_t h )
	{
		Page& P = m_pages[ page ];
		Node* me = &P.tree[ startnode ];
		
		// if not leaf..
		if( me->child0 )
		{
			Node* ret = _NodeAlloc( page, me->child0, w, h );
			if( ret )
				return ret;
			// -- no need to refresh pointer since node allocations only precede successful rect allocations
			return _NodeAlloc( page, me->child1, w, h );
		}
		
		// occupied
		if( me->occupied )
			return NULL;
		
		// too small
		if( w > me->x1 - me->x0 || h > me->y1 - me->y0 )
			return NULL;
		
		if( w == me->x1 - me->x0 && h == me->y1 - me->y0 )
			return me;
		
		// split
		int32_t chid0 = P.tree.size();
		me->child0 = chid0;
		me->child1 = me->child0 + 1;
		Node tmpl = { 0, 0, 0, 0, 0, 0, false };
		P.tree.push_back( tmpl );
		P.tree.push_back( tmpl );
		me = &P.tree[ startnode ]; // -- refresh my pointer (in case of reallocation)
		Node* ch0 = &P.tree[ me->child0 ];
		Node* ch1 = &P.tree[ me->child1 ];
		
		int32_t dw = me->x1 - me->x0 - w;
		int32_t dh = me->y1 - me->y0 - h;
		ch0->x0 = me->x0;
		ch0->y0 = me->y0;
		ch1->x1 = me->x1;
		ch1->y1 = me->y1;
		if( dw > dh )
		{
			ch0->x1 = me->x0 + w;
			ch0->y1 = me->y1;
			ch1->x0 = me->x0 + w;
			ch1->y0 = me->y0;
		}
		else
		{
			ch0->x1 = me->x1;
			ch0->y1 = me->y0 + h;
			ch1->x0 = me->x0;
			ch1->y0 = me->y0 + h;
		}
		
		return _NodeAlloc( page, chid0, w, h );
	}
	Node* _AllocItem( int32_t frame, const K& key, const V& data, int32_t w, int32_t h )
	{
		for( size_t i = 0; i < m_pages.size(); ++i )
		{
			Node* node = _NodeAlloc( i, 0, w, h );
			if( node )
			{
				node->key = key;
				node->data = data;
				node->page = i;
				node->occupied = true;
				node->lastframe = frame;
				
				NodePos np = { i, node - m_pages[ i ].tree.data() };
				m_keyNodeMap[ key ] = np;
				
				return node;
			}
		}
		return NULL;
	}
	
	void _AvgFrameDiff( int page, int32_t curframe )
	{
		int64_t counter = 0;
		int64_t count = 0;
		Page& P = m_pages[ page ];
		for( size_t i = 0; i < P.tree.size(); ++i )
		{
			Node& N = P.tree[ i ];
			if( N.occupied )
			{
				counter += curframe - N.lastframe;
				count++;
			}
		}
		P.framediff = count ? counter / count : 0;
	}
	bool _DropLast( int32_t curframe )
	{
		LOG << "Cache page drop requested!";
		uint32_t largestafd = 0;
		int lastpage = -1; 
		
		for( size_t i = 0; i < m_pages.size(); ++i )
		{
			_AvgFrameDiff( i, curframe );
			if( m_pages[ i ].framediff > largestafd )
			{
				largestafd = m_pages[ i ].framediff;
				lastpage = i;
			}
		}
		
		if( lastpage == -1 )
			return false;
		
		Page& P = m_pages[ lastpage ];
		for( size_t i = 0; i < P.tree.size(); ++i )
		{
			Node& N = P.tree[ i ];
			m_keyNodeMap.unset( N.key );
		}
		P.tree.resize(1);
		P.tree[0].child0 = 0;
		P.tree[0].child1 = 0;
		P.tree[0].occupied = false;
		return true;
	}
	
	// for best behavior, increase frame id after each submitted batch
	Node* Alloc( int32_t frame, const K& key, const V& data, int32_t w, int32_t h )
	{
		Node* node = _AllocItem( frame, key, data, w, h );
		if( node )
			return node; // could allocate
		
		// try dropping page with least usage
		if( !_DropLast( frame ) )
			return NULL; // all pages are used currently, severe error (should never happen)
		
		node = _AllocItem( frame, key, data, w, h );
		if( node )
			return node; // could allocate
		
		return NULL;
	}
	Node* Find( int32_t frame, const K& key )
	{
		NodePos np = { -1, 0 };
		np = m_keyNodeMap.getcopy( key, np );
		if( np.page == (uint32_t) -1 )
			return NULL;
		Node* node = &m_pages[ np.page ].tree[ np.node ];
		node->lastframe = frame;
		return node; // already here
	}
	void Resize( int numpages, int32_t w, int32_t h )
	{
		m_pageWidth = w;
		m_pageHeight = h;
		m_pages.resize( numpages );
		Node root =
		{
			0, 0, // children
			0, 0, w, h, // rect
			0, false, 0 // lastframe, page, occupied
		};
		for( int i = 0; i < numpages; ++i )
		{
			m_pages[ i ].texture = GR_CreateTexture( w, h, TEXFMT_RGBA8, 0, 1, NULL );
			m_pages[ i ].tree.clear();
			m_pages[ i ].tree.push_back( root );
		}
		m_keyNodeMap.clear();
	}
	TextureHandle GetPageTexture( int page )
	{
		if( page < 0 || page >= (int) m_pages.size() )
			return TextureHandle();
		return m_pages[ page ].texture;
	}
	// usage:
	// - Find
	// - if found, exit
	// - otherwise, generate data, Alloc, exit
	
	int32_t m_pageWidth;
	int32_t m_pageHeight;
	Array< Page > m_pages;
	Array< int32_t > m_timeStore;
	HashTable< K, NodePos > m_keyNodeMap;
};


#ifdef FREETYPE_INCLUDED
#  define FT_FACE_TYPE FT_Face
#else
#  define FT_FACE_TYPE void*
#endif

void sgrx_int_InitializeFontRendering();
void sgrx_int_FreeFontRendering();


struct SystemFont : SGRX_IFont
{
	SystemFont( bool ol ) : outlined( ol ){ Acquire(); }
	void LoadGlyphInfo( int pxsize, uint32_t ch, SGRX_GlyphInfo* info );
	void GetGlyphBitmap( uint32_t* out, int pitch );
	int GetYOffset( int ){ return -7 - outlined; }
	
	uint8_t* loaded_glyph;
	bool outlined;
};

struct FTFont : SGRX_IFont
{
	FTFont();
	~FTFont();
	void LoadGlyphInfo( int pxsize, uint32_t ch, SGRX_GlyphInfo* info );
	void GetGlyphBitmap( uint32_t* out, int pitch );
	int GetKerning( uint32_t ic1, uint32_t ic2 );
	int GetYOffset( int pxsize );
	void _Resize( int pxsize );
	
	ByteArray data;
	FT_FACE_TYPE face;
	int rendersize;
	bool nohint;
};

struct SVGIconFont : SGRX_IFont
{
	typedef HashTable< uint32_t, struct NSVGimage* > IconTable;
	
	SVGIconFont();
	~SVGIconFont();
	void LoadGlyphInfo( int pxsize, uint32_t ch, SGRX_GlyphInfo* info );
	void GetGlyphBitmap( uint32_t* out, int pitch );
	int GetYOffset( int pxsize ){ return -pxsize; }
	bool _LoadGlyph( uint32_t ch, const StringView& path );
	
	IconTable m_icons;
	struct NSVGimage* m_loaded_img;
	int m_loaded_width;
	int m_loaded_height;
	int m_rendersize;
};

SystemFont* sgrx_int_GetSystemFont( bool ol );
FTFont* sgrx_int_CreateFont( const StringView& path );
SVGIconFont* sgrx_int_CreateSVGIconFont( const StringView& path );


struct FontRenderer
{
	struct CacheKey
	{
		FontHandle font;
		int size;
		uint32_t codepoint;
		
		bool operator == ( const CacheKey& other ) const
		{
			return font == other.font && size == other.size && codepoint == other.codepoint;
		}
	};
	typedef RasterCache< CacheKey, SGRX_GlyphInfo > GlyphCache;
	
	FontRenderer( int pagecount = 2, int pagesize = 1024 );
	~FontRenderer();
	
	// core features
	bool SetFont( const StringView& name, int pxsize );
	void SetCursor( const Vec2& pos );
	
	int PutText( BatchRenderer* br, const StringView& text );
	int SkipText( const StringView& text );
	float GetTextWidth( const StringView& text );
	float GetAdvanceX( uint32_t cpprev, uint32_t cpcurr );
	
	// advanced features
	// TODO
	
	// internal
	GlyphCache::Node* _GetGlyph( uint32_t ch );
	
	Vec2 m_cursor;
	FontHandle m_currentFont;
	int m_currentSize;
	GlyphCache m_cache;
	int32_t m_cache_frame;
};

FINLINE Hash HashVar( const FontRenderer::CacheKey& ck )
{
	return ( (Hash)( ck.font.item ) + ck.size ) ^ ck.codepoint;
}


struct SGRX_Joystick : SGRX_RCRsrc
{
	SGRX_Joystick( int which );
	~SGRX_Joystick();
	
	int m_id;
	SDLSTRUCT SDL_Joystick* m_joystick;
	SDLSTRUCT SDL_GameController* m_gamectrl;
};
typedef Handle< SGRX_Joystick > JoystickHandle;


struct InputAction : SGRX_RefCounted
{
	InputAction( const StringView& nm, float thr = 0.25f ) :
		name( nm ), threshold(thr)
	{}
	
	void _SetState( float x )
	{
		x = clamp( x, -1, 1 );
		data.state = fabsf( x ) >= threshold;
		data.value = data.state ? x : 0;
	}
	void _Advance()
	{
		data.prev_value = data.value;
		data.prev_state = data.state;
	}
	
	RCString name;
	float threshold;
	InputData data; 
};
typedef Handle< InputAction > InputActionHandle;


struct ActionMap
{
	typedef HashTable< StringView, InputActionHandle > NameCmdMap;
	typedef HashTable< ActionInput, InputAction* > InputCmdMap;
	
	void Advance()
	{
		for( size_t i = 0; i < m_nameCmdMap.size(); ++i )
			m_nameCmdMap.item( i ).value->_Advance();
	}
	void Register( InputAction* cmd ){ m_nameCmdMap.set( cmd->name, cmd ); }
	InputAction* FindAction( StringView name ){ return m_nameCmdMap.getcopy( name ); }
	
	bool Map( ActionInput input, InputAction* cmd )
	{
		if( cmd )
			m_inputCmdMap.set( input, cmd );
		else
			m_inputCmdMap.unset( input );
		return !!cmd;
	}
	bool Map( ActionInput input, StringView name )
	{
		return Map( input, FindAction( name ) );
	}
	void Unmap( ActionInput input ){ m_inputCmdMap.unset( input ); }
	InputAction* Get( ActionInput input ){ return m_inputCmdMap.getcopy( input ); }
	
	NameCmdMap m_nameCmdMap;
	InputCmdMap m_inputCmdMap;
};


