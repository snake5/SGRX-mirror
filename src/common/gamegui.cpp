

#include "gamegui.hpp"



struct FocusSearch
{
	FocusSearch( GameUIControl* c, Vec2 d ) : bestdist(FLT_MAX), bestctrl(NULL),
		exclctrl(c), dir(d), rx0(c->rx0), ry0(c->ry0), rx1(c->rx1), ry1(c->ry1)
	{
		pos = V2( ( rx0 + rx1 ) * 0.5f, ( ry0 + ry1 ) * 0.5f );
		mymaxproj = GetMaxProj( c, d );
	}
	float GetMinProj( GameUIControl* c, Vec2 d )
	{
		return TMIN(
			TMIN(
				Vec2Dot( V2(c->rx0,c->ry0), d ),
				Vec2Dot( V2(c->rx1,c->ry0), d )
			),
			TMIN(
				Vec2Dot( V2(c->rx0,c->ry1), d ),
				Vec2Dot( V2(c->rx1,c->ry1), d )
			)
		);
	}
	float GetMaxProj( GameUIControl* c, Vec2 d ){ return -GetMinProj( c, -d ); }
	float GetDistance( GameUIControl* c )
	{
		float xoff = TMIN( c->rx1 - exclctrl->rx0, exclctrl->rx1 - c->rx0 );
		float yoff = TMIN( c->ry1 - exclctrl->ry0, exclctrl->ry1 - c->ry0 );
		if( xoff >= 0 && yoff >= 0 )
			return 0;
		if( xoff >= 0 )
			return -yoff;
		if( yoff >= 0 )
			return -xoff;
		return sqrtf( xoff * xoff + yoff * yoff );
	}
	float Measure( GameUIControl* c )
	{
		float dp = GetMinProj( c, dir ) - mymaxproj;
		if( dp < 0 )
			return FLT_MAX;
		return dp + GetDistance( c );
	}
	void CheckFit( GameUIControl* c )
	{
		if( exclctrl == c || c->focusable == false )
			return;
		
		float m = Measure( c );
		if( m < bestdist )
		{
			bestdist = m;
			bestctrl = c;
		}
	}
	float bestdist;
	GameUIControl* bestctrl;
	GameUIControl* exclctrl;
	Vec2 dir, pos;
	float mymaxproj;
	float rx0, ry0, rx1, ry1;
};


int GUI_DefaultEventCallback( SGS_CTX )
{
	sgs_Method( C );
	GameUIControl* ctrl = sgs_GetVarObj<GameUIControl>()( C, 0 );
	if( !ctrl )
		return sgs_Msg( C, SGS_WARNING, "this != GameUIControl" );
	
	GameUIEvent ev = sgs_GetVar<GameUIEvent>()( C, 1 );
	switch( ev.type )
	{
	case GUI_Event_BtnActivate:
	case GUI_Event_KeyActivate:
	case GUI_Event_CtrlBtnActivate:
		ctrl->InvokeCallbacks( ctrl->m_system->m_str_onclick );
		break;
	case GUI_Event_MouseEnter:
		ctrl->hover = true;
		ctrl->InvokeCallbacks( ctrl->m_system->m_str_onmouseenter );
		break;
	case GUI_Event_MouseLeave:
		ctrl->hover = false;
		ctrl->InvokeCallbacks( ctrl->m_system->m_str_onmouseleave );
		break;
	case GUI_Event_KeyDown:
		if( ev.target == ctrl )
		{
			if( ev.key.key == GUI_Key_Left )  ctrl->m_system->MoveFocus( -1, 0 );
			if( ev.key.key == GUI_Key_Right ) ctrl->m_system->MoveFocus( 1, 0 );
			if( ev.key.key == GUI_Key_Up )    ctrl->m_system->MoveFocus( 0, -1 );
			if( ev.key.key == GUI_Key_Down )  ctrl->m_system->MoveFocus( 0, 1 );
		}
		break;
	case GUI_Event_CtrlBtnDown:
		if( ev.target == ctrl )
		{
			if( ev.ctrl.key == GUI_Key_Left )  ctrl->m_system->MoveFocus( -1, 0 );
			if( ev.ctrl.key == GUI_Key_Right ) ctrl->m_system->MoveFocus( 1, 0 );
			if( ev.ctrl.key == GUI_Key_Up )    ctrl->m_system->MoveFocus( 0, -1 );
			if( ev.ctrl.key == GUI_Key_Down )  ctrl->m_system->MoveFocus( 0, 1 );
		}
		break;
	}
		
	return sgs_PushInt( C, 1 );
}


GameUIControl::GameUIControl() :
	enabled(true), mode(GUI_ScrMode_Abs), x(0), y(0), width(0), height(0),
	xalign(0), yalign(0), xscale(1), yscale(1),
	rx0(0), ry0(0), rx1(0), ry1(0), rwidth(0), rheight(0), z(0),
	hover(false), focusable(false), m_system(NULL)
{
}

GameUIControl::~GameUIControl()
{
	if( parent )
		parent->m_subitems.remove_first( this );
	m_system->_OnRemove( this );
	m_system->m_controls.unset( this );
}

GameUIControl* GameUIControl::Create( SGS_CTX )
{
//	sgsVariable obj( C ); // TODO FIX cppbc
//	GameUIControl* CTL = SGS_CREATECLASS( C, &obj.var, GameUIControl, () );
	GameUIControl* CTL = SGS_CREATECLASS( C, NULL, GameUIControl, () );
	
	CTL->eventCallback = sgsVariable( C );
	CTL->eventCallback.set( GUI_DefaultEventCallback );
	
	CTL->metadata = sgsVariable( C );
	sgs_CreateDict( C, &CTL->metadata.var, 0 );
	
	CTL->shaders = sgsVariable( C );
	sgs_CreateArray( C, &CTL->shaders.var, 0 );
	
//	obj._acquire();
	sgs_ObjAcquire( C, CTL->m_sgsObject );
	sgs_Pop( C, 1 );
	return CTL;
}

void GameUIControl::_FindBestFocus( FocusSearch& fs )
{
	if( enabled == false )
		return;
	fs.CheckFit( this );
	for( size_t i = 0; i < m_subitems.size(); ++i )
	{
		m_subitems[ i ]->_FindBestFocus( fs );
	}
}

bool GameUIControl::_isIn( GameUIControl* prt )
{
	const GameUIControl* ctrl = this;
	while( ctrl )
	{
		if( ctrl == prt )
			return true;
		ctrl = ctrl->parent;
	}
	return false;
}

GameUIControl* GameUIControl::_getFirstFocusable()
{
	if( enabled == false )
		return NULL;
	if( focusable )
		return this;
	for( size_t i = 0; i < m_subitems.size(); ++i )
	{
		GameUIControl* ctrl = m_subitems[ i ]->_getFirstFocusable();
		if( ctrl )
			return ctrl;
	}
	return NULL;
}

bool GameUIControl::_getVisible() const
{
	const GameUIControl* ctrl = this;
	while( ctrl )
	{
		if( ctrl->enabled == false )
			return false;
		ctrl = ctrl->parent;
	}
	return true;
}

int GameUIControl::OnEvent( const GameUIEvent& e )
{
	// call the event callback
	SGS_SCOPE;
	sgsVariable obj = Handle( this ).get_variable();
	sgs_CreateLiteClassFrom( C, NULL, &e );
	sgs_SetGlobal( C, m_system->m_str_GUIEvent.get_variable().var, sgsVariable( C, -1 ).var );
	obj.thiscall( C, eventCallback, 1, 1 );
	return sgs_GetInt( C, -1 );
}

void GameUIControl::BubblingEvent( const GameUIEvent& e, bool breakable )
{
	GameUIControl* cc = this;
	while( cc )
	{
		if( cc->OnEvent( e ) == 0 && breakable )
			break;
		cc = cc->parent;
	}
}

static int sort_ui_subitems( const void* A, const void* B )
{
	GameUIControl* uia = *(GameUIControl**) A;
	GameUIControl* uib = *(GameUIControl**) B;
	if( uia->z != uib->z )
		return uia->z < uib->z ? -1 : 1;
	if( uia->id != uib->id )
		return uia->id < uib->id ? -1 : 1;
	return 0;
}

void GameUIControl::Draw( float dt )
{
	if( enabled == false )
		return;
	
	qsort( m_subitems.data(), m_subitems.size(), sizeof(GameUIControl*), sort_ui_subitems );
	
	{
		// recalculate positions
		rwidth = width;
		rheight = height;
		if( parent )
		{
			if( mode == GUI_ScrMode_Fit ||
				mode == GUI_ScrMode_FitXtd ||
				mode == GUI_ScrMode_Crop )
			{
				float pw = parent->rx1 - parent->rx0, ph = parent->ry1 - parent->ry0;
				float parent_aspect = safe_fdiv( pw, ph );
				float self_aspect = safe_fdiv( width, height );
				
				float tw = pw, th = ph;
				if( ( self_aspect > parent_aspect ) ^ ( mode == GUI_ScrMode_Crop ) )
				{
					// match width
					th = tw / self_aspect;
				}
				else
				{
					// match height
					tw = th * self_aspect;
				}
				tw *= xscale;
				th *= yscale;
				float tx = ( pw - tw ) / 2;
				float ty = ( ph - th ) / 2;
				tx *= xalign + 1;
				ty *= yalign + 1;
				tx += x;
				ty += y;
				rx0 = parent->rx0 + tx;
				ry0 = parent->ry0 + ty;
				rx1 = rx0 + tw;
				ry1 = ry0 + th;
				if( mode == GUI_ScrMode_FitXtd )
				{
					if( self_aspect > parent_aspect )
					{
						rheight = rwidth / parent_aspect;
					}
					else
					{
						rwidth = rheight * parent_aspect;
					}
					rx0 = parent->rx0;
					ry0 = parent->ry0;
					rx1 = parent->rx1;
					ry1 = parent->ry1;
				}
			}
			else if( mode == GUI_ScrMode_Stretch )
			{
				rx0 = parent->rx0;
				ry0 = parent->ry0;
				rx1 = parent->rx1;
				ry1 = parent->ry1;
			}
			else
			{
				rx0 = parent->IX( x );
				ry0 = parent->IY( y );
				rx1 = parent->IX( x + width );
				ry1 = parent->IY( y + height );
			}
		}
		else
		{
			rx0 = x;
			ry0 = y;
			rx1 = x + width;
			ry1 = y + height;
		}
		
		// call all shaders
		SGS_SCOPE;
		sgsVariable obj = Handle( this ).get_variable();
		sgs_PushIterator( C, shaders.var );
		while( sgs_IterAdvance( C, sgs_StackItem( C, -1 ) ) )
		{
			SGS_SCOPE;
			sgsVariable shader( C );
			sgs_IterGetData( C, sgs_StackItem( C, -1 ), NULL, &shader.var );
			
			GR2D_GetBatchRenderer().Reset();
			shader.tthiscall<void>( C, shader, obj, dt );
		}
	}
	
	for( size_t i = 0; i < m_subitems.size(); ++i )
	{
		m_subitems[ i ]->Draw( dt );
	}
}

void GameUIControl::AddCallback( sgsString key, sgsVariable func )
{
	if( !sgs_IsCallableP( &func.var ) )
	{
		sgs_Msg( C, SGS_WARNING, "not callable" );
		return;
	}
	sgsVariable event = Handle( this ).get_variable().getindex( key );
	if( sgs_IsCallableP( &event.var ) )
	{
		sgs_PushVar( C, event );
		event = sgsVariable( C );
		sgs_CreateArray( C, &event.var, 1 );
		Handle( this ).get_variable().setindex( key, event );
	}
	if( sgs_IsArray( event.var ) )
	{
		sgs_PushVar( C, func );
		sgs_ArrayPush( C, event.var, 1 );
	}
	else
	{
		Handle( this ).get_variable().setindex( key, func );
	}
}

void GameUIControl::RemoveCallback( sgsString key, sgsVariable func )
{
	if( !sgs_IsCallableP( &func.var ) )
	{
		sgs_Msg( C, SGS_WARNING, "not callable" );
		return;
	}
	sgsVariable event = Handle( this ).get_variable().getindex( key );
	if( sgs_IsArray( event.var ) )
	{
		sgs_ArrayRemove( C, event.var, key.get_variable().var, SGS_TRUE );
	}
	else
	{
		Handle( this ).get_variable().setindex( key, sgsVariable() );
	}
}

void GameUIControl::InvokeCallbacks( sgsString key )
{
	sgsVariable self = Handle( this ).get_variable();
	sgsVariable event = self.getindex( key );
	if( sgs_IsArray( event.var ) )
	{
		// iterate array of callables
		int sz = sgs_ArraySize( event.var );
		for( int i = 0; i < sz; ++i )
		{
			self.thiscall( C, event.getindex( sgsVariable( C ).set_int( i ) ) );
		}
	}
	else if( sgs_IsCallableP( &event.var ) )
	{
		self.thiscall( C, event );
	}
	else if( event.not_null() )
	{
		sgs_Msg( C, SGS_WARNING, "invalid state for '%s'", key.c_str() );
	}
}

bool GameUIControl::Hit( int x, int y ) const
{
	return enabled && rx0 <= x && x < rx1 && ry0 <= y && y < ry1;
}

float GameUIControl::IX( float x ) const
{
	x = safe_fdiv( x, rwidth ); // to normalized coords
	return TLERP( rx0, rx1, x ); // interpolate from precalc
}

float GameUIControl::IY( float y ) const
{
	y = safe_fdiv( y, rheight ); // to normalized coords
	return TLERP( ry0, ry1, y ); // interpolate from precalc
}

float GameUIControl::IS( float s ) const
{
	return IX( s ) - IX( 0 );
}

float GameUIControl::InvIX( float x ) const
{
	x = TREVLERP<float>( rx0, rx1, x );
	return x * rwidth;
}

float GameUIControl::InvIY( float y ) const
{
	y = TREVLERP<float>( ry0, ry1, y );
	return y * rheight;
}

float GameUIControl::InvIS( float s ) const
{
	return InvIX( s ) - InvIX( 0 );
}

int GameUIControl::_getindex( SGS_ARGS_GETINDEXFUNC )
{
	SGRX_CAST( GameUIControl*, CTL, obj->data );
	SGSBOOL res = sgs_PushIndex( C, CTL->metadata.var, sgs_StackItem( C, 0 ), sgs_ObjectArg( C ) );
	if( res )
		return SGS_SUCCESS; // found
	return _sgs_getindex( C, obj );
}

int GameUIControl::_setindex( SGS_ARGS_SETINDEXFUNC )
{
	SGRX_CAST( GameUIControl*, CTL, obj->data );
	if( _sgs_setindex( C, obj ) != SGS_SUCCESS )
	{
		sgs_SetIndex( C, CTL->metadata.var, sgs_StackItem( C, 0 ),
			sgs_StackItem( C, 1 ), sgs_ObjectArg( C ) );
	}
	return SGS_SUCCESS;
}

GameUIControl::Handle GameUIControl::CreateScreen(
	int mode, float width, float height, float xalign, float yalign, float x, float y )
{
	GameUIControl* CTL = Create( C );
	CTL->mode = mode;
	CTL->width = width;
	CTL->height = height;
	CTL->xalign = xalign;
	CTL->yalign = yalign;
	CTL->x = x;
	CTL->y = y;
	CTL->parent = Handle( this );
	CTL->m_system = m_system;
	m_system->m_controls.set( CTL, NoValue() );
	CTL->id = ++m_system->m_idGen;
	m_subitems.push_back( CTL );
	
	Handle out( CTL );
	sgs_ObjRelease( C, CTL->m_sgsObject );
	return out;
}

GameUIControl::Handle GameUIControl::CreateControl(
	float x, float y, float width, float height )
{
	GameUIControl* CTL = GameUIControl::Create( C );
	CTL->mode = GUI_ScrMode_Abs;
	CTL->x = x;
	CTL->y = y;
	CTL->width = width;
	CTL->height = height;
	CTL->parent = Handle( this );
	CTL->m_system = m_system;
	m_system->m_controls.set( CTL, NoValue() );
	CTL->id = ++m_system->m_idGen;
	m_subitems.push_back( CTL );
	
	GameUIControl::Handle out( CTL );
	sgs_ObjRelease( C, CTL->m_sgsObject );
	return out;
}

void GameUIControl::DReset()
{
	GR2D_GetBatchRenderer().Reset();
}

void GameUIControl::DCol( float a, float b, float c, float d )
{
	BatchRenderer& br = GR2D_GetBatchRenderer();
	int ssz = sgs_StackSize( C );
	if( ssz <= 1 )
		br.Col( a );
	else if( ssz == 2 )
		br.Col( a, b );
	else if( ssz == 3 )
		br.Col( a, b, c );
	else
		br.Col( a, b, c, d );
}

void GameUIControl::DICol( int a, int b, int c, int d )
{
	static const float q = 1.0f / 255.0f;
	BatchRenderer& br = GR2D_GetBatchRenderer();
	int ssz = sgs_StackSize( C );
	if( ssz <= 1 )
		br.Col( a*q );
	else if( ssz == 2 )
		br.Col( a*q, b*q );
	else if( ssz == 3 )
		br.Col( a*q, b*q, c*q );
	else
		br.Col( a*q, b*q, c*q, d*q );
}

void GameUIControl::DSetPrimitiveType( int type )
{
	GR2D_GetBatchRenderer().SetPrimitiveType( (EPrimitiveType) type );
}

void GameUIControl::DPrev( int i )
{
	GR2D_GetBatchRenderer().Prev( i );
}

void GameUIControl::DPos( float x, float y, float z /* = 0 */ )
{
	if( sgs_StackSize( C ) < 3 )
		z = 0;
	GR2D_GetBatchRenderer().Pos( IX( x ), IY( y ), z );
}

void GameUIControl::DTex( StringView name )
{
	GR2D_GetBatchRenderer().SetTexture( GR_GetTexture( name ) );
}

void GameUIControl::DQuad( float x0, float y0, float x1, float y1 )
{
	GR2D_GetBatchRenderer().Quad( IX( x0 ), IY( y0 ), IX( x1 ), IY( y1 ) );
}

void GameUIControl::DQuadWH( float x, float y, float w, float h )
{
	float x1 = x + w, y1 = y + h;
	GR2D_GetBatchRenderer().Quad( IX( x ), IY( y ), IX( x1 ), IY( y1 ) );
}

void GameUIControl::DQuadTexRect( float x0, float y0, float x1, float y1,
	float tx0, float ty0, float tx1, float ty1 )
{
	GR2D_GetBatchRenderer().QuadTexRect( IX( x0 ), IY( y0 ), IX( x1 ), IY( y1 ), tx0, ty0, tx1, ty1 );
}

void GameUIControl::DQuadExt( float x0, float y0, float x1, float y1,
	float tox, float toy, float tsx /* = 1 */, float tsy /* = 1 */ )
{
	int ssz = sgs_StackSize( C );
	GR2D_GetBatchRenderer().QuadExt( IX( x0 ), IY( y0 ), IX( x1 ), IY( y1 ),
		tox, toy, ssz >= 7 ? tsx : 1.0f, ssz >= 8 ? tsy : 1.0f );
}

void GameUIControl::DBox( float x, float y, float w, float h, float z )
{
	int ssz = sgs_StackSize( C );
	GR2D_GetBatchRenderer().Box( IX( x ), IY( y ), IS( w ), IS( h ), ssz >= 5 ? z : 0 );
}

void GameUIControl::DTurnedBox( float x, float y, float dx, float dy, float z /* = 0 */ )
{
	int ssz = sgs_StackSize( C );
	GR2D_GetBatchRenderer().TurnedBox( IX( x ), IY( y ), IS( dx ), IS( dy ), ssz >= 5 ? z : 0 );
}

void GameUIControl::DTexLine( float x0, float y0, float x1, float y1, float rad )
{
	GR2D_GetBatchRenderer().TexLine( V2( IX( x0 ), IY( y0 ) ), V2( IX( x1 ), IY( y1 ) ), IS( rad ) );
}

void GameUIControl::DCircleFill( float x, float y, float r, float z /* = 0 */, int verts /* = -1 */ )
{
	int ssz = sgs_StackSize( C );
	GR2D_GetBatchRenderer().CircleFill( IX( x ), IY( y ), IS( r ), ssz >= 4 ? z : 0, ssz >= 5 ? verts : -1 );
}

void GameUIControl::DButton( float x0, float y0, float x1, float y1, Vec4 bdr, Vec4 texbdr )
{
	GR2D_GetBatchRenderer().Button(
		V4( round( IX( x0 ) ), round( IY( y0 ) ), round( IX( x1 ) ), round( IY( y1 ) ) ),
		bdr, texbdr );
}

void GameUIControl::DAALine( float x0, float y0, float x1, float y1, float w )
{
	if( sgs_StackSize( C ) < 5 )
		w = 1;
	Vec2 pts[2] =
	{
		V2( IX( x0 ), IY( y0 ) ),
		V2( IX( x1 ), IY( y1 ) ),
	};
	GR2D_GetBatchRenderer().AAStroke( pts, 2, IS( w ), false );
}

void GameUIControl::DAAStroke( sgsVariable pointlist, float w, bool closed )
{
	if( sgs_StackSize( C ) < 3 )
		closed = false;
	Array< Vec2 > points;
	ScriptVarIterator it( pointlist );
	while( it.Advance() )
	{
		sgsVariable val = it.GetValue();
		points.push_back( IP( val.get<Vec2>() ) );
	}
	GR2D_GetBatchRenderer().AAStroke( points.data(), points.size(), IS( w ), closed );
}

void GameUIControl::DAAPoly( sgsVariable pointlist )
{
	Array< Vec2 > points;
	ScriptVarIterator it( pointlist );
	while( it.Advance() )
	{
		sgsVariable val = it.GetValue();
		points.push_back( IP( val.get<Vec2>() ) );
	}
	GR2D_GetBatchRenderer().AAPoly( points.data(), points.size() );
}

void GameUIControl::DAARectOutline( float x0, float y0, float x1, float y1, float w )
{
	if( sgs_StackSize( C ) < 5 )
		w = 1;
	Vec2 pts[4] =
	{
		V2( IX( x0 ), IY( y0 ) ),
		V2( IX( x1 ), IY( y0 ) ),
		V2( IX( x1 ), IY( y1 ) ),
		V2( IX( x0 ), IY( y1 ) ),
	};
	GR2D_GetBatchRenderer().AAStroke( pts, 4, IS( w ), true );
}

void GameUIControl::DAACircleFill( float x, float y, float r )
{
	GR2D_GetBatchRenderer().AACircle( IX( x ), IY( y ), IS( r ) );
}

void GameUIControl::DAACircleOutline( float x, float y, float r, float w )
{
	if( sgs_StackSize( C ) < 4 )
		w = 1;
	GR2D_GetBatchRenderer().AACircleOutline( IX( x ), IY( y ), IS( r ), IS( w ) );
}

void GameUIControl::DFont( StringView name, float size )
{
	GR2D_SetFont( name, IS( size ) );
}

void GameUIControl::DText( StringView text, float x, float y, int ha, int va )
{
	if( sgs_StackSize( C ) <= 1 )
		GR2D_DrawTextLine( text );
	else
		GR2D_DrawTextLine( IX( x ), IY( y ), text, ha, va );
}

float GameUIControl::DTextLen( StringView text )
{
	int len = GR2D_GetTextLength( text );
	return InvIS( len );
}

float GameUIControl::DTextRect( StringView text, float x0, float y0, float x1, float y1, int ha, int va )
{
	int ht = GR2D_DrawTextRect( IX( x0 ), IY( y0 ), IX( x1 ), IY( y1 ), text, ha, va );
	return InvIS( ht );
}

float GameUIControl::DTextRectHeight( StringView text, float x0, float y0, float x1, float y1 )
{
	int ht = GR2D_GetTextRectHeight( IX( x0 ), IY( y0 ), IX( x1 ), IY( y1 ), text );
	return InvIS( ht );
}



static GameUISystem* GetSystem( SGS_CTX )
{
	sgs_PushIndex( C, sgs_Registry( C, SGS_REG_ROOT ), sgs_MakeInt( GUI_REG_KEY ), 0 );
	SGRX_CAST( GameUISystem*, sys, sgs_GetPtr( C, -1 ) );
	sgs_Pop( C, 1 );
	return sys;
}

static int TEXTURE( SGS_CTX )
{
	char* bfr;
	sgs_SizeVal size;
	SGSFN( "TEXTURE" );
	if( !sgs_LoadArgs( C, "m", &bfr, &size ) )
		return 0;
	
	GameUISystem* sys = GetSystem( C );
	sys->PrecacheTexture( StringView( bfr, size ) );
	sgs_SetStackSize( C, 1 );
	return 1; // return the string itself
}

static int SetFocusRoot( SGS_CTX )
{
	SGSFN( "SetFocusRoot" );
	GetSystem( C )->m_focusRootCtrl = sgs_GetVarObj<GameUIControl>()( C, 0 );
	return 0;
}

static int GetCVar( SGS_CTX )
{
	SGSFN( "GetCVar" );
	CObj* cv = Game_FindCObj( sgs_GetVar<StringView>()( C, 0 ) );
	if( cv == NULL )
		return sgs_Msg( C, SGS_WARNING, "CVar not found: '%s'",
			sgs_GetVar<sgsString>()( C, 0 ).c_str() );
	
	if( cv->type == COBJ_TYPE_CVAR_BOOL )
		return sgs_PushBool( C, ((CVarBool*)cv)->value );
	if( cv->type == COBJ_TYPE_CVAR_INT )
		return sgs_PushInt( C, ((CVarInt*)cv)->value );
	if( cv->type == COBJ_TYPE_CVAR_FLOAT )
		return sgs_PushReal( C, ((CVarFloat*)cv)->value );
	if( cv->type == COBJ_TYPE_CVAR_ENUM )
	{
		String s;
		((CVarEnum*)cv)->ToString( s );
		sgs_PushVar( C, s );
		return 1;
	}
	return 0;
}

static int SetCVar( SGS_CTX )
{
	SGSFN( "SetCVar" );
	CObj* cv = Game_FindCObj( sgs_GetVar<StringView>()( C, 0 ) );
	if( cv == NULL )
		return sgs_Msg( C, SGS_WARNING, "CVar not found: '%s'",
			sgs_GetVar<sgsString>()( C, 0 ).c_str() );
	
	if( cv->type == COBJ_TYPE_CVAR_BOOL )
		((CVarBool*)cv)->value = sgs_GetBool( C, 1 );
	if( cv->type == COBJ_TYPE_CVAR_INT )
		((CVarInt*)cv)->value = sgs_GetInt( C, 1 );
	if( cv->type == COBJ_TYPE_CVAR_FLOAT )
		((CVarFloat*)cv)->value = sgs_GetReal( C, 1 );
	if( cv->type == COBJ_TYPE_CVAR_ENUM )
		((CVarEnum*)cv)->FromString( sgs_GetVar<StringView>()( C, 1 ) );
	return 0;
}

static int DoCommand( SGS_CTX )
{
	SGSFN( "DoCommand" );
	sgs_PushBool( C, Game_DoCommand( sgs_GetVar<StringView>()( C, 0 ) ) );
	return 1;
}

sgs_RegIntConst sgs_iconsts[] =
{
	{ "HALIGN_LEFT", HALIGN_LEFT },
	{ "HALIGN_CENTER", HALIGN_CENTER },
	{ "HALIGN_RIGHT", HALIGN_RIGHT },
	{ "VALIGN_TOP", VALIGN_TOP },
	{ "VALIGN_CENTER", VALIGN_CENTER },
	{ "VALIGN_BOTTOM", VALIGN_BOTTOM },
	
	{ "GUI_ScrMode_Abs", GUI_ScrMode_Abs },
	{ "GUI_ScrMode_Fit", GUI_ScrMode_Fit },
	{ "GUI_ScrMode_Crop", GUI_ScrMode_Crop },
	{ "GUI_ScrMode_FitXtd", GUI_ScrMode_FitXtd },
	{ "GUI_ScrMode_Stretch", GUI_ScrMode_Stretch },
	
	{ "GUI_Event_MouseMove", GUI_Event_MouseMove },
	{ "GUI_Event_MouseEnter", GUI_Event_MouseEnter },
	{ "GUI_Event_MouseLeave", GUI_Event_MouseLeave },
	{ "GUI_Event_BtnDown", GUI_Event_BtnDown },
	{ "GUI_Event_BtnUp", GUI_Event_BtnUp },
	{ "GUI_Event_BtnActivate", GUI_Event_BtnActivate },
	{ "GUI_Event_MouseWheel", GUI_Event_MouseWheel },
	{ "GUI_Event_PropEdit", GUI_Event_PropEdit },
	{ "GUI_Event_PropChange", GUI_Event_PropChange },
	{ "GUI_Event_SetFocus", GUI_Event_SetFocus },
	{ "GUI_Event_LoseFocus", GUI_Event_LoseFocus },
	{ "GUI_Event_KeyDown", GUI_Event_KeyDown },
	{ "GUI_Event_KeyUp", GUI_Event_KeyUp },
	{ "GUI_Event_KeyActivate", GUI_Event_KeyActivate },
	{ "GUI_Event_CtrlBtnUp", GUI_Event_CtrlBtnUp },
	{ "GUI_Event_CtrlBtnDown", GUI_Event_CtrlBtnDown },
	{ "GUI_Event_CtrlBtnActivate", GUI_Event_CtrlBtnActivate },
	{ "GUI_Event_TextInput", GUI_Event_TextInput },
	{ "GUI_Event_User", GUI_Event_User },
	
	{ "GUI_Key_Unknown", GUI_Key_Unknown },
	{ "GUI_Key_Left", GUI_Key_Left },
	{ "GUI_Key_Right", GUI_Key_Right },
	{ "GUI_Key_Up", GUI_Key_Up },
	{ "GUI_Key_Down", GUI_Key_Down },
	{ "GUI_Key_DelLeft", GUI_Key_DelLeft },
	{ "GUI_Key_DelRight", GUI_Key_DelRight },
	{ "GUI_Key_Tab", GUI_Key_Tab },
	{ "GUI_Key_Cut", GUI_Key_Cut },
	{ "GUI_Key_Copy", GUI_Key_Copy },
	{ "GUI_Key_Paste", GUI_Key_Paste },
	{ "GUI_Key_Undo", GUI_Key_Undo },
	{ "GUI_Key_Redo", GUI_Key_Redo },
	{ "GUI_Key_SelectAll", GUI_Key_SelectAll },
	{ "GUI_Key_PageUp", GUI_Key_PageUp },
	{ "GUI_Key_PageDown", GUI_Key_PageDown },
	{ "GUI_Key_Enter", GUI_Key_Enter },
	{ "GUI_Key_Activate", GUI_Key_Activate },
	{ "GUI_Key_Escape", GUI_Key_Escape },
	
	{ "GUI_MB_Left", GUI_MB_Left },
	{ "GUI_MB_Right", GUI_MB_Right },
	
	{ "GUI_KeyMod_Filter", GUI_KeyMod_Filter },
	{ "GUI_KeyMod_Shift", GUI_KeyMod_Shift },
	
	{ "PT_Points", PT_Points },
	{ "PT_Lines", PT_Lines },
	{ "PT_LineStrip", PT_LineStrip },
	{ "PT_Triangles", PT_Triangles },
	{ "PT_TriangleStrip", PT_TriangleStrip },
};

sgs_RegFuncConst sgs_funcs[] =
{
	{ "TEXTURE", TEXTURE },
	{ "SetFocusRoot", SetFocusRoot },
	{ "GetCVar", GetCVar },
	{ "SetCVar", SetCVar },
	{ "DoCommand", DoCommand },
};

GameUISystem::GameUISystem( ScriptContext* scrctx ) :
	m_idGen(0), m_rootCtrl(NULL), m_focusRootCtrl(NULL),
	m_scriptCtx(scrctx), m_hoverCtrl(NULL), m_kbdFocusCtrl(NULL),
	m_mouseX(0), m_mouseY(0)
{
	SGS_CTX = m_scriptCtx->C;
	m_rootCtrl = GameUIControl::Create( C );
	m_rootCtrl->m_system = this;
	m_controls.set( m_rootCtrl, NoValue() );
	m_rootCtrl->id = ++m_idGen;
	m_focusRootCtrl = m_rootCtrl;
	m_clickCtrl[0] = NULL;
	m_clickCtrl[1] = NULL;
	
	sgs_SetIndex( C, sgs_Registry( C, SGS_REG_ROOT ),
		sgs_MakeInt( GUI_REG_KEY ), sgs_MakePtr( this ), 0 );
	sgs_RegIntConsts( C, sgs_iconsts, SGRX_ARRAY_SIZE(sgs_iconsts) );
	sgs_RegFuncConsts( C, sgs_funcs, SGRX_ARRAY_SIZE(sgs_funcs) );
	m_scriptCtx->SetGlobal( "ROOT", m_rootCtrl->GetHandle() );
	
	m_str_onclick = sgsString( C, "onclick" );
	m_str_onmouseenter = sgsString( C, "onmouseenter" );
	m_str_onmouseleave = sgsString( C, "onmouseleave" );
	m_str_GUIEvent = sgsString( C, "GUIEvent" );
}

static sgs_ObjInterface deleted_gui_ctrl = { "deleted_gui_ctrl" };
GameUISystem::~GameUISystem()
{
	m_str_onclick = sgsString();
	m_str_onmouseenter = sgsString();
	m_str_onmouseleave = sgsString();
	
	sgs_ObjRelease( m_scriptCtx->C, m_rootCtrl->m_sgsObject );
	m_rootCtrl = NULL;
	
	while( m_controls.size() )
	{
		GameUIControl* ctrl = m_controls.item( 0 ).key;
		sgs_VarObj* obj = ctrl->m_sgsObject;
		sgs_ObjCallDtor( m_scriptCtx->C, obj );
		obj->data = NULL;
		obj->iface = &deleted_gui_ctrl;
	}
}

void GameUISystem::Load( const StringView& sv )
{
	m_scriptCtx->ExecFile( sv );
}

void GameUISystem::EngineEvent( const Event& eev )
{
	if( eev.type == SDL_MOUSEMOTION )
	{
		m_mouseX = eev.motion.x;
		m_mouseY = eev.motion.y;
		if( m_hoverCtrl )
		{
			GameUIEvent ev = { GUI_Event_MouseMove, m_hoverCtrl->GetHandle() };
			ev.mouse.x = m_mouseX;
			ev.mouse.y = m_mouseY;
			m_hoverCtrl->OnEvent( ev );
		}
		_HandleMouseMove( true );
	}
	else if( eev.type == SDL_MOUSEWHEEL )
	{
		if( m_hoverCtrl )
		{
			GameUIEvent ev = { GUI_Event_MouseWheel, m_hoverCtrl->GetHandle() };
			ev.mouse.x = eev.wheel.x;
			ev.mouse.y = eev.wheel.y;
			m_hoverCtrl->BubblingEvent( ev );
		}
	}
	else if( eev.type == SDL_MOUSEBUTTONUP || eev.type == SDL_MOUSEBUTTONDOWN )
	{
		int btn = eev.button.button;
		if( btn == SGRX_MB_LEFT ) btn = GUI_MB_Left;
		else if( btn == SGRX_MB_RIGHT ) btn = GUI_MB_Right;
		else
			btn = -1;
		if( btn >= 0 )
		{
			bool down = eev.type == SDL_MOUSEBUTTONDOWN;
			GameUIEvent ev = { down ? GUI_Event_BtnDown : GUI_Event_BtnUp };
			ev.mouse.x = m_mouseX;
			ev.mouse.y = m_mouseY;
			ev.mouse.button = btn;
			
			if( !down )
			{
				ev.target = m_clickCtrl[ btn ]->GetHandle();
				if( m_clickCtrl[ btn ] )
				{
					m_clickCtrl[ btn ]->BubblingEvent( ev, true );
					if( m_clickCtrl[ btn ]->Hit( m_mouseX, m_mouseY ) )
					{
						ev.type = GUI_Event_BtnActivate;
						m_clickCtrl[ btn ]->BubblingEvent( ev, true );
					}
					m_clickCtrl[ btn ] = NULL;
				}
				_HandleMouseMove( true );
			}
			else if( m_hoverCtrl )
			{
				m_clickCtrl[ btn ] = m_hoverCtrl;
				m_kbdFocusCtrl = m_hoverCtrl;
				ev.target = m_hoverCtrl->GetHandle();
				m_hoverCtrl->BubblingEvent( ev, true );
			}
		}
	}
	else if( eev.type == SDL_KEYDOWN || eev.type == SDL_KEYUP )
	{
		if( !m_kbdFocusCtrl )
			return;
		
		bool down = eev.type == SDL_KEYDOWN;
		int engkey = eev.key.keysym.sym;
		int engmod = eev.key.keysym.mod;
		
		GameUIEvent ev =
		{
			down ? GUI_Event_KeyDown : GUI_Event_KeyUp,
			m_kbdFocusCtrl->GetHandle()
		};
		ev.key.key = GUI_Key_Unknown;
		ev.key.engkey = engkey;
		ev.key.engmod = engmod;
		ev.key.repeat = !!eev.key.repeat;
		
		if(0);
		else if( engkey == SDLK_RETURN ) ev.key.key = GUI_Key_Enter;
		else if( engkey == SDLK_KP_ENTER ) ev.key.key = GUI_Key_Enter;
		else if( engkey == SDLK_SPACE ) ev.key.key = GUI_Key_Activate;
		else if( engkey == SDLK_ESCAPE ) ev.key.key = GUI_Key_Escape;
		else if( engkey == SDLK_BACKSPACE ) ev.key.key = GUI_Key_DelLeft;
		else if( engkey == SDLK_DELETE ) ev.key.key = GUI_Key_DelRight;
		else if( engkey == SDLK_LEFT ) ev.key.key = GUI_Key_Left;
		else if( engkey == SDLK_RIGHT ) ev.key.key = GUI_Key_Right;
		else if( engkey == SDLK_UP ) ev.key.key = GUI_Key_Up;
		else if( engkey == SDLK_DOWN ) ev.key.key = GUI_Key_Down;
		else if( engkey == SDLK_a && engmod & KMOD_CTRL ){ ev.key.key = GUI_Key_SelectAll; }
		else if( engkey == SDLK_x && engmod & KMOD_CTRL ){ ev.key.key = GUI_Key_Cut; }
		else if( engkey == SDLK_c && engmod & KMOD_CTRL ){ ev.key.key = GUI_Key_Copy; }
		else if( engkey == SDLK_v && engmod & KMOD_CTRL ){ ev.key.key = GUI_Key_Paste; }
		
		if( engmod & KMOD_SHIFT )
			ev.key.key |= GUI_KeyMod_Shift;
		
		m_kbdFocusCtrl->BubblingEvent( ev, true );
		
		if( down && ( ev.key.key == GUI_Key_Enter || ev.key.key == GUI_Key_Activate )
			&& ( engmod & (KMOD_CTRL|KMOD_ALT) ) == 0 )
		{
			ev.type = GUI_Event_KeyActivate;
			m_kbdFocusCtrl->BubblingEvent( ev, true );
		}
	}
	else if( eev.type == SDL_TEXTINPUT )
	{
		if( !m_kbdFocusCtrl )
			return;
		
		GameUIEvent ev = { GUI_Event_TextInput, m_kbdFocusCtrl->GetHandle() };
		sgrx_sncopy( ev.text.text, 8, eev.text.text );
		ev.text.text[7] = 0;
		
		m_kbdFocusCtrl->OnEvent( ev );
	}
	else if( eev.type == SDL_CONTROLLERBUTTONDOWN || eev.type == SDL_CONTROLLERBUTTONUP )
	{
		bool down = eev.type == SDL_CONTROLLERBUTTONDOWN;
		GameUIEvent ev =
		{
			down ? GUI_Event_CtrlBtnDown : GUI_Event_CtrlBtnUp,
			m_kbdFocusCtrl->GetHandle()
		};
		ev.ctrl.key = GUI_Key_Unknown;
		ev.ctrl.engbutton = eev.cbutton.button;
		
		if(0);
		else if( eev.cbutton.button == SDL_CONTROLLER_BUTTON_A ) ev.ctrl.key = GUI_Key_Enter;
		else if( eev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT ) ev.ctrl.key = GUI_Key_Left;
		else if( eev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ) ev.ctrl.key = GUI_Key_Right;
		else if( eev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP ) ev.ctrl.key = GUI_Key_Up;
		else if( eev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN ) ev.ctrl.key = GUI_Key_Down;
		
		m_kbdFocusCtrl->BubblingEvent( ev, true );
		
		if( down && ( ev.ctrl.key == GUI_Key_Enter || ev.ctrl.key == GUI_Key_Activate ) )
		{
			ev.type = GUI_Event_CtrlBtnActivate;
			m_kbdFocusCtrl->BubblingEvent( ev, true );
		}
	}
}

void GameUISystem::_HandleMouseMove( bool optional )
{
	if( optional )
	{
		for( int i = 0; i < 2; ++i )
			if( m_clickCtrl[ i ] )
				return;
	}
	
	GameUIControl* prevhover = m_hoverCtrl;
	m_hoverCtrl = _GetItemAtPosition( m_mouseX, m_mouseY );
	
	if( m_hoverCtrl != prevhover )
	{
		if( prevhover || m_hoverCtrl )
		{
			// try to find common parent
			GameUIControl* phi = prevhover, *pphi = NULL;
			size_t i;
			while( phi )
			{
				for( i = 0; i < m_hoverTrail.size(); ++i )
					if( m_hoverTrail[ i ] == phi )
						break;
				if( i < m_hoverTrail.size() )
					break;
				pphi = phi;
				phi = phi->parent;
			}
			
		//	printf("ht0=%p htsz=%d pphi=%p phi=%p prev=%p curr=%p\n", m_hoverTrail.size()>=2 ? m_hoverTrail[1] : NULL, (int) m_hoverTrail.size(), pphi, phi, prevhover, m_hover );
			if( !phi )
			{
				// no common parent, run bubbling event on previous hover, bubbling event on current hover, update all styles
				if( pphi )
				{
					GameUIEvent e = { GUI_Event_MouseLeave, pphi->GetHandle() };
					e.mouse.x = m_mouseX;
					e.mouse.y = m_mouseY;
					pphi->BubblingEvent( e );
				}
				if( m_hoverCtrl )
				{
					GameUIEvent e = { GUI_Event_MouseEnter, m_hoverCtrl->GetHandle() };
					e.mouse.x = m_mouseX;
					e.mouse.y = m_mouseY;
					m_hoverCtrl->BubblingEvent( e );
				}
			}
			else
			{
				GameUIEvent e;
				// found common parent, run events through that (parent gets none), update styles up from parent (not parent itself)
				GameUIControl *cc;
				
				e.type = GUI_Event_MouseLeave;
				e.target = prevhover->GetHandle();
				e.mouse.x = m_mouseX;
				e.mouse.y = m_mouseY;
				
				cc = prevhover;
				while( cc && cc != phi )
				{
					cc->OnEvent( e );
					cc = cc->parent;
				}
				
				e.type = GUI_Event_MouseEnter;
				e.target = m_hoverCtrl->GetHandle();
				e.mouse.x = m_mouseX;
				e.mouse.y = m_mouseY;
				
				cc = m_hoverCtrl;
				while( cc && cc != phi )
				{
					cc->OnEvent( e );
					cc = cc->parent;
				}
			}
		}
	}
}

GameUIControl* GameUISystem::_GetItemAtPosition( int x, int y )
{
	m_hoverTrail.clear();
	m_hoverTrail.push_back( m_rootCtrl );
	GameUIControl* item = m_rootCtrl, *atpos = NULL;
	while( item && item != atpos )
	{
		atpos = item;
		for( size_t i = item->m_subitems.size(); i > 0; )
		{
			--i;
			if( item->m_subitems[ i ]->Hit( x, y ) )
			{
				item = item->m_subitems[ i ];
				m_hoverTrail.push_back( item );
				break;
			}
		}
	}
	return atpos;
}

void GameUISystem::Draw( float dt )
{
	// check keyboard focus control
	if( m_kbdFocusCtrl &&
		( m_kbdFocusCtrl->_getVisible() == false ||
		m_kbdFocusCtrl->_isIn( m_focusRootCtrl ) == false ) )
	{
		MoveFocus( 0, 0 );
	}
	if( m_kbdFocusCtrl == NULL )
	{
		m_kbdFocusCtrl = m_focusRootCtrl->_getFirstFocusable();
	}
	
	GR2D_SetViewMatrix( Mat4::CreateUI( 0, 0, GR_GetWidth(), GR_GetHeight() ) );
	
	m_rootCtrl->x = 0;
	m_rootCtrl->y = 0;
	m_rootCtrl->width = GR_GetWidth();
	m_rootCtrl->height = GR_GetHeight();
	m_rootCtrl->Draw( dt );
	
	_HandleMouseMove( true );
	
	GR2D_GetBatchRenderer().Flush();
}

void GameUISystem::CallFunc( StringView name )
{
	m_scriptCtx->GlobalCall( name );
}

void GameUISystem::_OnRemove( GameUIControl* ctrl )
{
	m_hoverTrail.remove_all( ctrl );
	if( m_focusRootCtrl == ctrl )
		m_focusRootCtrl = m_rootCtrl;
	if( m_hoverCtrl == ctrl )
		m_hoverCtrl = NULL;
	if( m_kbdFocusCtrl == ctrl )
		m_kbdFocusCtrl = NULL;
	if( m_clickCtrl[0] == ctrl )
		m_clickCtrl[0] = NULL;
	if( m_clickCtrl[1] == ctrl )
		m_clickCtrl[1] = NULL;
}

void GameUISystem::MoveFocus( float x, float y )
{
	FocusSearch fs( m_kbdFocusCtrl, V2( x, y ) );
	( m_focusRootCtrl ? m_focusRootCtrl : m_rootCtrl )->_FindBestFocus( fs );
	m_kbdFocusCtrl = fs.bestctrl;
}

void GameUISystem::PrecacheTexture( const StringView& texname )
{
	m_precachedTextures.push_back( GR_GetTexture( texname ) );
}


