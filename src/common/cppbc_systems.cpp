// SGS/CPP-BC
// warning: do not modify this file, it may be regenerated during any build

#include "systems.hpp"

static int _sgs_method__MessagingSystem__Add( SGS_CTX )
{
	MessagingSystem* data; if( !SGS_PARSE_METHOD( C, MessagingSystem::_sgs_interface, data, MessagingSystem, Add ) ) return 0;
	data->sgsAddMsg( sgs_GetVar<int>()(C,0), sgs_GetVar<StringView>()(C,1), sgs_GetVar<float>()(C,2) ); return 0;
}

int MessagingSystem::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<MessagingSystem*>( obj->data )->~MessagingSystem();
	return SGS_SUCCESS;
}

int MessagingSystem::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	return SGS_SUCCESS;
}

int MessagingSystem::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int MessagingSystem::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int MessagingSystem::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	char bfr[ 47 ];
	sprintf( bfr, "MessagingSystem (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		sgs_StringConcat( C, 0 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst MessagingSystem__sgs_funcs[] =
{
	{ "Add", _sgs_method__MessagingSystem__Add },
	{ NULL, NULL },
};

static int MessagingSystem__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		MessagingSystem__sgs_funcs,
		-1, "MessagingSystem." );
	return 1;
}

static sgs_ObjInterface MessagingSystem__sgs_interface =
{
	"MessagingSystem",
	NULL, MessagingSystem::_sgs_gcmark, MessagingSystem::_sgs_getindex, MessagingSystem::_sgs_setindex, NULL, NULL, MessagingSystem::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface MessagingSystem::_sgs_interface(MessagingSystem__sgs_interface, MessagingSystem__sgs_ifn);


static int _sgs_method__ObjectiveSystem__Add( SGS_CTX )
{
	ObjectiveSystem* data; if( !SGS_PARSE_METHOD( C, ObjectiveSystem::_sgs_interface, data, ObjectiveSystem, Add ) ) return 0;
	sgs_PushVar(C,data->sgsAddObj( sgs_GetVar<StringView>()(C,0), sgs_GetVar<int>()(C,1), sgs_GetVar<StringView>()(C,2), sgs_GetVar<bool>()(C,3), sgs_GetVar<Vec3>()(C,4) )); return 1;
}

static int _sgs_method__ObjectiveSystem__GetTitle( SGS_CTX )
{
	ObjectiveSystem* data; if( !SGS_PARSE_METHOD( C, ObjectiveSystem::_sgs_interface, data, ObjectiveSystem, GetTitle ) ) return 0;
	sgs_PushVar(C,data->sgsGetTitle( sgs_GetVar<int>()(C,0) )); return 1;
}

static int _sgs_method__ObjectiveSystem__SetTitle( SGS_CTX )
{
	ObjectiveSystem* data; if( !SGS_PARSE_METHOD( C, ObjectiveSystem::_sgs_interface, data, ObjectiveSystem, SetTitle ) ) return 0;
	data->sgsSetTitle( sgs_GetVar<int>()(C,0), sgs_GetVar<StringView>()(C,1) ); return 0;
}

static int _sgs_method__ObjectiveSystem__GetState( SGS_CTX )
{
	ObjectiveSystem* data; if( !SGS_PARSE_METHOD( C, ObjectiveSystem::_sgs_interface, data, ObjectiveSystem, GetState ) ) return 0;
	sgs_PushVar(C,data->sgsGetState( sgs_GetVar<int>()(C,0) )); return 1;
}

static int _sgs_method__ObjectiveSystem__SetState( SGS_CTX )
{
	ObjectiveSystem* data; if( !SGS_PARSE_METHOD( C, ObjectiveSystem::_sgs_interface, data, ObjectiveSystem, SetState ) ) return 0;
	data->sgsSetState( sgs_GetVar<int>()(C,0), sgs_GetVar<int>()(C,1) ); return 0;
}

static int _sgs_method__ObjectiveSystem__SetLocation( SGS_CTX )
{
	ObjectiveSystem* data; if( !SGS_PARSE_METHOD( C, ObjectiveSystem::_sgs_interface, data, ObjectiveSystem, SetLocation ) ) return 0;
	data->sgsSetLocation( sgs_GetVar<int>()(C,0), sgs_GetVar<Vec3>()(C,1) ); return 0;
}

int ObjectiveSystem::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<ObjectiveSystem*>( obj->data )->~ObjectiveSystem();
	return SGS_SUCCESS;
}

int ObjectiveSystem::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	return SGS_SUCCESS;
}

int ObjectiveSystem::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int ObjectiveSystem::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int ObjectiveSystem::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	char bfr[ 47 ];
	sprintf( bfr, "ObjectiveSystem (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		sgs_StringConcat( C, 0 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst ObjectiveSystem__sgs_funcs[] =
{
	{ "Add", _sgs_method__ObjectiveSystem__Add },
	{ "GetTitle", _sgs_method__ObjectiveSystem__GetTitle },
	{ "SetTitle", _sgs_method__ObjectiveSystem__SetTitle },
	{ "GetState", _sgs_method__ObjectiveSystem__GetState },
	{ "SetState", _sgs_method__ObjectiveSystem__SetState },
	{ "SetLocation", _sgs_method__ObjectiveSystem__SetLocation },
	{ NULL, NULL },
};

static int ObjectiveSystem__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		ObjectiveSystem__sgs_funcs,
		-1, "ObjectiveSystem." );
	return 1;
}

static sgs_ObjInterface ObjectiveSystem__sgs_interface =
{
	"ObjectiveSystem",
	NULL, ObjectiveSystem::_sgs_gcmark, ObjectiveSystem::_sgs_getindex, ObjectiveSystem::_sgs_setindex, NULL, NULL, ObjectiveSystem::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface ObjectiveSystem::_sgs_interface(ObjectiveSystem__sgs_interface, ObjectiveSystem__sgs_ifn);


static int _sgs_method__HelpTextSystem__Clear( SGS_CTX )
{
	HelpTextSystem* data; if( !SGS_PARSE_METHOD( C, HelpTextSystem::_sgs_interface, data, HelpTextSystem, Clear ) ) return 0;
	data->sgsClear(  ); return 0;
}

static int _sgs_method__HelpTextSystem__SetText( SGS_CTX )
{
	HelpTextSystem* data; if( !SGS_PARSE_METHOD( C, HelpTextSystem::_sgs_interface, data, HelpTextSystem, SetText ) ) return 0;
	data->sgsSetText( sgs_GetVar<StringView>()(C,0), sgs_GetVar<float>()(C,1), sgs_GetVar<float>()(C,2), sgs_GetVar<float>()(C,3) ); return 0;
}

int HelpTextSystem::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<HelpTextSystem*>( obj->data )->~HelpTextSystem();
	return SGS_SUCCESS;
}

int HelpTextSystem::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	return SGS_SUCCESS;
}

int HelpTextSystem::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "text" ){ sgs_PushVar( C, static_cast<HelpTextSystem*>( obj->data )->m_text ); return SGS_SUCCESS; }
		SGS_CASE( "alpha" ){ sgs_PushVar( C, static_cast<HelpTextSystem*>( obj->data )->m_alpha ); return SGS_SUCCESS; }
		SGS_CASE( "fadeTime" ){ sgs_PushVar( C, static_cast<HelpTextSystem*>( obj->data )->m_fadeTime ); return SGS_SUCCESS; }
		SGS_CASE( "fadeTo" ){ sgs_PushVar( C, static_cast<HelpTextSystem*>( obj->data )->m_fadeTo ); return SGS_SUCCESS; }
		SGS_CASE( "fontSize" ){ if( !( static_cast<HelpTextSystem*>( obj->data )->renderer ) ){ return SGS_EINPROC; } sgs_PushVar( C, static_cast<HelpTextSystem*>( obj->data )->renderer->fontSize ); return SGS_SUCCESS; }
		SGS_CASE( "centerPos" ){ if( !( static_cast<HelpTextSystem*>( obj->data )->renderer ) ){ return SGS_EINPROC; } sgs_PushVar( C, static_cast<HelpTextSystem*>( obj->data )->renderer->centerPos ); return SGS_SUCCESS; }
		SGS_CASE( "lineHeightFactor" ){ if( !( static_cast<HelpTextSystem*>( obj->data )->renderer ) ){ return SGS_EINPROC; } sgs_PushVar( C, static_cast<HelpTextSystem*>( obj->data )->renderer->lineHeightFactor ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int HelpTextSystem::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "text" ){ static_cast<HelpTextSystem*>( obj->data )->m_text = sgs_GetVar<String>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "alpha" ){ static_cast<HelpTextSystem*>( obj->data )->m_alpha = sgs_GetVar<float>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "fadeTime" ){ static_cast<HelpTextSystem*>( obj->data )->m_fadeTime = sgs_GetVar<float>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "fadeTo" ){ static_cast<HelpTextSystem*>( obj->data )->m_fadeTo = sgs_GetVar<float>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "fontSize" ){ if( !( static_cast<HelpTextSystem*>( obj->data )->renderer ) ){ return SGS_EINPROC; } static_cast<HelpTextSystem*>( obj->data )->renderer->fontSize = sgs_GetVar<int>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "centerPos" ){ if( !( static_cast<HelpTextSystem*>( obj->data )->renderer ) ){ return SGS_EINPROC; } static_cast<HelpTextSystem*>( obj->data )->renderer->centerPos = sgs_GetVar<Vec2>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "lineHeightFactor" ){ if( !( static_cast<HelpTextSystem*>( obj->data )->renderer ) ){ return SGS_EINPROC; } static_cast<HelpTextSystem*>( obj->data )->renderer->lineHeightFactor = sgs_GetVar<float>()( C, 1 ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int HelpTextSystem::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	char bfr[ 46 ];
	sprintf( bfr, "HelpTextSystem (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		{ sgs_PushString( C, "\ntext = " ); sgs_DumpData( C, static_cast<HelpTextSystem*>( obj->data )->m_text, depth ).push( C ); }
		{ sgs_PushString( C, "\nalpha = " ); sgs_DumpData( C, static_cast<HelpTextSystem*>( obj->data )->m_alpha, depth ).push( C ); }
		{ sgs_PushString( C, "\nfadeTime = " ); sgs_DumpData( C, static_cast<HelpTextSystem*>( obj->data )->m_fadeTime, depth ).push( C ); }
		{ sgs_PushString( C, "\nfadeTo = " ); sgs_DumpData( C, static_cast<HelpTextSystem*>( obj->data )->m_fadeTo, depth ).push( C ); }
		{ sgs_PushString( C, "\nfontSize = " ); if( !( static_cast<HelpTextSystem*>( obj->data )->renderer ) ) sgs_PushString( C, "<inaccessible>" ); else sgs_DumpData( C, static_cast<HelpTextSystem*>( obj->data )->renderer->fontSize, depth ).push( C ); }
		{ sgs_PushString( C, "\ncenterPos = " ); if( !( static_cast<HelpTextSystem*>( obj->data )->renderer ) ) sgs_PushString( C, "<inaccessible>" ); else sgs_DumpData( C, static_cast<HelpTextSystem*>( obj->data )->renderer->centerPos, depth ).push( C ); }
		{ sgs_PushString( C, "\nlineHeightFactor = " ); if( !( static_cast<HelpTextSystem*>( obj->data )->renderer ) ) sgs_PushString( C, "<inaccessible>" ); else sgs_DumpData( C, static_cast<HelpTextSystem*>( obj->data )->renderer->lineHeightFactor, depth ).push( C ); }
		sgs_StringConcat( C, 14 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst HelpTextSystem__sgs_funcs[] =
{
	{ "Clear", _sgs_method__HelpTextSystem__Clear },
	{ "SetText", _sgs_method__HelpTextSystem__SetText },
	{ NULL, NULL },
};

static int HelpTextSystem__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		HelpTextSystem__sgs_funcs,
		-1, "HelpTextSystem." );
	return 1;
}

static sgs_ObjInterface HelpTextSystem__sgs_interface =
{
	"HelpTextSystem",
	NULL, HelpTextSystem::_sgs_gcmark, HelpTextSystem::_sgs_getindex, HelpTextSystem::_sgs_setindex, NULL, NULL, HelpTextSystem::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface HelpTextSystem::_sgs_interface(HelpTextSystem__sgs_interface, HelpTextSystem__sgs_ifn);


static int _sgs_method__FlareSystem__Update( SGS_CTX )
{
	FlareSystem* data; if( !SGS_PARSE_METHOD( C, FlareSystem::_sgs_interface, data, FlareSystem, Update ) ) return 0;
	data->sgsUpdate( sgs_GetVarObj<void>()(C,0), sgs_GetVar<Vec3>()(C,1), sgs_GetVar<Vec3>()(C,2), sgs_GetVar<float>()(C,3), sgs_GetVar<bool>()(C,4) ); return 0;
}

static int _sgs_method__FlareSystem__Remove( SGS_CTX )
{
	FlareSystem* data; if( !SGS_PARSE_METHOD( C, FlareSystem::_sgs_interface, data, FlareSystem, Remove ) ) return 0;
	data->sgsRemove( sgs_GetVarObj<void>()(C,0) ); return 0;
}

int FlareSystem::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<FlareSystem*>( obj->data )->~FlareSystem();
	return SGS_SUCCESS;
}

int FlareSystem::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	return SGS_SUCCESS;
}

int FlareSystem::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "layers" ){ sgs_PushVar( C, static_cast<FlareSystem*>( obj->data )->m_layers ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int FlareSystem::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "layers" ){ static_cast<FlareSystem*>( obj->data )->m_layers = sgs_GetVar<uint32_t>()( C, 1 ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int FlareSystem::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	char bfr[ 43 ];
	sprintf( bfr, "FlareSystem (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		{ sgs_PushString( C, "\nlayers = " ); sgs_DumpData( C, static_cast<FlareSystem*>( obj->data )->m_layers, depth ).push( C ); }
		sgs_StringConcat( C, 2 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst FlareSystem__sgs_funcs[] =
{
	{ "Update", _sgs_method__FlareSystem__Update },
	{ "Remove", _sgs_method__FlareSystem__Remove },
	{ NULL, NULL },
};

static int FlareSystem__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		FlareSystem__sgs_funcs,
		-1, "FlareSystem." );
	return 1;
}

static sgs_ObjInterface FlareSystem__sgs_interface =
{
	"FlareSystem",
	NULL, FlareSystem::_sgs_gcmark, FlareSystem::_sgs_getindex, FlareSystem::_sgs_setindex, NULL, NULL, FlareSystem::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface FlareSystem::_sgs_interface(FlareSystem__sgs_interface, FlareSystem__sgs_ifn);


static int _sgs_method__ScriptedSequenceSystem__Start( SGS_CTX )
{
	ScriptedSequenceSystem* data; if( !SGS_PARSE_METHOD( C, ScriptedSequenceSystem::_sgs_interface, data, ScriptedSequenceSystem, Start ) ) return 0;
	data->sgsStart( sgs_GetVar<sgsVariable>()(C,0), sgs_GetVar<float>()(C,1) ); return 0;
}

int ScriptedSequenceSystem::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<ScriptedSequenceSystem*>( obj->data )->~ScriptedSequenceSystem();
	return SGS_SUCCESS;
}

int ScriptedSequenceSystem::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	return SGS_SUCCESS;
}

int ScriptedSequenceSystem::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "func" ){ sgs_PushVar( C, static_cast<ScriptedSequenceSystem*>( obj->data )->m_func ); return SGS_SUCCESS; }
		SGS_CASE( "time" ){ sgs_PushVar( C, static_cast<ScriptedSequenceSystem*>( obj->data )->m_time ); return SGS_SUCCESS; }
		SGS_CASE( "subtitle" ){ sgs_PushVar( C, static_cast<ScriptedSequenceSystem*>( obj->data )->m_subtitle ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int ScriptedSequenceSystem::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "func" ){ static_cast<ScriptedSequenceSystem*>( obj->data )->m_func = sgs_GetVar<sgsVariable>()( C, 1 );
			static_cast<ScriptedSequenceSystem*>( obj->data )->_StartCutscene(); return SGS_SUCCESS; }
		SGS_CASE( "time" ){ static_cast<ScriptedSequenceSystem*>( obj->data )->m_time = sgs_GetVar<float>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "subtitle" ){ static_cast<ScriptedSequenceSystem*>( obj->data )->m_subtitle = sgs_GetVar<String>()( C, 1 ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int ScriptedSequenceSystem::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	char bfr[ 54 ];
	sprintf( bfr, "ScriptedSequenceSystem (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		{ sgs_PushString( C, "\nfunc = " ); sgs_DumpData( C, static_cast<ScriptedSequenceSystem*>( obj->data )->m_func, depth ).push( C ); }
		{ sgs_PushString( C, "\ntime = " ); sgs_DumpData( C, static_cast<ScriptedSequenceSystem*>( obj->data )->m_time, depth ).push( C ); }
		{ sgs_PushString( C, "\nsubtitle = " ); sgs_DumpData( C, static_cast<ScriptedSequenceSystem*>( obj->data )->m_subtitle, depth ).push( C ); }
		sgs_StringConcat( C, 6 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst ScriptedSequenceSystem__sgs_funcs[] =
{
	{ "Start", _sgs_method__ScriptedSequenceSystem__Start },
	{ NULL, NULL },
};

static int ScriptedSequenceSystem__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		ScriptedSequenceSystem__sgs_funcs,
		-1, "ScriptedSequenceSystem." );
	return 1;
}

static sgs_ObjInterface ScriptedSequenceSystem__sgs_interface =
{
	"ScriptedSequenceSystem",
	NULL, ScriptedSequenceSystem::_sgs_gcmark, ScriptedSequenceSystem::_sgs_getindex, ScriptedSequenceSystem::_sgs_setindex, NULL, NULL, ScriptedSequenceSystem::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface ScriptedSequenceSystem::_sgs_interface(ScriptedSequenceSystem__sgs_interface, ScriptedSequenceSystem__sgs_ifn);


static int _sgs_method__MusicSystem__SetTrack( SGS_CTX )
{
	MusicSystem* data; if( !SGS_PARSE_METHOD( C, MusicSystem::_sgs_interface, data, MusicSystem, SetTrack ) ) return 0;
	data->sgsSetTrack( sgs_GetVar<StringView>()(C,0) ); return 0;
}

static int _sgs_method__MusicSystem__SetVar( SGS_CTX )
{
	MusicSystem* data; if( !SGS_PARSE_METHOD( C, MusicSystem::_sgs_interface, data, MusicSystem, SetVar ) ) return 0;
	data->sgsSetVar( sgs_GetVar<StringView>()(C,0), sgs_GetVar<float>()(C,1) ); return 0;
}

int MusicSystem::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<MusicSystem*>( obj->data )->~MusicSystem();
	return SGS_SUCCESS;
}

int MusicSystem::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	return SGS_SUCCESS;
}

int MusicSystem::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int MusicSystem::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int MusicSystem::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	char bfr[ 43 ];
	sprintf( bfr, "MusicSystem (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		sgs_StringConcat( C, 0 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst MusicSystem__sgs_funcs[] =
{
	{ "SetTrack", _sgs_method__MusicSystem__SetTrack },
	{ "SetVar", _sgs_method__MusicSystem__SetVar },
	{ NULL, NULL },
};

static int MusicSystem__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		MusicSystem__sgs_funcs,
		-1, "MusicSystem." );
	return 1;
}

static sgs_ObjInterface MusicSystem__sgs_interface =
{
	"MusicSystem",
	NULL, MusicSystem::_sgs_gcmark, MusicSystem::_sgs_getindex, MusicSystem::_sgs_setindex, NULL, NULL, MusicSystem::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface MusicSystem::_sgs_interface(MusicSystem__sgs_interface, MusicSystem__sgs_ifn);


int AIFact::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<AIFact*>( obj->data )->~AIFact();
	return SGS_SUCCESS;
}

int AIFact::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	return SGS_SUCCESS;
}

int AIFact::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "id" ){ sgs_PushVar( C, static_cast<AIFact*>( obj->data )->id ); return SGS_SUCCESS; }
		SGS_CASE( "ref" ){ sgs_PushVar( C, static_cast<AIFact*>( obj->data )->ref ); return SGS_SUCCESS; }
		SGS_CASE( "type" ){ sgs_PushVar( C, static_cast<AIFact*>( obj->data )->type ); return SGS_SUCCESS; }
		SGS_CASE( "position" ){ sgs_PushVar( C, static_cast<AIFact*>( obj->data )->position ); return SGS_SUCCESS; }
		SGS_CASE( "created" ){ sgs_PushVar( C, static_cast<AIFact*>( obj->data )->created ); return SGS_SUCCESS; }
		SGS_CASE( "expires" ){ sgs_PushVar( C, static_cast<AIFact*>( obj->data )->expires ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int AIFact::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
		SGS_CASE( "id" ){ static_cast<AIFact*>( obj->data )->id = sgs_GetVar<uint32_t>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "ref" ){ static_cast<AIFact*>( obj->data )->ref = sgs_GetVar<uint32_t>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "type" ){ static_cast<AIFact*>( obj->data )->type = sgs_GetVar<uint32_t>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "position" ){ static_cast<AIFact*>( obj->data )->position = sgs_GetVar<Vec3>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "created" ){ static_cast<AIFact*>( obj->data )->created = sgs_GetVar<TimeVal>()( C, 1 ); return SGS_SUCCESS; }
		SGS_CASE( "expires" ){ static_cast<AIFact*>( obj->data )->expires = sgs_GetVar<TimeVal>()( C, 1 ); return SGS_SUCCESS; }
	SGS_END_INDEXFUNC;
}

int AIFact::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	char bfr[ 38 ];
	sprintf( bfr, "AIFact (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		{ sgs_PushString( C, "\nid = " ); sgs_DumpData( C, static_cast<AIFact*>( obj->data )->id, depth ).push( C ); }
		{ sgs_PushString( C, "\nref = " ); sgs_DumpData( C, static_cast<AIFact*>( obj->data )->ref, depth ).push( C ); }
		{ sgs_PushString( C, "\ntype = " ); sgs_DumpData( C, static_cast<AIFact*>( obj->data )->type, depth ).push( C ); }
		{ sgs_PushString( C, "\nposition = " ); sgs_DumpData( C, static_cast<AIFact*>( obj->data )->position, depth ).push( C ); }
		{ sgs_PushString( C, "\ncreated = " ); sgs_DumpData( C, static_cast<AIFact*>( obj->data )->created, depth ).push( C ); }
		{ sgs_PushString( C, "\nexpires = " ); sgs_DumpData( C, static_cast<AIFact*>( obj->data )->expires, depth ).push( C ); }
		sgs_StringConcat( C, 12 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst AIFact__sgs_funcs[] =
{
	{ NULL, NULL },
};

static int AIFact__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		AIFact__sgs_funcs,
		-1, "AIFact." );
	return 1;
}

static sgs_ObjInterface AIFact__sgs_interface =
{
	"AIFact",
	AIFact::_sgs_destruct, AIFact::_sgs_gcmark, AIFact::_sgs_getindex, AIFact::_sgs_setindex, NULL, NULL, AIFact::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface AIFact::_sgs_interface(AIFact__sgs_interface, AIFact__sgs_ifn);


static int _sgs_method__AIDBSystem__AddSound( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, AddSound ) ) return 0;
	data->sgsAddSound( sgs_GetVar<Vec3>()(C,0), sgs_GetVar<float>()(C,1), sgs_GetVar<float>()(C,2), sgs_GetVar<int>()(C,3) ); return 0;
}

static int _sgs_method__AIDBSystem__HasFact( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, HasFact ) ) return 0;
	sgs_PushVar(C,data->sgsHasFact( sgs_GetVar<uint32_t>()(C,0) )); return 1;
}

static int _sgs_method__AIDBSystem__HasRecentFact( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, HasRecentFact ) ) return 0;
	sgs_PushVar(C,data->sgsHasRecentFact( sgs_GetVar<uint32_t>()(C,0), sgs_GetVar<TimeVal>()(C,1) )); return 1;
}

static int _sgs_method__AIDBSystem__GetRecentFact( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, GetRecentFact ) ) return 0;
	return data->sgsGetRecentFact( C, sgs_GetVar<uint32_t>()(C,0), sgs_GetVar<TimeVal>()(C,1) );
}

static int _sgs_method__AIDBSystem__InsertFact( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, InsertFact ) ) return 0;
	data->sgsInsertFact( sgs_GetVar<uint32_t>()(C,0), sgs_GetVar<Vec3>()(C,1), sgs_GetVar<TimeVal>()(C,2), sgs_GetVar<TimeVal>()(C,3), sgs_GetVar<uint32_t>()(C,4) ); return 0;
}

static int _sgs_method__AIDBSystem__UpdateFact( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, UpdateFact ) ) return 0;
	sgs_PushVar(C,data->sgsUpdateFact( C, sgs_GetVar<uint32_t>()(C,0), sgs_GetVar<Vec3>()(C,1), sgs_GetVar<float>()(C,2), sgs_GetVar<TimeVal>()(C,3), sgs_GetVar<TimeVal>()(C,4), sgs_GetVar<uint32_t>()(C,5), sgs_GetVar<bool>()(C,6) )); return 1;
}

static int _sgs_method__AIDBSystem__InsertOrUpdateFact( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, InsertOrUpdateFact ) ) return 0;
	data->sgsInsertOrUpdateFact( C, sgs_GetVar<uint32_t>()(C,0), sgs_GetVar<Vec3>()(C,1), sgs_GetVar<float>()(C,2), sgs_GetVar<TimeVal>()(C,3), sgs_GetVar<TimeVal>()(C,4), sgs_GetVar<uint32_t>()(C,5), sgs_GetVar<bool>()(C,6) ); return 0;
}

static int _sgs_method__AIDBSystem__GetRoomList( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, GetRoomList ) ) return 0;
	return data->sgsGetRoomList( C );
}

static int _sgs_method__AIDBSystem__GetRoomNameByPos( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, GetRoomNameByPos ) ) return 0;
	sgs_PushVar(C,data->sgsGetRoomNameByPos( C, sgs_GetVar<Vec3>()(C,0) )); return 1;
}

static int _sgs_method__AIDBSystem__GetRoomByPos( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, GetRoomByPos ) ) return 0;
	return data->sgsGetRoomByPos( C, sgs_GetVar<Vec3>()(C,0) );
}

static int _sgs_method__AIDBSystem__GetRoomPoints( SGS_CTX )
{
	AIDBSystem* data; if( !SGS_PARSE_METHOD( C, AIDBSystem::_sgs_interface, data, AIDBSystem, GetRoomPoints ) ) return 0;
	return data->sgsGetRoomPoints( C, sgs_GetVar<StringView>()(C,0) );
}

int AIDBSystem::_sgs_destruct( SGS_CTX, sgs_VarObj* obj )
{
	static_cast<AIDBSystem*>( obj->data )->~AIDBSystem();
	return SGS_SUCCESS;
}

int AIDBSystem::_sgs_gcmark( SGS_CTX, sgs_VarObj* obj )
{
	return SGS_SUCCESS;
}

int AIDBSystem::_sgs_getindex( SGS_ARGS_GETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int AIDBSystem::_sgs_setindex( SGS_ARGS_SETINDEXFUNC )
{
	SGS_BEGIN_INDEXFUNC
	SGS_END_INDEXFUNC;
}

int AIDBSystem::_sgs_dump( SGS_CTX, sgs_VarObj* obj, int depth )
{
	char bfr[ 42 ];
	sprintf( bfr, "AIDBSystem (%p) %s", obj->data, depth > 0 ? "\n{" : " ..." );
	sgs_PushString( C, bfr );
	if( depth > 0 )
	{
		sgs_StringConcat( C, 0 );
		sgs_PadString( C );
		sgs_PushString( C, "\n}" );
		sgs_StringConcat( C, 3 );
	}
	return SGS_SUCCESS;
}

static sgs_RegFuncConst AIDBSystem__sgs_funcs[] =
{
	{ "AddSound", _sgs_method__AIDBSystem__AddSound },
	{ "HasFact", _sgs_method__AIDBSystem__HasFact },
	{ "HasRecentFact", _sgs_method__AIDBSystem__HasRecentFact },
	{ "GetRecentFact", _sgs_method__AIDBSystem__GetRecentFact },
	{ "InsertFact", _sgs_method__AIDBSystem__InsertFact },
	{ "UpdateFact", _sgs_method__AIDBSystem__UpdateFact },
	{ "InsertOrUpdateFact", _sgs_method__AIDBSystem__InsertOrUpdateFact },
	{ "GetRoomList", _sgs_method__AIDBSystem__GetRoomList },
	{ "GetRoomNameByPos", _sgs_method__AIDBSystem__GetRoomNameByPos },
	{ "GetRoomByPos", _sgs_method__AIDBSystem__GetRoomByPos },
	{ "GetRoomPoints", _sgs_method__AIDBSystem__GetRoomPoints },
	{ NULL, NULL },
};

static int AIDBSystem__sgs_ifn( SGS_CTX )
{
	sgs_CreateDict( C, NULL, 0 );
	sgs_StoreFuncConsts( C, sgs_StackItem( C, -1 ),
		AIDBSystem__sgs_funcs,
		-1, "AIDBSystem." );
	return 1;
}

static sgs_ObjInterface AIDBSystem__sgs_interface =
{
	"AIDBSystem",
	NULL, AIDBSystem::_sgs_gcmark, AIDBSystem::_sgs_getindex, AIDBSystem::_sgs_setindex, NULL, NULL, AIDBSystem::_sgs_dump, NULL, NULL, NULL, 
};
_sgsInterface AIDBSystem::_sgs_interface(AIDBSystem__sgs_interface, AIDBSystem__sgs_ifn);
