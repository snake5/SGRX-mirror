

#include "level.hpp"
#include "systems.hpp"
#include "entities.hpp"


GameLevel* g_GameLevel = NULL;
SoundSystemHandle g_SoundSys;

InputState MOVE_LEFT( "move_left" );
InputState MOVE_RIGHT( "move_right" );
InputState MOVE_UP( "move_up" );
InputState MOVE_DOWN( "move_down" );
InputState INTERACT( "interact" );
InputState SHOOT( "shoot" );
InputState JUMP( "jump" );
InputState SLOW_WALK( "slow_walk" );
InputState SHOW_OBJECTIVES( "show_objectives" );



ActionInput g_i_move_left = ACTINPUT_MAKE_KEY( SDLK_a );
ActionInput g_i_move_right = ACTINPUT_MAKE_KEY( SDLK_d );
ActionInput g_i_move_up = ACTINPUT_MAKE_KEY( SDLK_w );
ActionInput g_i_move_down = ACTINPUT_MAKE_KEY( SDLK_s );
ActionInput g_i_interact = ACTINPUT_MAKE_KEY( SDLK_e );
ActionInput g_i_shoot = ACTINPUT_MAKE_MOUSE( SGRX_MB_LEFT );
ActionInput g_i_jump = ACTINPUT_MAKE_KEY( SDLK_SPACE );
ActionInput g_i_slow_walk = ACTINPUT_MAKE_KEY( SDLK_LSHIFT );
ActionInput g_i_show_objectives = ACTINPUT_MAKE_KEY( SDLK_TAB );

float g_i_mouse_sensitivity = 1.0f;
bool g_i_mouse_invert_x = false;
bool g_i_mouse_invert_y = false;

float g_s_vol_master = 0.8f;
float g_s_vol_sfx = 0.8f;
float g_s_vol_music = 0.8f;



bool endgame = false;

struct StartScreen : IScreen
{
	float m_timer;
	TextureHandle m_tx_logo;
	
	StartScreen() : m_timer(0)
	{
	}
	
	void OnStart()
	{
		m_timer = 0;
		m_tx_logo = GR_GetTexture( "ui/scr_title.png" );
	}
	void OnEnd()
	{
		m_tx_logo = NULL;
	}
	
	bool OnEvent( const Event& e )
	{
		if( e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SGRX_MB_LEFT )
		{
			Game_ShowCursor( false );
		//	g_GameLevel->StartLevel();
			m_timer = 5;
		}
		return true;
	}
	bool Draw( float delta )
	{
		BatchRenderer& br = GR2D_GetBatchRenderer();
		
		// logo
		const TextureInfo& texinfo = m_tx_logo.GetInfo();
		float scale = GR_GetWidth() / 1024.0f;
		br.Reset().SetTexture( m_tx_logo ).Box( GR_GetWidth() / 2.0f, GR_GetHeight() / 2.1f, texinfo.width * scale, texinfo.height * scale );
		
		br.Reset();
		GR2D_SetFont( "core", GR_GetHeight()/20 );
		GR2D_DrawTextLine( GR_GetWidth()/2,GR_GetHeight()*3/4, "Click to start", HALIGN_CENTER, VALIGN_CENTER );
		
		return m_timer >= 5;
	}
}
g_StartScreen;


struct EndScreen : IScreen
{
	float m_timer;
	
	EndScreen() : m_timer(0)
	{
	}
	
	void OnStart()
	{
		m_timer = 0;
	}
	void OnEnd()
	{
	}
	
	bool OnEvent( const Event& e )
	{
		return true;
	}
	bool Draw( float delta )
	{
		m_timer += delta;
		
		BatchRenderer& br = GR2D_GetBatchRenderer();
		br.Reset()
			.Col( 0, clamp( m_timer / 3, 0, 0.4f ) )
			.Quad( 0, 0, GR_GetWidth(), GR_GetHeight() );
		
		br.Reset().Col( 1 );
		GR2D_SetFont( "core", GR_GetHeight()/20 );
		GR2D_DrawTextLine( GR_GetWidth()/2,GR_GetHeight()*3/4, "Success!", HALIGN_CENTER, VALIGN_CENTER );
		
		return false;
	}
}
g_EndScreen;



struct MLD62Player : Entity
{
	PhyRigidBodyHandle m_bodyHandle;
	PhyShapeHandle m_shapeHandle;
	
	Vec2 m_angles;
	float m_jumpTimeout;
	float m_canJumpTimeout;
	float m_footstepTime;
	bool m_isOnGround;
	float m_isCrouching;
	
	// camera fix
	float m_bobPower;
	float m_bobTime;
	YawPitch m_weaponTurn;
	float m_wallRunTgt;
	float m_wallRunShow;
	Vec3 m_wallRunDir;
	
	IVState< Vec3 > m_ivPos;
	
	Vec2 inCursorMove;
	
	Entity* m_targetII;
	bool m_targetTriggered;
	
	TextureHandle m_tex_interact_icon;
	TextureHandle m_tex_dead_img;
	
	float m_health;
	float m_hitTimeout;
	float m_shootTimeout;
	float m_deadTimeout;
	AnimCharacter m_weapon;
	ParticleSystem m_shootPS;
	LightHandle m_shootLT;
	
	MLD62Player( GameLevel* lev, const Vec3& pos, const Vec3& dir );
	
	void Hit( float pwr );
	
	Mat4 GetBulletOutputMatrix();
	void FixedTick( float deltaTime );
	void Tick( float deltaTime, float blendFactor );
	void DrawUI();
	
	bool Alive() const { return m_health > 0; }
	Vec3 GetPosition(){ return m_bodyHandle->GetPosition(); }
};

MLD62Player::MLD62Player( GameLevel* lev, const Vec3& pos, const Vec3& dir ) :
	Entity( lev ),
	m_angles( V2( atan2( dir.y, dir.x ), atan2( dir.z, dir.ToVec2().Length() ) ) ),
	m_jumpTimeout(0), m_canJumpTimeout(0), m_footstepTime(0), m_isOnGround(false), m_isCrouching(0),
	m_bobPower(0), m_bobTime(0), m_weaponTurn(YP(0)), m_wallRunTgt(0), m_wallRunShow(0), m_wallRunDir(V3(0)),
	m_ivPos( pos ), inCursorMove( V2(0) ),
	m_targetII( NULL ), m_targetTriggered( false ),
	m_health(100), m_hitTimeout(0), m_shootTimeout(0), m_deadTimeout(0),
	m_weapon( lev->GetScene(), lev->GetPhyWorld() )
{
	// m_tex_interact_icon = GR_GetTexture( "ui/interact_icon.png" );
	m_tex_dead_img = GR_GetTexture( "ui/fscr_dead.png" );
	
	m_weapon.Load( "chars/weapon.chr" );
	m_weapon.m_cachedMeshInst->layers = 0x2;
	
	SGRX_PhyRigidBodyInfo rbinfo;
	rbinfo.friction = 0;
	rbinfo.restitution = 0;
	rbinfo.shape = m_level->GetPhyWorld()->CreateCylinderShape( V3(0.3f,0.3f,0.3f) );
	rbinfo.mass = 70;
	rbinfo.inertia = V3(0);
	rbinfo.position = pos + V3(0,0,1);
	rbinfo.canSleep = false;
	rbinfo.group = 2;
	m_bodyHandle = m_level->GetPhyWorld()->CreateRigidBody( rbinfo );
	m_shapeHandle = m_level->GetPhyWorld()->CreateCylinderShape( V3(0.29f,0.29f,0.3f) );
	
	m_shootPS.Load( "psys/gunflash.psy" );
	m_shootPS.AddToScene( m_level->GetScene() );
	m_shootPS.OnRenderUpdate();
	m_shootLT = m_level->GetScene()->CreateLight();
	m_shootLT->type = LIGHT_POINT;
	m_shootLT->enabled = false;
	m_shootLT->position = pos;
	m_shootLT->color = V3(0.9f,0.7f,0.5f)*1;
	m_shootLT->range = 4;
	m_shootLT->power = 2;
	m_shootLT->UpdateTransform();
	m_shootTimeout = 0;
}

void MLD62Player::Hit( float pwr )
{
	if( m_hitTimeout > 0 )
		return;
	
	if( Alive() )
	{
		m_hitTimeout += pwr / 40;
		m_health -= pwr;
		if( m_health <= 0 )
		{
			m_health = 0;
			// dead
		}
	}
}

Mat4 MLD62Player::GetBulletOutputMatrix()
{
	Mat4 out = m_weapon.m_cachedMeshInst->matrix;
	for( size_t i = 0; i < m_weapon.attachments.size(); ++i )
	{
		if( m_weapon.attachments[ i ].name == StringView("barrel") )
		{
			m_weapon.GetAttachmentMatrix( i, out );
			break;
		}
	}
	return out;
}

void MLD62Player::FixedTick( float deltaTime )
{
	SGRX_IPhyWorld* PW = m_level->GetPhyWorld();
	SGRX_PhyRaycastInfo rcinfo;
	SGRX_PhyRaycastInfo rcinfo2;
	
	m_jumpTimeout = TMAX( 0.0f, m_jumpTimeout - deltaTime );
	m_canJumpTimeout = TMAX( 0.0f, m_canJumpTimeout - deltaTime );
	m_hitTimeout = TMAX( 0.0f, m_hitTimeout - deltaTime );
	
	Vec3 pos = m_bodyHandle->GetPosition();
	m_ivPos.Advance( pos );
	
	bool slowWalk = false;//SLOW_WALK.value > 0.5f;
	m_isCrouching = 0;
	if( PW->ConvexCast( m_shapeHandle, pos + V3(0,0,0), pos + V3(0,0,3), 1, 1, &rcinfo ) &&
		PW->ConvexCast( m_shapeHandle, pos + V3(0,0,0), pos + V3(0,0,-3), 1, 1, &rcinfo2 ) &&
		fabsf( rcinfo.point.z - rcinfo2.point.z ) < 1.8f )
	{
		m_isCrouching = 1;
	}
	
	float cheight = m_isCrouching ? 0.5f : 1.5f;
	if( Alive() == false )
		cheight = 0.3f;
	float rcxdist = 1.0f;
	Vec3 lvel = m_bodyHandle->GetLinearVelocity();
	float ht = cheight - 0.29f;
	if( lvel.z >= -SMALL_FLOAT && !m_jumpTimeout )
		ht += rcxdist;
	
	bool ground = false;
	if( PW->ConvexCast( m_shapeHandle, pos + V3(0,0,0), pos + V3(0,0,-ht), 1, 1, &rcinfo )
		&& fabsf( rcinfo.point.z - pos.z ) < cheight + SMALL_FLOAT )
	{
		Vec3 v = m_bodyHandle->GetPosition();
		float tgt = rcinfo.point.z + cheight;
		v.z += TMIN( deltaTime * 5, fabsf( tgt - v.z ) ) * sign( tgt - v.z );
		m_bodyHandle->SetPosition( v );
		lvel.z = 0;
		ground = true;
		m_canJumpTimeout = 0.5f;
	}
	if( !m_jumpTimeout && m_canJumpTimeout && JUMP.value && Alive() )
	{
		lvel.z = 5;
		m_jumpTimeout = 0.5f;
		
		SoundEventInstanceHandle fsev = g_SoundSys->CreateEventInstance( "/footsteps" );
		SGRX_Sound3DAttribs s3dattr = { pos, lvel, V3(0), V3(0) };
		fsev->Set3DAttribs( s3dattr );
		fsev->Start();
	}
	
	if( !m_isOnGround && ground )
	{
		m_footstepTime = 1.0f;
	}
	m_isOnGround = ground;
	
	Vec2 realdir = { cos( m_angles.x ), sin( m_angles.x ) };
	Vec2 perpdir = realdir.Perp();
	
	Vec2 md = { MOVE_LEFT.value - MOVE_RIGHT.value, MOVE_DOWN.value - MOVE_UP.value };
	if( Alive() == false )
		md = V2(0);
	md.Normalize();
	md = -realdir * md.y - perpdir * md.x;
	
	Vec2 lvel2 = lvel.ToVec2();
	
	float maxspeed = 5;
	float accel = ( md.NearZero() && !m_isCrouching ) ? 38 : 30;
	if( m_isCrouching ){ accel = 5; maxspeed = 2.5f; }
	if( !ground ){ accel = 10; }
	if( slowWalk ){ accel *= 0.5f; maxspeed *= 0.5f; }
	
	float curspeed = Vec2Dot( lvel2, md );
	float revmaxfactor = clamp( maxspeed - curspeed, 0, 1 );
	lvel2 += md * accel * revmaxfactor * deltaTime;
	
	///// WALLRUN /////
	m_wallRunTgt = 0;
	if( JUMP.value && Alive() && ground == false )
	{
		if( PW->ConvexCast( m_shapeHandle, pos, pos + V3( md.x, md.y, 0 ), 1, 1, &rcinfo ) )
		{
			m_wallRunDir = rcinfo.normal;
			m_bodyHandle->ApplyCentralForce( PFT_Acceleration, V3(0,0,40) );
			m_wallRunTgt = 1;
		}
	}
	
	///// FRICTION /////
	curspeed = Vec2Dot( lvel2, md );
	if( ground )
	{
		if( curspeed > maxspeed )
			curspeed = maxspeed;
	}
	lvel2 -= md * curspeed;
	{
		Vec2 ldd = lvel2.Normalized();
		float llen = lvel2.Length();
		llen = TMAX( 0.0f, llen - deltaTime * ( ground ? 20 : ( m_isCrouching ? 0.5f : 3 ) ) );
		lvel2 = ldd * llen;
	}
	lvel2 += md * curspeed;
	
	// if( llen > 5 )
	// 	llen = 5;
	// else if( !md.Length() )
	// 	llen = TMAX( 0.0f, llen - deltaTime * ( ground ? 20 : 3 ) );
	
//	float accel = ground ? 3 : 1;
//	float dirchange = ground ? 1 : 0.1f;
//	float resist = ground ? 20 : 3;
//	
//	if( ground && lvel2.Length() < 2 )
//		accel *= pow( 10, 1 - lvel2.Length() / 2 ); // startup boost
//	
//	ldd = ( ldd + md * dirchange ).Normalized();
//	if( md.Length() )
//		llen += Vec2Dot( ldd, md ) * TMAX( 0.0f, 5 - llen ) * accel * deltaTime;
//	else
//		llen = TMAX( 0.0f, llen - deltaTime * resist );
	// lvel2 = ldd * llen;
	
//	lvel.x = md.x * 4;
//	lvel.y = md.y * 4;
	lvel.x = lvel2.x;
	lvel.y = lvel2.y;
	
	m_bodyHandle->SetLinearVelocity( lvel );
	
	InfoEmissionSystem::Data D = { pos, 0.5f, IEST_HeatSource | IEST_Player };
	m_level->GetSystem<InfoEmissionSystem>()->UpdateEmitter( this, D );
	
	m_weapon.FixedTick( deltaTime );
}

void MLD62Player::Tick( float deltaTime, float blendFactor )
{
	m_angles += inCursorMove * V2(-0.01f);
	m_angles.y = clamp( m_angles.y, (float) -M_PI/2 + SMALL_FLOAT, (float) M_PI/2 - SMALL_FLOAT );
	
	float ch = cos( m_angles.x ), sh = sin( m_angles.x );
	float cv = cos( m_angles.y ), sv = sin( m_angles.y );
	
	Vec3 pos = m_ivPos.Get( blendFactor );
	Vec3 dir = V3( ch * cv, sh * cv, sv );
	
	SGRX_Sound3DAttribs s3dattr = { pos, m_bodyHandle->GetLinearVelocity(), dir, V3(0,0,1) };
	
	float needfs = m_isOnGround || m_wallRunShow > 0;
	m_footstepTime += deltaTime * needfs * m_bodyHandle->GetLinearVelocity().Length() * ( 0.6f + m_isCrouching * 0.3f );
	if( m_footstepTime >= 1 )//&& SLOW_WALK.value >= 0.5f )
	{
		SoundEventInstanceHandle fsev = g_SoundSys->CreateEventInstance( "/footsteps" );
		fsev->SetVolume( 1.0f - m_isCrouching * 0.3f );
		fsev->Set3DAttribs( s3dattr );
		fsev->Start();
		m_footstepTime = 0;
		m_level->GetSystem<AIDBSystem>()->AddSound( pos, 10, 0.5f, AIS_Footstep );
	}
	
	if( Alive() == false )
		m_deadTimeout += deltaTime;
	
	///// BOBBING /////
	Vec2 md = { MOVE_LEFT.value - MOVE_RIGHT.value, MOVE_DOWN.value - MOVE_UP.value };
	if( Alive() == false )
		md = V2(0);
	float bobtgt = m_isOnGround && md.Length() > 0.5f ? 1.0f : 0.0f;
	if( m_isCrouching ) bobtgt *= 0.3f;
//	if( SLOW_WALK.value ) bobtgt *= 0.5f;
	float bobdiff = bobtgt - m_bobPower;
	m_bobPower += TMIN( fabsf( bobdiff ), deltaTime * 5 ) * sign( bobdiff );
	m_weaponTurn.yaw += inCursorMove.x * 0.004f;
	m_weaponTurn.pitch += inCursorMove.y * 0.004f;
	m_weaponTurn.TurnTo( YP(0), YawPitchDist(m_weaponTurn,YP(0)).Abs().Scaled(deltaTime*10) );
	
	float diff = m_wallRunTgt - m_wallRunShow;
	m_wallRunShow += TMIN( fabsf( diff ), deltaTime * 4 ) * sign( diff );
//	Vec2 realdir = { cos( m_angles.x ), sin( m_angles.x ) };
//	Vec2 perpdir = realdir.Perp();
//	Vec3 wallrundir = V3(perpdir.x,perpdir.y,0);
//	float weaponturnfac = 1;
//	if( Vec3Dot( m_wallRunDir, wallrundir ) < 0 )
//		weaponturnfac = -1;
	
	Vec3 campos = pos;
	campos.z += m_bobPower * 0.1f + fabsf( sinf( m_bobTime * 3 ) ) * 0.1f;
	m_bobTime += deltaTime * m_isOnGround * m_bodyHandle->GetLinearVelocity().Length() * ( 0.6f + m_isCrouching * 0.3f );
	
	SGRX_Scene* S = m_level->GetScene();
	S->camera.znear = 0.1f;
	S->camera.angle = 90;
	S->camera.direction = dir;
	S->camera.position = campos;
	S->camera.updir = ( V3(0,0,1) + m_wallRunDir * 0.1f * m_wallRunShow ).Normalized();
	S->camera.UpdateMatrices();
	
	g_SoundSys->Set3DAttribs( s3dattr );
	
	m_targetII = m_level->GetSystem<InfoEmissionSystem>()->QueryOneRay( pos, pos + dir, IEST_InteractiveItem );
	if( m_targetII && INTERACT.value && !m_targetTriggered )
	{
		m_targetTriggered = true;
		m_targetII->OnEvent( "trigger_switch" );
	}
	else if( !INTERACT.value )
	{
		m_targetTriggered = false;
	}
	
	float wpnbob = fabs(sinf(m_bobTime*3))*m_bobPower*0.01f;
	Mat4 mtx = Mat4::Identity;
	mtx = mtx * Mat4::CreateRotationX( -FLT_PI / 2 );
	mtx = mtx * Mat4::CreateTranslation( -0.3f, -0.6f+wpnbob, +0.5f );
	mtx = mtx * Mat4::CreateScale( V3( 0.2f ) );
//	mtx = mtx * Mat4::CreateRotationZ( m_wallRunShow * weaponturnfac * 0.2f );
	mtx = mtx * Mat4::CreateRotationX( -m_weaponTurn.pitch );
	mtx = mtx * Mat4::CreateRotationY( m_weaponTurn.yaw );
//	mtx = mtx * Mat4::CreateRotationBetweenVectors( V3(0,0,1), S->camera.updir );
	mtx = mtx * m_level->GetScene()->camera.mInvView;
	
	Vec3 tr = mtx.GetTranslation();
	mtx = mtx * Mat4::CreateTranslation( -tr );
	mtx = mtx * Mat4::CreateRotationBetweenVectors( V3(0,0,1), S->camera.updir );
	mtx = mtx * Mat4::CreateTranslation( tr );
	
	m_weapon.SetTransform( mtx );
	m_weapon.PreRender( blendFactor );
	m_level->LightMesh( m_weapon.m_cachedMeshInst );
	
	
	m_shootLT->enabled = false;
	if( m_shootTimeout > 0 )
	{
		m_shootTimeout -= deltaTime;
		m_shootLT->enabled = true;
	}
	Mat4 bmtx = GetBulletOutputMatrix();
	m_shootPS.SetTransform( bmtx );
	if( Alive() && SHOOT.value && m_shootTimeout <= 0 )
	{
		Vec3 aimtgt = campos + dir * 100;
		Vec3 origin = bmtx.TransformPos( V3(0) );
		Vec3 dir = ( aimtgt - origin ).Normalized();
		dir = ( dir + V3( randf11(), randf11(), randf11() ) * 0.02f ).Normalized();
		m_level->GetSystem<BulletSystem>()->Add( origin, dir * 100, 1, 1, GAT_Player );
		m_shootPS.Trigger();
		m_shootLT->position = origin;
		m_shootLT->UpdateTransform();
		m_shootLT->enabled = true;
		m_shootTimeout += 0.1f;
		m_level->GetSystem<AIDBSystem>()->AddSound( pos, 10, 0.2f, AIS_Shot );
		
		SoundEventInstanceHandle fsev = g_SoundSys->CreateEventInstance( "/gunshot" );
	//	fsev->SetVolume( 1.0f );
		fsev->Set3DAttribs( s3dattr );
		fsev->Start();
	}
	m_shootLT->color = V3(0.9f,0.7f,0.5f) * smoothlerp_oneway( m_shootTimeout, 0, 0.1f );
	
	
	m_shootPS.Tick( deltaTime );
	m_shootPS.PreRender();
}

void MLD62Player::DrawUI()
{
	GR2D_GetBatchRenderer().Reset().Col(1);
	GR2D_SetFont( "core", 12 );
	char buf[ 150 ];
	sprintf( buf, "speed: %g", m_bodyHandle->GetLinearVelocity().ToVec2().Length() );
//	GR2D_DrawTextLine( 16, 16, buf );
	
	if( m_targetII )
	{
		float bsz = TMIN( GR_GetWidth(), GR_GetHeight() );
		float x = GR_GetWidth() / 2.0f;
		float y = GR_GetHeight() / 2.0f;
		GR2D_GetBatchRenderer().Reset().Col(1).SetTexture( m_tex_interact_icon ).QuadWH( x, y, bsz / 10, bsz / 10 );
	}
}



enum EBossEyeAction
{
	BEA_None = 0,
	BEA_FollowTarget,
	BEA_FollowPlayer,
	BEA_Charge,
	BEA_Shoot,
	BEA_Cooldown,
	BEA_Malfunction,
};

#define LASER_MAX_WIDTH 0.15f
#define FLARE_MAX_SIZE 3.0f

struct MLD62_BossEye : Entity, SGRX_MeshInstUserData
{
	MLD62_BossEye( GameLevel* lev, StringView name, Vec3 pos, Vec3 dir );
	
	void OnEvent( SGRX_MeshInstance* MI, uint32_t evid, void* data );
	void Hit( float pwr );
	
	void Tick( float deltaTime, float blendFactor );
	
	void SetState( EBossEyeAction ns );
	
	EBossEyeAction m_state;
	float m_health;
	float m_timeInState;
	float m_hitTimeout;
	float m_laserWidth;
	float m_flareSize;
	Vec3 m_target;
	Vec3 m_position;
	YawPitch m_direction;
	MeshInstHandle m_coreMesh;
	MeshInstHandle m_shieldMesh;
	MeshInstHandle m_laserMesh;
	PhyRigidBodyHandle m_body;
	
	ParticleSystem m_shootPS;
	LightHandle m_shootLT;
};

MLD62_BossEye::MLD62_BossEye( GameLevel* lev, StringView name, Vec3 pos, Vec3 dir ) :
	Entity( lev ), m_state( BEA_FollowPlayer ),
	m_health(100), m_timeInState(0), m_hitTimeout(0),
	m_laserWidth(1), m_flareSize(1), m_target(V3(0)),
	m_position( pos ), m_direction( YP( dir ) )
{
	m_coreMesh = lev->GetScene()->CreateMeshInstance();
	m_coreMesh->SetMesh( "meshes/eye.ssm" );
	m_coreMesh->userData = (SGRX_MeshInstUserData*) this;
	m_shieldMesh = lev->GetScene()->CreateMeshInstance();
	m_shieldMesh->SetMesh( "meshes/shield.ssm" );
	m_shieldMesh->layers = 0x4;
	m_laserMesh = lev->GetScene()->CreateMeshInstance();
	m_laserMesh->SetMesh( "meshes/laser.ssm" );
	m_laserMesh->layers = 0x4;
	
	SGRX_PhyRigidBodyInfo rbinfo;
	rbinfo.friction = 0;
	rbinfo.restitution = 0;
	rbinfo.shape = lev->GetPhyWorld()->CreateSphereShape( 1.1f );
	rbinfo.mass = 0;
	rbinfo.inertia = V3(0);
	rbinfo.position = pos;
	rbinfo.canSleep = false;
	rbinfo.group = 2;
	m_body = lev->GetPhyWorld()->CreateRigidBody( rbinfo );
	
	m_shootPS.Load( "psys/defaulthit.psy" );
	m_shootPS.AddToScene( m_level->GetScene() );
	m_shootPS.OnRenderUpdate();
	m_shootLT = m_level->GetScene()->CreateLight();
	m_shootLT->type = LIGHT_POINT;
	m_shootLT->enabled = false;
	m_shootLT->position = pos;
	m_shootLT->color = V3(2.0f,0.05f,0.01f)*1;
	m_shootLT->range = 16;
	m_shootLT->power = 2;
	m_shootLT->UpdateTransform();
}

void MLD62_BossEye::OnEvent( SGRX_MeshInstance* MI, uint32_t evid, void* data )
{
	if( evid == MIEVT_BulletHit )
	{
		SGRX_CAST( MI_BulletHit_Data*, bhinfo, data );
		Hit( bhinfo->vel.Length() * 0.01f );
	}
}

void MLD62_BossEye::Hit( float pwr )
{
	if( m_health > 0 )
	{
		m_health -= pwr;
		m_hitTimeout = 0.1f;
		if( m_health <= 0 )
		{
			SetState( BEA_Malfunction );
		}
	}
}

void MLD62_BossEye::Tick( float deltaTime, float blendFactor )
{
	m_hitTimeout = TMAX( 0.0f, m_hitTimeout - deltaTime );
	
	Vec4 col = V4(0);
	if( m_hitTimeout > 0 )
		col = V4( 1, 1, 1, 0.5f );
	
	Mat4 mtx = 
		Mat4::CreateRotationY( -m_direction.pitch ) *
		Mat4::CreateRotationZ( m_direction.yaw ) *
		Mat4::CreateTranslation( m_position );
	m_coreMesh->matrix = mtx;
	m_coreMesh->constants[0] = col;
	m_shieldMesh->matrix = mtx;
	Mat4 laserMtx =
		Mat4::CreateTranslation( V3(0,0,1.11f) ) *
		Mat4::CreateRotationY( FLT_PI/2 ) *
		mtx;
	
	m_timeInState += deltaTime;
	switch( m_state )
	{
	case BEA_None:
		break;
	case BEA_FollowTarget:
		{
			YawPitch targetDir = YP( ( m_target - m_position ).Normalized() );
			m_direction.TurnTo( targetDir, YP( deltaTime ) );
			if( YawPitchAlmostEqual( m_direction, targetDir ) )
			{
				SetState( BEA_Charge );
				break;
			}
		}
		m_laserWidth = LASER_MAX_WIDTH;
		m_flareSize = 1.0f;
		break;
	case BEA_FollowPlayer:
		if( m_level->m_player )
		{
			SGRX_CAST( MLD62Player*, P, m_level->m_player );
			if( m_level->GetPhyWorld()->Raycast( m_body->GetPosition(), P->GetPosition(), 0x0001, 0x0001 ) == false )
			{
				YawPitch targetDir = YP( ( P->GetPosition() - m_position ).Normalized() );
				m_direction.TurnTo( targetDir, YP( deltaTime ) );
				if( YawPitchAlmostEqual( m_direction, targetDir ) )
				{
					SetState( BEA_Charge );
					break;
				}
			}
		}
		m_laserWidth = LASER_MAX_WIDTH;
		m_flareSize = 1.0f;
		break;
	case BEA_Charge:
		m_laserWidth = TLERP( LASER_MAX_WIDTH, 0.02f, clamp( m_timeInState / 3, 0, 1 ) );
		m_flareSize = TLERP( 1.0f, FLARE_MAX_SIZE, clamp( m_timeInState / 3, 0, 1 ) );
		if( m_timeInState >= 3 )
		{
			SetState( BEA_Shoot );
			break;
		}
		break;
	case BEA_Shoot:
		m_flareSize = 5;
		if( m_timeInState >= 0.1f )
		{
			m_shootLT->enabled = false;
			SetState( BEA_Cooldown );
			break;
		}
		break;
	case BEA_Cooldown:
		m_flareSize = 0;
		m_laserWidth = 0;
		if( m_timeInState >= 3.0f )
		{
			m_shieldMesh->enabled = true;
			SetState( BEA_FollowPlayer );
			break;
		}
		break;
	case BEA_Malfunction:
		m_flareSize = 0;
		m_laserWidth = 0;
		m_shootLT->enabled = fmodf( m_timeInState, 0.5f ) < 0.25f;
		m_shieldMesh->enabled = false;
		{
			float prevtime = m_timeInState - deltaTime;
			if( fmodf( prevtime, 0.2f ) < 0.1f && fmodf( m_timeInState, 0.2f ) >= 0.1f )
			{
				Vec3 randdir = V3(randf11(),randf11(),randf11()).Normalized();
				m_target = m_body->GetPosition() + randdir;
				Mat4 emitdir = Mat4::CreateScale( V3(2) ) *
					Mat4::CreateTranslation( V3(0,0,1) ) *
					Mat4::CreateRotationBetweenVectors( V3(0,0,1), randdir ) * mtx;
				m_shootPS.SetTransform( emitdir );
				m_shootPS.Trigger();
			}
			YawPitch targetDir = YP( ( m_target - m_position ).Normalized() );
			m_direction.TurnTo( targetDir, YP( deltaTime ) );
		}
		break;
	}
	
	float dist = 100;
	SceneRaycastInfo rcinfo;
	if( m_level->GetScene()->RaycastOne(
		laserMtx.TransformPos( V3(0,0,0) ),
		laserMtx.TransformPos( V3(0,0,dist) ), &rcinfo, 0x1 ) )
	{
		dist *= rcinfo.factor;
	}
	
	FlareSystem* FS = m_level->GetSystem<FlareSystem>();
	FSFlare laserFlare = { laserMtx.TransformPos( V3(0,0,0) ), V3(2.0f,0.05f,0.01f), m_flareSize, true };
	FS->UpdateFlare( this, laserFlare );
	laserFlare.pos = laserMtx.TransformPos( V3(0,0,dist-0.01f) );
	laserFlare.size = m_laserWidth ? 1.0f : 0;
	FS->UpdateFlare( ((char*)this)+1, laserFlare );
	
	m_laserMesh->matrix = Mat4::CreateScale( m_laserWidth, m_laserWidth, dist ) * laserMtx;
	
	
	m_shootPS.Tick( deltaTime );
	m_shootPS.PreRender();
}

void MLD62_BossEye::SetState( EBossEyeAction ns )
{
	switch( ns )
	{
	case BEA_Shoot:
		{
			Mat4 lm = m_laserMesh->matrix;
			m_level->GetSystem<BulletSystem>()->Add(
				lm.TransformPos( V3(0) ),
				lm.TransformNormal( V3(0,0,1) ).Normalized() * 1000, 1, 1, GAT_Enemy );
			if( m_level->m_player )
			{
				SGRX_CAST( MLD62Player*, P, m_level->m_player );
				
				SceneRaycastInfo rcinfo;
				Vec3 p0 = lm.TransformPos(V3(0)), p1 = lm.TransformPos(V3(0,0,100));
				if( m_level->GetScene()->RaycastOne( p0, p1, &rcinfo, 0x1 ) )
				{
					Vec3 hitpos = TLERP( p0, p1, rcinfo.factor );
					float dist = ( P->GetPosition() - hitpos ).Length();
					P->Hit( clamp( 1 - dist * 0.1f, 0, 1 ) * 40 );
				}
				
				SGRX_PhyRaycastInfo prci;
				if( m_level->GetPhyWorld()->Raycast( p0, p1, 0x1, 0xffff, &prci ) && prci.body == P->m_bodyHandle )
				{
					P->Hit( 40 );
				}
			}
			
			m_shootLT->enabled = true;
			m_shieldMesh->enabled = false;
			
			SGRX_Sound3DAttribs s3dattr = { lm.TransformPos( V3(0) ), V3(0), lm.TransformNormal( V3(0,0,1) ).Normalized(), V3(0,0,1) };
			SoundEventInstanceHandle fsev = g_SoundSys->CreateEventInstance( "/lasershot" );
			fsev->Set3DAttribs( s3dattr );
			fsev->Start();
		}
		break;
	case BEA_Cooldown:
		break;
	case BEA_Malfunction:
		Game_AddOverlayScreen( &g_EndScreen );
		Game_ShowCursor( true );
		SHOOT.value = 0;
		endgame = true;
		break;
	default:
		break;
	}
	
	m_state = ns;
	m_timeInState = 0;
}



struct MLD62_RoboSaw : Entity, SGRX_MeshInstUserData
{
	MLD62_RoboSaw( GameLevel* lev, StringView name, Vec3 pos, Vec3 dir );
	
	void OnEvent( SGRX_MeshInstance* MI, uint32_t evid, void* data );
	void Hit( float pwr );
	
	void FixedTick( float deltaTime );
	void Tick( float deltaTime, float blendFactor );
	
	float m_health;
	float m_sawRotation;
	float m_hitTimeout;
	Vec3 m_position;
	MeshInstHandle m_coreMesh;
	MeshInstHandle m_coreMeshSaw;
	PhyRigidBodyHandle m_body;
	
	IVState< Vec3 > m_ivPos;
	IVState< Quat > m_ivRot;
};

MLD62_RoboSaw::MLD62_RoboSaw( GameLevel* lev, StringView name, Vec3 pos, Vec3 dir ) :
	Entity( lev ),
	m_health( 20 ), m_sawRotation( 0 ), m_hitTimeout( 0 ), m_position( pos ),
	m_ivPos( pos ), m_ivRot( Quat::Identity )
{
	m_coreMesh = lev->GetScene()->CreateMeshInstance();
	m_coreMesh->SetMesh( "meshes/robosaw.ssm" );
	m_coreMesh->userData = (SGRX_MeshInstUserData*) this;
	m_coreMeshSaw = lev->GetScene()->CreateMeshInstance();
	m_coreMeshSaw->SetMesh( "meshes/robosaw.ssm" );
	m_coreMeshSaw->userData = (SGRX_MeshInstUserData*) this;
	
	m_coreMesh->GetMaterial( 0 ).flags |= SGRX_MtlFlag_Disable;
	m_coreMeshSaw->GetMaterial( 1 ).flags |= SGRX_MtlFlag_Disable;
	m_coreMesh->OnUpdate();
	m_coreMeshSaw->OnUpdate();
	
	SGRX_PhyRigidBodyInfo rbinfo;
	rbinfo.friction = 1.0f;
	rbinfo.restitution = 0.1f;
	rbinfo.shape = lev->GetPhyWorld()->CreateSphereShape( 0.5f );
	rbinfo.shape->SetScale( V3(0.7f,0.7f,0.32f) );
	rbinfo.mass = 5;
	rbinfo.angularDamping = 0.5f;
	rbinfo.inertia = rbinfo.shape->CalcInertia( rbinfo.mass ) * 0.1f;
	rbinfo.position = pos;
	rbinfo.rotation = Quat::CreateAxisAngle( V3(0,0,1), atan2(dir.y,dir.x) );
	rbinfo.canSleep = false;
	rbinfo.group = 2;
	m_body = lev->GetPhyWorld()->CreateRigidBody( rbinfo );
}

void MLD62_RoboSaw::OnEvent( SGRX_MeshInstance* MI, uint32_t evid, void* data )
{
	if( evid == MIEVT_BulletHit )
	{
		SGRX_CAST( MI_BulletHit_Data*, bhinfo, data );
		m_body->ApplyForce( PFT_Impulse, bhinfo->vel * 0.1f, bhinfo->pos );
		Hit( bhinfo->vel.Length() * 0.1f );
	}
}

void MLD62_RoboSaw::Hit( float pwr )
{
	if( m_health > 0 )
	{
		m_hitTimeout = 0.1f;
		m_health -= pwr;
		if( m_health <= 0 )
		{
			FlareSystem* FS = m_level->GetSystem<FlareSystem>();
			FS->RemoveFlare( this );
		}
	}
}

void MLD62_RoboSaw::FixedTick( float deltaTime )
{
	SGRX_CAST( MLD62Player*, P, m_level->m_player );
	bool canSeePlayer = false;
	Vec3 pos = m_body->GetPosition();
	if( m_level->m_player )
	{
		if( m_level->GetPhyWorld()->Raycast( pos, P->GetPosition(), 0x0001, 0x0001 ) == false )
		{
			canSeePlayer = true;
		}
	}
	
	Vec3 force = V3(0);
	{
		Vec3 gnd = pos + V3(0,0,-100);
		SGRX_PhyRaycastInfo info;
		if( m_level->GetPhyWorld()->Raycast( pos, gnd, 0x0001, 0xffff, &info ) )
		{
			gnd = TLERP( pos, gnd, info.factor );
		}
		Vec3 tgt = gnd + V3(0,0,1.5f);
		float len = ( tgt - pos ).Length();
		if( tgt.z > pos.z )
			force += ( tgt - pos ).Normalized() * TMIN( len, 1.0f ) * 50 - m_body->GetLinearVelocity();
	}
	if( canSeePlayer )
	{
		Vec3 tgt = P->GetPosition();
		float len = ( tgt - pos ).Length();
		if( tgt.z > pos.z )
			force += ( tgt - pos ).Normalized() * TMIN( len, 0.2f ) * 50 - m_body->GetLinearVelocity();
		
		if( len < 0.9f )
		{
			P->Hit( 10.0f );
			force += ( pos - tgt ).Normalized() * 100;
		}
	}
	
	if( m_health > 0 )
	{
		Vec3 angvel = m_body->GetAngularVelocity() * 0.5f
			+ V3(1,1,0) * 100 * deltaTime * PHY_QuaternionToEulerXYZ( m_body->GetRotation().Inverted() );
		if( canSeePlayer )
		{
			Vec3 dir = Mat4::CreateRotationFromQuat( m_body->GetRotation() ).TransformNormal( V3(-1,0,0) );
			angvel.z += YawPitchDist( YP( P->GetPosition() - pos ), YP( dir ) ).yaw * 10 * deltaTime;
		}
		
		m_body->ApplyCentralForce( PFT_Velocity, force * deltaTime );
		m_body->SetAngularVelocity( angvel );
	}
	
	m_ivPos.Advance( pos );
	m_ivRot.Advance( m_body->GetRotation() );
}

void MLD62_RoboSaw::Tick( float deltaTime, float blendFactor )
{
	m_hitTimeout = TMAX( 0.0f, m_hitTimeout - deltaTime );
	
	if( endgame )
		Hit(1000);
	
	if( m_health > 0 )
	{
		m_sawRotation += deltaTime * 100;
	}
	
	Vec4 col = V4(0);
	if( m_hitTimeout > 0 )
		col = V4( 1, 1, 1, 0.5f );
	
	FlareSystem* FS = m_level->GetSystem<FlareSystem>();
	Mat4 mtx =
		Mat4::CreateScale( V3(0.5f) ) *
		Mat4::CreateRotationFromQuat( m_ivRot.Get( blendFactor ) ) *
		Mat4::CreateTranslation( m_ivPos.Get( blendFactor ) );
	m_coreMesh->matrix = mtx;
	m_coreMeshSaw->matrix = Mat4::CreateRotationZ( m_sawRotation ) * mtx;
	m_coreMesh->constants[ 0 ] = col;
	m_coreMeshSaw->constants[ 0 ] = col;
	if( m_health > 0 )
	{
		FSFlare statusFlare = { mtx.TransformPos( V3(0.251f,0.151f,0.155f) ), V3(2.0f,0.05f,0.01f), 1.0f, true };
		FS->UpdateFlare( this, statusFlare );
	}
}



struct MLD62EntityCreationSystem : IGameLevelSystem
{
	enum { e_system_uid = 1000 };
	MLD62EntityCreationSystem( GameLevel* lev );
	virtual bool AddEntity( const StringView& type, sgsVariable data );
	virtual void DrawUI();
};


MLD62EntityCreationSystem::MLD62EntityCreationSystem( GameLevel* lev ) : IGameLevelSystem( lev, e_system_uid )
{
}

bool MLD62EntityCreationSystem::AddEntity( const StringView& type, sgsVariable data )
{
	///////////////////////////
	if( type == "player" )
	{
		MLD62Player* P = new MLD62Player
		(
			m_level,
			data.getprop("position").get<Vec3>(),
			data.getprop("viewdir").get<Vec3>()
		);
		m_level->AddEntity( P );
		m_level->SetPlayer( P );
		return true;
	}
	
	///////////////////////////
	if( type == "enemy_start" )
	{
		StringView type = data.getprop("type").get<StringView>();
		if( type == "eye" )
		{
			MLD62_BossEye* E = new MLD62_BossEye
			(
				m_level,
				data.getprop("name").get<StringView>(),
				data.getprop("position").get<Vec3>(),
				data.getprop("viewdir").get<Vec3>()
			);
			m_level->AddEntity( E );
			return true;
		}
		if( type == "robosaw" )
		{
			MLD62_RoboSaw* E = new MLD62_RoboSaw
			(
				m_level,
				data.getprop("name").get<StringView>(),
				data.getprop("position").get<Vec3>(),
				data.getprop("viewdir").get<Vec3>()
			);
			m_level->AddEntity( E );
			return true;
		}
	}
	return false;
}

void MLD62EntityCreationSystem::DrawUI()
{
#if 0
	SGRX_CAST( MLD62Player*, P, m_level->m_player );
	if( P )
		P->DrawUI();
#endif
}



#define MAX_TICK_SIZE (1.0f/15.0f)
#define FIXED_TICK_SIZE (1.0f/30.0f)

struct SciFiBossFightGame : IGame
{
	SciFiBossFightGame() : m_accum( 0.0f ), m_lastFrameReset( false )
	{
	}
	
	bool OnConfigure( int argc, char** argv )
	{
		return true;
	}
	
	bool OnInitialize()
	{
		GR2D_LoadFont( "core", "fonts/lato-regular.ttf" );
		
		g_SoundSys = SND_CreateSystem();
		
		Game_RegisterAction( &MOVE_LEFT );
		Game_RegisterAction( &MOVE_RIGHT );
		Game_RegisterAction( &MOVE_UP );
		Game_RegisterAction( &MOVE_DOWN );
		Game_RegisterAction( &INTERACT );
		Game_RegisterAction( &SHOOT );
		Game_RegisterAction( &JUMP );
		Game_RegisterAction( &SLOW_WALK );
		Game_RegisterAction( &SHOW_OBJECTIVES );
		
		Game_BindInputToAction( g_i_move_left, &MOVE_LEFT );
		Game_BindInputToAction( g_i_move_right, &MOVE_RIGHT );
		Game_BindInputToAction( g_i_move_up, &MOVE_UP );
		Game_BindInputToAction( g_i_move_down, &MOVE_DOWN );
		Game_BindInputToAction( g_i_interact, &INTERACT );
		Game_BindInputToAction( g_i_shoot, &SHOOT );
		Game_BindInputToAction( g_i_jump, &JUMP );
		Game_BindInputToAction( g_i_slow_walk, &SLOW_WALK );
		Game_BindInputToAction( g_i_show_objectives, &SHOW_OBJECTIVES );
		
		g_SoundSys->Load( "sound/master.bank" );
		g_SoundSys->Load( "sound/master.strings.bank" );
		
		g_SoundSys->SetVolume( "bus:/", g_s_vol_master );
		g_SoundSys->SetVolume( "bus:/music", g_s_vol_music );
		g_SoundSys->SetVolume( "bus:/sfx", g_s_vol_sfx );
		
		g_GameLevel = new GameLevel( PHY_CreateWorld() );
		g_GameLevel->SetGlobalToSelf();
		g_GameLevel->GetPhyWorld()->SetGravity( V3( 0, 0, -9.81f ) );
		AddSystemToLevel<InfoEmissionSystem>( g_GameLevel );
	//	AddSystemToLevel<MessagingSystem>( g_GameLevel );
	//	AddSystemToLevel<ObjectiveSystem>( g_GameLevel );
		AddSystemToLevel<HelpTextSystem>( g_GameLevel );
		AddSystemToLevel<FlareSystem>( g_GameLevel );
		AddSystemToLevel<LevelCoreSystem>( g_GameLevel );
		AddSystemToLevel<ScriptedSequenceSystem>( g_GameLevel );
		AddSystemToLevel<MusicSystem>( g_GameLevel );
		AddSystemToLevel<DamageSystem>( g_GameLevel );
		AddSystemToLevel<BulletSystem>( g_GameLevel );
		AddSystemToLevel<AIDBSystem>( g_GameLevel );
		AddSystemToLevel<StockEntityCreationSystem>( g_GameLevel );
		AddSystemToLevel<MLD62EntityCreationSystem>( g_GameLevel );
		
		g_GameLevel->GetSystem<FlareSystem>()->m_layers &= ~0x4;
		
		HelpTextSystem* HTS = g_GameLevel->GetSystem<HelpTextSystem>();
		HTS->renderer = &htr;
		htr.lineHeightFactor = 1.4f;
		htr.buttonTex = GR_GetTexture( "ui/key.png" );
		htr.centerPos = V2( GR_GetWidth() / 2, GR_GetHeight() * 3 / 4 );
		htr.fontSize = GR_GetHeight() / 20;
		htr.SetNamedFont( "", "core" );
		
		GR2D_SetFont( "core", TMIN(GR_GetWidth(),GR_GetHeight())/20 );
		g_GameLevel->Load( "test" );
		g_GameLevel->Tick( 0, 0 );
		
		Game_ShowCursor( false );
		
		Game_AddOverlayScreen( &g_StartScreen );
		
		return true;
	}
	void OnDestroy()
	{
		delete g_GameLevel;
		g_GameLevel = NULL;
		
		htr.buttonTex = NULL;
		
		g_SoundSys = NULL;
	}
	
	void OnEvent( const Event& e )
	{
		if( e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST )
		{
			m_lastFrameReset = false;
			Game_ShowCursor( true );
		}
		if( e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED )
		{
			if( Game_HasOverlayScreens() == false )
				Game_ShowCursor( false );
		}
		if( e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE )
		{
			if( Game_HasOverlayScreens() == false )
				;//Game_AddOverlayScreen( &g_PauseMenu );
		}
	}
	void Game_FixedTick( float dt )
	{
		g_GameLevel->FixedTick( dt );
	}
	void Game_Tick( float dt, float bf )
	{
		g_GameLevel->Tick( dt, bf );
	}
	void Game_Render()
	{
		BatchRenderer& br = GR2D_GetBatchRenderer();
		int W = GR_GetWidth();
		int H = GR_GetHeight();
		int minw = TMIN( W, H );
		
		htr.centerPos = V2( GR_GetWidth() / 2, GR_GetHeight() * 3 / 4 );
		htr.fontSize = GR_GetHeight() / 20;
		htr.buttonBorder = GR_GetHeight() / 80;
		
		g_GameLevel->Draw();
		g_GameLevel->Draw2D();
		
		if( g_GameLevel->m_player )
		{
			SGRX_CAST( MLD62Player*, player, g_GameLevel->m_player );
			if( player->Alive() == false )
			{
				float a = clamp( player->m_deadTimeout, 0, 1 );
				float s = 2 - smoothstep( a );
				br.Reset().Col( 1, a ).SetTexture( player->m_tex_dead_img )
				  .Box( W/2, H/2, minw*s/1, minw*s/2 ).Flush();
			}
		}
	}
	
	void OnTick( float dt, uint32_t gametime )
	{
		g_SoundSys->Update();
		
		if( g_GameLevel->m_player )
		{
			static_cast<MLD62Player*>(g_GameLevel->m_player)->inCursorMove = V2(0);
			if( Game_HasOverlayScreens() == false )
			{
				Vec2 cpos = Game_GetCursorPos();
				Game_SetCursorPos( GR_GetWidth() / 2, GR_GetHeight() / 2 );
				Vec2 opos = V2( GR_GetWidth() / 2, GR_GetHeight() / 2 );
				Vec2 curmove = cpos - opos;
				if( m_lastFrameReset )
					static_cast<MLD62Player*>(g_GameLevel->m_player)->inCursorMove = curmove * V2( g_i_mouse_invert_x ? -1 : 1, g_i_mouse_invert_y ? -1 : 1 ) * g_i_mouse_sensitivity;
				m_lastFrameReset = true;
			}
			else
				m_lastFrameReset = false;
		}
		
		if( dt > MAX_TICK_SIZE )
			dt = MAX_TICK_SIZE;
		
		m_accum += dt;
		while( m_accum >= 0 )
		{
			Game_FixedTick( FIXED_TICK_SIZE );
			m_accum -= FIXED_TICK_SIZE;
		}
		
		Game_Tick( dt, ( m_accum + FIXED_TICK_SIZE ) / FIXED_TICK_SIZE );
		
		Game_Render();
	}
	
	void OnDrawSceneGeom( SGRX_IRenderControl* ctrl, SGRX_RenderScene& info,
		SGRX_RTSpec rtt, DepthStencilSurfHandle dss )
	{
		IGame::OnDrawSceneGeom( ctrl, info, rtt, dss );
		
		if( g_GameLevel->m_player )
		{
			SGRX_CAST( MLD62Player*, player, g_GameLevel->m_player );
			
			BatchRenderer& br = GR2D_GetBatchRenderer();
			SGRX_Scene* scene = info.scene;
			int W = GR_GetWidth();
			int H = GR_GetHeight();
			float avgw = ( W + H ) * 0.5f;
			int minw = TMIN( W, H );
			
			DepthStencilSurfHandle dssFPV;
			TextureHandle rttGUI;
			dssFPV = GR_GetDepthStencilSurface( W, H, RT_FORMAT_COLOR_HDR16, 0xff00 );
			rttGUI = GR_GetRenderTarget( W, H, RT_FORMAT_COLOR_LDR8, 0xff01 );
			GR_PreserveResource( dssFPV );
			GR_PreserveResource( rttGUI );
			
			// GUI
			ctrl->SetRenderTargets( NULL, SGRX_RT_ClearColor, 0, 0, 1, rttGUI );
			if( player->Alive() )
			{
				br.Reset();
				GR2D_SetFont( "core", minw / 24 );
				StringView healthstr = "HEALTH: ||||||||||||||||||||";
				StringView curhltstr = healthstr.part( 0, sizeof("HEALTH: ")-1 + TMIN(player->m_health,100.0f) / 5 );
				br.Col( 0.1f, 1 ); GR2D_DrawTextLine( minw / 10, minw / 10, healthstr );
				br.Col( 1, 1 ); GR2D_DrawTextLine( minw / 10, minw / 10, curhltstr );
				TextureHandle crosshairTex = GR_GetTexture( "ui/crosshair.png" );
				GR_PreserveResource( crosshairTex );
				br.Reset().Col(0.5f,0.01f,0,1).SetTexture( crosshairTex ).Box( W/2, H/2, minw/18, minw/18 );
				br.Reset().SetTexture( crosshairTex ).Box( W/2, H/2, minw/20, minw/20 );
				br.Flush();
			}
			
			// WEAPON
			ctrl->SetRenderTargets( dssFPV, SGRX_RT_ClearDepth | SGRX_RT_ClearStencil, 0, 0, 1, rtt );
			if( info.viewport )
				GR2D_SetViewport( info.viewport->x0, info.viewport->y0, info.viewport->x1, info.viewport->y1 );
			
			SGRX_Camera cambk = scene->camera;
			scene->camera.zfar = 5;
			scene->camera.znear = 0.01f;
			scene->camera.UpdateMatrices();
			
			SGRX_MeshInstance* minsts[] = { player->m_weapon.m_cachedMeshInst };
			size_t micount = 1;
			
			ctrl->RenderMeshes( scene, 1, 1, SGRX_TY_Solid, minsts, micount );
			ctrl->RenderMeshes( scene, 3, 4, SGRX_TY_Solid, minsts, micount );
			ctrl->RenderMeshes( scene, 1, 1, SGRX_TY_Decal, minsts, micount );
			ctrl->RenderMeshes( scene, 3, 4, SGRX_TY_Decal, minsts, micount );
			ctrl->RenderMeshes( scene, 1, 1, SGRX_TY_Transparent, minsts, micount );
			ctrl->RenderMeshes( scene, 3, 4, SGRX_TY_Transparent, minsts, micount );
			
			// GUI overlay
			Mat4 backup = br.viewMatrix;
			Mat4 guimtx = Mat4::Identity;
			float turnQ = 0.1f;
			guimtx = guimtx * Mat4::CreateRotationX( -player->m_weaponTurn.pitch*turnQ );
			guimtx = guimtx * Mat4::CreateRotationY( player->m_weaponTurn.yaw*turnQ );
			guimtx = guimtx * scene->camera.mProj;
			br.Reset().Col( 0.5f, 0.01f, 0.0f, TMIN(player->m_hitTimeout,0.5f) ).Quad( 0, 0, W, H ).Flush();
			GR2D_SetViewMatrix( guimtx );
			br.Reset().SetTexture( rttGUI ).Quad( W/avgw, H/avgw, -W/avgw, -H/avgw, 1 ).Flush();
			GR2D_SetViewMatrix( backup );
			
			scene->camera = cambk;
		}
	}
	
	SGRX_HelpTextRenderer htr;
	float m_accum;
	bool m_lastFrameReset;
}
g_Game;


extern "C" EXPORT IGame* CreateGame()
{
	return &g_Game;
}

