

#include "entities.hpp"


extern Vec2 CURSOR_POS;


Trigger::Trigger( GameLevel* lev, bool laststate ) :
	Entity( lev ), m_once( true ), m_done( false ), m_lastState( laststate ), m_currState( false )
{
}

void Trigger::Invoke( bool newstate )
{
	sgsVariable& curfn = newstate ? m_func : m_funcOut;
	if( curfn.not_null() )
	{
		SGS_CSCOPE( m_level->m_scriptCtx.C );
		m_level->m_scriptCtx.Push( newstate );
		m_level->m_scriptCtx.Call( curfn, 1 );
	}
}

void Trigger::Update( bool newstate )
{
	if( m_currState != newstate && !m_done )
	{
		m_currState = newstate;
		Invoke( newstate );
		if( m_once && newstate == m_lastState ) // once on, once off
			m_done = true;
	}
}

void Trigger::sgsSetupTrigger( bool once, sgsVariable fn, sgsVariable fnout )
{
	m_done = false;
	m_once = once;
	m_func = fn;
	if( sgs_StackSize( C ) < 3 )
		m_funcOut = fn;
	else if( sgs_ItemType( C, 2 ) == SGS_VT_BOOL )
		m_funcOut = sgs_GetBool( C, 2 ) ? fn : sgsVariable();
	else
		m_funcOut = fnout;
}


BoxTrigger::BoxTrigger( GameLevel* lev, StringView name, const Vec3& pos, const Quat& rot, const Vec3& scl ) :
	Trigger( lev ), m_matrix( Mat4::CreateSRT( scl, rot, pos ) )
{
	m_matrix.InvertTo( m_matrix );
	
	m_name = name;
	m_level->MapEntityByName( this );
}

void BoxTrigger::FixedTick( float deltaTime )
{
	Update( m_level->GetSystem<InfoEmissionSystem>()->QueryBB( m_matrix, IEST_Player ) );
}


ProximityTrigger::ProximityTrigger( GameLevel* lev, StringView name, const Vec3& pos, float rad ) :
	Trigger( lev ), m_position( pos ), m_radius( rad )
{
	m_name = name;
	m_level->MapEntityByName( this );
}

void ProximityTrigger::FixedTick( float deltaTime )
{
	Update( m_level->GetSystem<InfoEmissionSystem>()->QuerySphereAny( m_position, m_radius, IEST_Player ) );
}


void SlidingDoor::_UpdatePhysics()
{
	if( !bodyHandle )
		return;
	Vec3 localpos = TLERP( pos_closed, pos_open, open_factor );
	Quat localrot = TLERP( rot_closed, rot_open, open_factor );
	Mat4 basemtx = Mat4::CreateSRT( scale, rotation, position );
	bodyHandle->SetPosition( basemtx.TransformPos( localpos ) );
	bodyHandle->SetRotation( localrot * rotation );
}

void SlidingDoor::_UpdateTransforms( float bf )
{
	Vec3 localpos = m_ivPos.Get( bf );
	Quat localrot = m_ivRot.Get( bf );
	Mat4 basemtx = Mat4::CreateSRT( scale, rotation, position );
	meshInst->matrix = Mat4::CreateSRT( V3(1), localrot, localpos ) * basemtx;
	m_level->LightMesh( meshInst );
}

SlidingDoor::SlidingDoor
(
	GameLevel* lev,
	const StringView& name,
	const StringView& mesh,
	const Vec3& pos,
	const Quat& rot,
	const Vec3& scl,
	const Vec3& oopen,
	const Quat& ropen,
	const Vec3& oclos,
	const Quat& rclos,
	float otime,
	bool istate,
	bool isswitch
):
	Trigger( lev, true ),
	open_factor( istate ), open_target( istate ), open_time( TMAX( otime, SMALL_FLOAT ) ),
	pos_open( oopen ), pos_closed( oclos ), rot_open( ropen ), rot_closed( rclos ),
	target_state( istate ), m_isSwitch( isswitch ),
	position( pos ), rotation( rot ), scale( scl ),
	m_bbMin( V3(-1) ), m_bbMax( V3(1) ),
	m_ivPos( V3(0) ), m_ivRot( Quat::Identity )
{
	m_name = name;
	meshInst = m_level->GetScene()->CreateMeshInstance();
	
	char bfr[ 256 ] = {0};
	sgrx_snprintf( bfr, sizeof(bfr), "meshes/%.*s.ssm", TMIN( 240, (int) mesh.size() ), mesh.data() );
	meshInst->SetMesh( bfr );
	
	if( meshInst->GetMesh() )
	{
		m_bbMin = meshInst->GetMesh()->m_boundsMin;
		m_bbMax = meshInst->GetMesh()->m_boundsMax;
	}
	
	if( m_isSwitch == false )
	{
		SGRX_PhyRigidBodyInfo rbinfo;
		rbinfo.shape = m_level->GetPhyWorld()->CreateAABBShape( m_bbMin, m_bbMax );
		rbinfo.shape->SetScale( scale );
		rbinfo.kinematic = true;
		rbinfo.position = position;
		rbinfo.rotation = rotation;
		rbinfo.mass = 0;
		rbinfo.inertia = V3(0);
		bodyHandle = m_level->GetPhyWorld()->CreateRigidBody( rbinfo );
		soundEvent = g_SoundSys->CreateEventInstance( "/gate_open" );
		if( soundEvent )
		{
			SGRX_Sound3DAttribs s3dattr = { position, V3(0), V3(0), V3(0) };
			soundEvent->Set3DAttribs( s3dattr );
		}
	}
	
	_UpdatePhysics();
	m_ivPos.prev = m_ivPos.curr = TLERP( pos_closed, pos_open, open_factor );
	m_ivRot.prev = m_ivRot.curr = TLERP( rot_closed, rot_open, open_factor );
	_UpdateTransforms( 1 );
	
	m_level->MapEntityByName( this );
}

void SlidingDoor::FixedTick( float deltaTime )
{
	if( soundEvent )
		soundEvent->SetParameter( "position", (target_state?0:1) + (target_state?1:-1) * open_factor / 1 );
	if( open_factor != open_target )
	{
		float df = open_target - open_factor;
		df /= open_time;
		float df_len = fabsf( df );
		float df_dir = sign( df );
		if( df_len > deltaTime )
			df_len = deltaTime;
		open_factor += df_len * df_dir;
		
		_UpdatePhysics();
	}
	else if( open_target != (target_state ? 1.0f : 0.0f) )
	{
		open_target = target_state;
		if( soundEvent )
			soundEvent->Start();
	}
	m_ivPos.Advance( TLERP( pos_closed, pos_open, open_factor ) );
	m_ivRot.Advance( TLERP( rot_closed, rot_open, open_factor ) );
	
	if( m_isSwitch )
	{
		if( !m_done )
		{
			InfoEmissionSystem::Data D = { position, 0.5f, IEST_InteractiveItem };
			m_level->GetSystem<InfoEmissionSystem>()->UpdateEmitter( this, D );
		}
		else
			m_level->GetSystem<InfoEmissionSystem>()->RemoveEmitter( this );
	}
}

void SlidingDoor::Tick( float deltaTime, float blendFactor )
{
	_UpdateTransforms( blendFactor );
}

void SlidingDoor::OnEvent( const StringView& type )
{
	if( type == "trigger_enter" || type == "trigger_leave" || type == "trigger_switch" )
	{
		bool newstate = type == "trigger_enter" || ( type == "trigger_switch" && !target_state );
		if( m_isSwitch )
		{
			if( open_factor != open_target )
			{
				return;
			}
			SGS_CSCOPE( m_level->m_scriptCtx.C );
			m_level->m_scriptCtx.Push( newstate );
			if( m_level->m_scriptCtx.Call( m_switchPred, 1, 1 ) )
			{
				bool val = sgs_GetVar<bool>()( m_level->m_scriptCtx.C, -1 );
				if( !val )
					return;
			}
		}
		
		target_state = newstate;
		
		if( m_isSwitch )
		{
			Update( target_state );
		}
	}
}


PickupItem::PickupItem( GameLevel* lev, const StringView& name, const StringView& type,
	int count, const StringView& mesh, const Vec3& pos, const Quat& rot, const Vec3& scl ) :
	Entity( lev ), m_type( type ), m_count( count ), m_pos( pos )
{
	m_name = name;
	m_meshInst = m_level->GetScene()->CreateMeshInstance();
	
	char bfr[ 256 ] = {0};
	sgrx_snprintf( bfr, sizeof(bfr), "meshes/%.*s.ssm", TMIN( 240, (int) mesh.size() ), mesh.data() );
	m_meshInst->SetMesh( bfr );
	m_meshInst->matrix = Mat4::CreateSRT( scl, rot, pos );
	m_level->LightMesh( m_meshInst );
	
	InfoEmissionSystem::Data D = { pos, 0.5f, IEST_InteractiveItem };
	m_level->GetSystem<InfoEmissionSystem>()->UpdateEmitter( this, D );
}

void PickupItem::OnEvent( const StringView& type )
{
	if( ( type == "trigger_switch" || type == "action_end" ) )
	{
		SGS_SCOPE;
		
		sgsVariable scrobj = GetScriptedObject();
		scrobj.push( C );
		if( scrobj.getprop( "level" ).thiscall( C, "onPickupItem", 1, 1 ) )
		{
			bool keep = sgs_GetVar<bool>()( C, -1 );
			if( keep == false )
			{
				m_level->GetSystem<InfoEmissionSystem>()->RemoveEmitter( this );
				m_meshInst->enabled = false;
			}
		}
	}
}

bool PickupItem::GetInteractionInfo( Vec3 pos, InteractInfo* out )
{
	out->type = IT_Pickup;
	out->placePos = m_pos;
	out->placeDir = V3(0);
	out->timeEstimate = 0.5f;
	out->timeActual = 0.5f;
	return true;
}


Actionable::Actionable( GameLevel* lev, const StringView& name, const StringView& mesh,
	const Vec3& pos, const Quat& rot, const Vec3& scl, const Vec3& placeoff, const Vec3& placedir ) :
	Entity( lev )
{
	Mat4 mtx = Mat4::CreateSRT( scl, rot, pos );
	
	m_info.type = IT_Investigate;
	m_info.placePos = mtx.TransformPos( placeoff );
	m_info.placeDir = mtx.TransformNormal( placedir ).Normalized();
	m_info.timeEstimate = 0.5f;
	m_info.timeActual = 0.5f;
	
	m_name = name;
	m_viewName = name;
	m_meshInst = m_level->GetScene()->CreateMeshInstance();
	
	char bfr[ 256 ] = {0};
	sgrx_snprintf( bfr, sizeof(bfr), "meshes/%.*s.ssm", TMIN( 240, (int) mesh.size() ), mesh.data() );
	m_meshInst->SetMesh( bfr );
	m_meshInst->matrix = mtx;
	m_level->LightMesh( m_meshInst );
	
	SetEnabled( true );
	
	m_level->MapEntityByName( this );
}

void Actionable::OnEvent( const StringView& type )
{
	if( m_enabled )
	{
		if( type == "action_start" )
		{
			// start animation?
		}
		else if( type == "action_end" )
		{
			// end animation?
			SGS_SCOPE;
			GetScriptedObject().push( C );
			if( m_onSuccess.call( C, 1, 1 ) )
			{
				bool keep = sgs_GetVar<bool>()( C, -1 );
				if( keep == false )
				{
					m_level->GetSystem<InfoEmissionSystem>()->RemoveEmitter( this );
				}
			}
		}
	}
}

bool Actionable::GetInteractionInfo( Vec3 pos, InteractInfo* out )
{
	if( m_enabled == false )
		return false;
	*out = m_info;
	return true;
}

void Actionable::SetEnabled( bool v )
{
	m_enabled = v;
	if( v )
	{
		InfoEmissionSystem::Data D = { m_info.placePos, 0.5f, IEST_InteractiveItem };
		m_level->GetSystem<InfoEmissionSystem>()->UpdateEmitter( this, D );
	}
	else
	{
		m_level->GetSystem<InfoEmissionSystem>()->RemoveEmitter( this );
	}
}


ParticleFX::ParticleFX( GameLevel* lev, const StringView& name, const StringView& psys, const StringView& sndev, const Vec3& pos, const Quat& rot, const Vec3& scl, bool start ) :
	Entity( lev ), m_soundEventName( sndev ), m_position( pos )
{
	m_name = name;
	m_soundEventOneShot = g_SoundSys->EventIsOneShot( sndev );
	
	char bfr[ 256 ] = {0};
	sgrx_snprintf( bfr, sizeof(bfr), "psys/%.*s.psy", TMIN( 240, (int) psys.size() ), psys.data() );
	m_psys.Load( bfr );
	m_psys.AddToScene( m_level->GetScene() );
	m_psys.SetTransform( Mat4::CreateSRT( scl, rot, pos ) );
	m_psys.OnRenderUpdate();
	
	if( start )
		OnEvent( "trigger_enter" );
	
	m_level->MapEntityByName( this );
}

void ParticleFX::Tick( float deltaTime, float blendFactor )
{
	bool needstrig = m_psys.Tick( deltaTime );
	if( needstrig && m_soundEventOneShot )
	{
		SoundEventInstanceHandle sndevinst = g_SoundSys->CreateEventInstance( m_soundEventName );
		if( sndevinst )
		{
			SGRX_Sound3DAttribs s3dattr = { m_position, V3(0), V3(0), V3(0) };
			sndevinst->Set3DAttribs( s3dattr );
			sndevinst->Start();
		}
	}
	m_psys.PreRender();
}

void ParticleFX::OnEvent( const StringView& _type )
{
	StringView type = _type;
	if( type == "trigger_switch" )
		type = m_psys.m_isPlaying ? "trigger_leave" : "trigger_enter";
	if( type == "trigger_enter" )
	{
		m_psys.Play();
		SoundEventInstanceHandle sndevinst = g_SoundSys->CreateEventInstance( m_soundEventName );
		if( sndevinst )
		{
			SGRX_Sound3DAttribs s3dattr = { m_position, V3(0), V3(0), V3(0) };
			sndevinst->Set3DAttribs( s3dattr );
			sndevinst->Start();
			if( !sndevinst->isOneShot )
				m_soundEventInst = sndevinst;
		}
	}
	else if( type == "trigger_leave" )
	{
		m_psys.Stop();
		m_soundEventInst = NULL;
	}
	else if( type == "trigger_once" )
	{
		m_psys.Trigger();
		if( m_soundEventOneShot )
		{
			SoundEventInstanceHandle sndevinst = g_SoundSys->CreateEventInstance( m_soundEventName );
			if( sndevinst )
			{
				SGRX_Sound3DAttribs s3dattr = { m_position, V3(0), V3(0), V3(0) };
				sndevinst->Set3DAttribs( s3dattr );
				sndevinst->Start();
			}
		}
	}
}




ScriptedItem::ScriptedItem( GameLevel* lev, const StringView& name, sgsVariable args ) : Entity( lev ), m_scrItem(NULL)
{
	char bfr[ 256 ];
	sgrx_snprintf( bfr, 256, "SCRITEM_CREATE_%s", StackString<200>(name).str );
	sgsVariable func = m_level->m_scriptCtx.GetGlobal( bfr );
	if( func.not_null() )
	{
		sgs_Variable pvar = sgs_MakePtr( this );
		args.setprop( "__entity", sgsVariable( m_level->m_scriptCtx.C, &pvar ) );
		
		m_scrItem = SGRX_ScriptedItem::Create(
			m_level->GetScene(), m_level->GetPhyWorld(), m_level->m_scriptCtx.C,
			func, args );
		m_scrItem->SetLightSampler( m_level );
		m_scrItem->PreRender();
	}
}

ScriptedItem::~ScriptedItem()
{
	if( m_scrItem )
		m_scrItem->Release();
}

void ScriptedItem::FixedTick( float deltaTime )
{
	if( m_scrItem )
	{
		m_scrItem->FixedTick( deltaTime );
	}
}

void ScriptedItem::Tick( float deltaTime, float blendFactor )
{
	if( m_scrItem )
	{
		m_scrItem->Tick( deltaTime, blendFactor );
		m_scrItem->PreRender();
	}
}

void ScriptedItem::OnEvent( const StringView& type )
{
	if( m_scrItem )
	{
		m_scrItem->EntityEvent( type );
	}
}


StockEntityCreationSystem::StockEntityCreationSystem( GameLevel* lev ) : IGameLevelSystem( lev, e_system_uid )
{
	ScrItem_InstallAPI( lev->GetSGSC() );
	lev->GetScriptCtx().Include( "data/scritems" );
}

bool StockEntityCreationSystem::AddEntity( const StringView& type, sgsVariable data, sgsVariable& outvar )
{
	///////////////////////////
	if( type == "trigger" )
	{
		BoxTrigger* BT = new BoxTrigger
		(
			m_level,
			data.getprop("name").get<StringView>(),
			data.getprop("position").get<Vec3>(),
			Mat4::CreateRotationXYZ( DEG2RAD( data.getprop("rot_angles").get<Vec3>() ) ).GetRotationQuaternion(),
			data.getprop("scale_sep").get<Vec3>() * data.getprop("scale_uni").get<float>()
		);
		m_level->AddEntity( BT );
		outvar = BT->GetScriptedObject();
		return true;
	}
	
	///////////////////////////
	if( type == "trigger_prox" )
	{
		ProximityTrigger* PT = new ProximityTrigger
		(
			m_level,
			data.getprop("name").get<StringView>(),
			data.getprop("position").get<Vec3>(),
			data.getprop("distance").get<float>()
		);
		m_level->AddEntity( PT );
		outvar = PT->GetScriptedObject();
		return true;
	}
	
	///////////////////////////
	if( type == "door_slide" )
	{
		SlidingDoor* SD = new SlidingDoor
		(
			m_level,
			data.getprop("name").get<StringView>(),
			data.getprop("mesh").get<StringView>(),
			data.getprop("position").get<Vec3>(),
			Mat4::CreateRotationXYZ( DEG2RAD( data.getprop("rot_angles").get<Vec3>() ) ).GetRotationQuaternion(),
			data.getprop("scale_sep").get<Vec3>() * data.getprop("scale_uni").get<float>(),
			data.getprop("open_offset").get<Vec3>(),
			Mat4::CreateRotationXYZ( DEG2RAD( data.getprop("open_rot_angles").get<Vec3>() ) ).GetRotationQuaternion(),
			V3(0),
			Quat::Identity,
			data.getprop("open_time").get<float>(),
			false,
			data.getprop("is_switch").get<bool>()
		);
		m_level->AddEntity( SD );
		outvar = SD->GetScriptedObject();
		return true;
	}
	
	///////////////////////////
	if( type == "pickup" )
	{
		PickupItem* PI = new PickupItem
		(
			m_level,
			data.getprop("name").get<StringView>(),
			data.getprop("type").get<StringView>(),
			data.getprop("count").get<int>(),
			data.getprop("mesh").get<StringView>(),
			data.getprop("position").get<Vec3>(),
			Mat4::CreateRotationXYZ( DEG2RAD( data.getprop("rot_angles").get<Vec3>() ) ).GetRotationQuaternion(),
			data.getprop("scale_sep").get<Vec3>() * data.getprop("scale_uni").get<float>()
		);
		m_level->AddEntity( PI );
		outvar = PI->GetScriptedObject();
		return true;
	}
	
	///////////////////////////
	if( type == "actionable" )
	{
		Actionable* AC = new Actionable
		(
			m_level,
			data.getprop("name").get<StringView>(),
			data.getprop("mesh").get<StringView>(),
			data.getprop("position").get<Vec3>(),
			Mat4::CreateRotationXYZ( DEG2RAD( data.getprop("rot_angles").get<Vec3>() ) ).GetRotationQuaternion(),
			data.getprop("scale_sep").get<Vec3>() * data.getprop("scale_uni").get<float>(),
			data.getprop("place_offset").get<Vec3>(),
			data.getprop("place_dir").get<Vec3>()
		);
		m_level->AddEntity( AC );
		outvar = AC->GetScriptedObject();
		return true;
	}
	
	///////////////////////////
	CoverSystem* coverSys = m_level->GetSystem<CoverSystem>();
	if( type == "cover" && coverSys )
	{
		Mat4 mtx = Mat4::CreateSXT(
			data.getprop("scale_sep").get<Vec3>() * data.getprop("scale_uni").get<float>(),
			Mat4::CreateRotationXYZ( DEG2RAD( data.getprop("rot_angles").get<Vec3>() ) ),
			data.getprop("position").get<Vec3>() );
		coverSys->AddAABB( data.getprop("name").get<StringView>(), V3(-1), V3(1), mtx );
		return true;
	}
	
	///////////////////////////
	AIDBSystem* aidbSys = m_level->GetSystem<AIDBSystem>();
	if( type == "room" && aidbSys )
	{
		Mat4 mtx = Mat4::CreateSXT(
			data.getprop("scale_sep").get<Vec3>() * data.getprop("scale_uni").get<float>(),
			Mat4::CreateRotationXYZ( DEG2RAD( data.getprop("rot_angles").get<Vec3>() ) ),
			data.getprop("position").get<Vec3>() );
		aidbSys->AddRoomPart(
			data.getprop("name").get<StringView>(), mtx,
			data.getprop("negative").get<bool>(), data.getprop("cell_size").get<float>() );
	}
	
	///////////////////////////
	if( type == "particle_fx" )
	{
		ParticleFX* PF = new ParticleFX
		(
			m_level,
			data.getprop("name").get<StringView>(),
			data.getprop("partsys").get<StringView>(),
			data.getprop("soundevent").get<StringView>(),
			data.getprop("position").get<Vec3>(),
			Mat4::CreateRotationXYZ( DEG2RAD( data.getprop("rot_angles").get<Vec3>() ) ).GetRotationQuaternion(),
			data.getprop("scale_sep").get<Vec3>() * data.getprop("scale_uni").get<float>(),
			data.getprop("start").get<bool>()
		);
		m_level->AddEntity( PF );
		outvar = PF->GetScriptedObject();
		return true;
	}
	
	///////////////////////////
	if( type == "scritem" )
	{
		sgsVariable scritem = data.getprop("scritem");
		ScriptedItem* SI = new ScriptedItem
		(
			m_level,
			scritem.getprop("__type").get<StringView>(),
			scritem
		);
		m_level->AddEntity( SI );
		outvar = SI->GetScriptedObject();
		return true;
	}
	
	return false;
}




