// SGS/CPP-BC
// warning: do not modify this file, it may be regenerated during any build

#include "gfwcore.hpp"

int SGSTextureHandle::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<SGSTextureHandle*>( obj->data )->C = C;
	static_cast<SGSTextureHandle*>( obj->data )->~SGSTextureHandle();
	return SGS_SUCCESS;
}

int SGSTextureHandle::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	_sgsTmpChanger<sgs_Context*> _tmpchg( static_cast<SGSTextureHandle*>( obj->data )->C, C );
	return SGS_SUCCESS;
}

int SGSTextureHandle::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	_sgsTmpChanger<sgs_Context*> _tmpchg( static_cast<SGSTextureHandle*>( obj->data )->C, C );
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "key" ){ sgs_PushVar( C, static_cast<SGSTextureHandle*>( obj->data )->h->m_key ); return SGS_SUCCESS; }
		SGS_CASE( "typeID" ){ sgs_PushVar( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().type ); return SGS_SUCCESS; }
		SGS_CASE( "mipmapCount" ){ sgs_PushVar( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().mipcount ); return SGS_SUCCESS; }
		SGS_CASE( "width" ){ sgs_PushVar( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().width ); return SGS_SUCCESS; }
		SGS_CASE( "height" ){ sgs_PushVar( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().height ); return SGS_SUCCESS; }
		SGS_CASE( "depth" ){ sgs_PushVar( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().depth ); return SGS_SUCCESS; }
		SGS_CASE( "formatID" ){ sgs_PushVar( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().format ); return SGS_SUCCESS; }
		SGS_CASE( "isRenderTexture" ){ sgs_PushVar( C, static_cast<SGSTextureHandle*>( obj->data )->h->m_isRenderTexture ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int SGSTextureHandle::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	_sgsTmpChanger<sgs_Context*> _tmpchg( static_cast<SGSTextureHandle*>( obj->data )->C, C );
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int SGSTextureHandle::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	_sgsTmpChanger<sgs_Context*> _tmpchg( static_cast<SGSTextureHandle*>( obj->data )->C, C );
	char bfr[ 48 ];
	sprintf( bfr, "SGSTextureHandle (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		{ sgs_PushString( C, "\nkey = " ); sgs_DumpData( C, static_cast<SGSTextureHandle*>( obj->data )->h->m_key, depth ).push( C ); }
		{ sgs_PushString( C, "\ntypeID = " ); sgs_DumpData( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().type, depth ).push( C ); }
		{ sgs_PushString( C, "\nmipmapCount = " ); sgs_DumpData( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().mipcount, depth ).push( C ); }
		{ sgs_PushString( C, "\nwidth = " ); sgs_DumpData( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().width, depth ).push( C ); }
		{ sgs_PushString( C, "\nheight = " ); sgs_DumpData( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().height, depth ).push( C ); }
		{ sgs_PushString( C, "\ndepth = " ); sgs_DumpData( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().depth, depth ).push( C ); }
		{ sgs_PushString( C, "\nformatID = " ); sgs_DumpData( C, static_cast<SGSTextureHandle*>( obj->data )->h.GetInfo().format, depth ).push( C ); }
		{ sgs_PushString( C, "\nisRenderTexture = " ); sgs_DumpData( C, static_cast<SGSTextureHandle*>( obj->data )->h->m_isRenderTexture, depth ).push( C ); }
		sgs_StringConcat( C, 16 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst SGSTextureHandle__sgs_funcs[] =
{
	{ NULL, NULL },
};

static int SGSTextureHandle__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		SGSTextureHandle__sgs_funcs,
		-1, "SGSTextureHandle." );
	return 1;
}

static sgs_ObjInterface SGSTextureHandle__sgs_interface =
{
	"SGSTextureHandle",
	SGSTextureHandle::_sgs_destruct, SGSTextureHandle::_sgs_gcmark, SGSTextureHandle::_sgs_getindex, SGSTextureHandle::_sgs_setindex, NULL, NULL, SGSTextureHandle::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface SGSTextureHandle::_sgs_interface(SGSTextureHandle__sgs_interface, SGSTextureHandle__sgs_ifn);


int SGSMeshHandle::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<SGSMeshHandle*>( obj->data )->C = C;
	static_cast<SGSMeshHandle*>( obj->data )->~SGSMeshHandle();
	return SGS_SUCCESS;
}

int SGSMeshHandle::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	_sgsTmpChanger<sgs_Context*> _tmpchg( static_cast<SGSMeshHandle*>( obj->data )->C, C );
	return SGS_SUCCESS;
}

int SGSMeshHandle::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	_sgsTmpChanger<sgs_Context*> _tmpchg( static_cast<SGSMeshHandle*>( obj->data )->C, C );
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "key" ){ sgs_PushVar( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_key ); return SGS_SUCCESS; }
		SGS_CASE( "dataFlags" ){ sgs_PushVar( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_dataFlags ); return SGS_SUCCESS; }
		SGS_CASE( "numParts" ){ sgs_PushVar( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_meshParts.size() ); return SGS_SUCCESS; }
		SGS_CASE( "numBones" ){ sgs_PushVar( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_numBones ); return SGS_SUCCESS; }
		SGS_CASE( "boundsMin" ){ sgs_PushVar( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_boundsMin ); return SGS_SUCCESS; }
		SGS_CASE( "boundsMax" ){ sgs_PushVar( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_boundsMax ); return SGS_SUCCESS; }
		SGS_CASE( "totalVertexCount" ){ sgs_PushVar( C, static_cast<SGSMeshHandle*>( obj->data )->h->GetBufferVertexCount() ); return SGS_SUCCESS; }
		SGS_CASE( "totalIndexCount" ){ sgs_PushVar( C, static_cast<SGSMeshHandle*>( obj->data )->h->GetBufferIndexCount() ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int SGSMeshHandle::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	_sgsTmpChanger<sgs_Context*> _tmpchg( static_cast<SGSMeshHandle*>( obj->data )->C, C );
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int SGSMeshHandle::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	_sgsTmpChanger<sgs_Context*> _tmpchg( static_cast<SGSMeshHandle*>( obj->data )->C, C );
	char bfr[ 45 ];
	sprintf( bfr, "SGSMeshHandle (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		{ sgs_PushString( C, "\nkey = " ); sgs_DumpData( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_key, depth ).push( C ); }
		{ sgs_PushString( C, "\ndataFlags = " ); sgs_DumpData( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_dataFlags, depth ).push( C ); }
		{ sgs_PushString( C, "\nnumParts = " ); sgs_DumpData( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_meshParts.size(), depth ).push( C ); }
		{ sgs_PushString( C, "\nnumBones = " ); sgs_DumpData( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_numBones, depth ).push( C ); }
		{ sgs_PushString( C, "\nboundsMin = " ); sgs_DumpData( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_boundsMin, depth ).push( C ); }
		{ sgs_PushString( C, "\nboundsMax = " ); sgs_DumpData( C, static_cast<SGSMeshHandle*>( obj->data )->h->m_boundsMax, depth ).push( C ); }
		{ sgs_PushString( C, "\ntotalVertexCount = " ); sgs_DumpData( C, static_cast<SGSMeshHandle*>( obj->data )->h->GetBufferVertexCount(), depth ).push( C ); }
		{ sgs_PushString( C, "\ntotalIndexCount = " ); sgs_DumpData( C, static_cast<SGSMeshHandle*>( obj->data )->h->GetBufferIndexCount(), depth ).push( C ); }
		sgs_StringConcat( C, 16 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst SGSMeshHandle__sgs_funcs[] =
{
	{ NULL, NULL },
};

static int SGSMeshHandle__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		SGSMeshHandle__sgs_funcs,
		-1, "SGSMeshHandle." );
	return 1;
}

static sgs_ObjInterface SGSMeshHandle__sgs_interface =
{
	"SGSMeshHandle",
	SGSMeshHandle::_sgs_destruct, SGSMeshHandle::_sgs_gcmark, SGSMeshHandle::_sgs_getindex, SGSMeshHandle::_sgs_setindex, NULL, NULL, SGSMeshHandle::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface SGSMeshHandle::_sgs_interface(SGSMeshHandle__sgs_interface, SGSMeshHandle__sgs_ifn);

