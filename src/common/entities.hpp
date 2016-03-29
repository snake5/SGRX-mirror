

#pragma once
#include <engine.hpp>
#include <enganim.hpp>
#include <pathfinding.hpp>
#include <sound.hpp>
#include <physics.hpp>
#include <script.hpp>

#include "level.hpp"
#include "systems.hpp"


enum CoreEntityEventIDs
{
	EID_Type_CoreEntity = 10000,
	EID_MeshUpdate,
	EID_LightUpdate,
};


EXP_STRUCT ParticleFX : Entity
{
	SGS_OBJECT_INHERIT( Entity );
	ENT_SGS_IMPLEMENT;
	
	GFW_EXPORT ParticleFX( GameLevel* lev );
	GFW_EXPORT virtual void OnTransformUpdate();
	GFW_EXPORT virtual void EditorDrawWorld();
	GFW_EXPORT virtual void Tick( float deltaTime, float blendFactor );
	GFW_EXPORT virtual void PreRender();
	
	GFW_EXPORT void sgsSetParticleSystem( StringView path );
	GFW_EXPORT void sgsSetSoundEvent( StringView name );
	GFW_EXPORT void _StartSoundEvent();
	GFW_EXPORT void sgsSetPlaying( bool playing );
	
	SGS_PROPERTY_FUNC( READ WRITE sgsSetParticleSystem VARNAME particleSystemPath ) String m_partSysPath;
	SGS_PROPERTY_FUNC( READ WRITE sgsSetSoundEvent VARNAME soundEvent ) String m_soundEventName;
	SGS_PROPERTY_FUNC( READ WRITE sgsSetPlaying VARNAME enabled ) bool m_enabled;
	
	GFW_EXPORT SGS_METHOD void Trigger();
	
	ParticleSystem m_psys;
	bool m_soundEventOneShot;
	SoundEventInstanceHandle m_soundEventInst;
};


EXP_STRUCT MeshEntity : Entity
{
	SGS_OBJECT_INHERIT( Entity );
	ENT_SGS_IMPLEMENT;
	
	GFW_EXPORT MeshEntity( GameLevel* lev );
	GFW_EXPORT ~MeshEntity();
	GFW_EXPORT virtual void OnTransformUpdate();
	GFW_EXPORT virtual void EditorDrawWorld();
	GFW_EXPORT void _UpdateLighting();
	GFW_EXPORT void _UpdateBody();
	
	FINLINE void _UpEv(){ Game_FireEvent( EID_MeshUpdate, this ); }
	
	bool IsStatic() const { return m_isStatic; }
	void SetStatic( bool v ){ if( m_isStatic != v ){ m_body = NULL; } m_isStatic = v; _UpdateBody(); }
	bool IsVisible() const { return m_isVisible; }
	void SetVisible( bool v ){ m_isVisible = v; m_meshInst->enabled = v; _UpEv(); }
	MeshHandle GetMeshData() const { return m_mesh; }
	GFW_EXPORT void SetMeshData( MeshHandle mesh );
	StringView GetMeshPath() const { return m_mesh ? m_mesh->m_key : ""; }
	void SetMeshPath( StringView path ){ SetMeshData( GR_GetMesh( path ) ); }
	bool IsSolid() const { return m_isSolid; }
	void SetSolid( bool v ){ m_isSolid = v; _UpdateBody(); }
	bool GetLightingMode() const { return m_lightingMode; }
	void SetLightingMode( int v ){ m_lightingMode = v;
		m_meshInst->SetLightingMode( (SGRX_LightingMode) v ); _UpdateLighting(); }
	
	SGS_PROPERTY_FUNC( READ SOURCE m_meshInst.item ) SGS_ALIAS( void* meshInst );
	SGS_PROPERTY_FUNC( READ IsStatic WRITE SetStatic VARNAME isStatic ) bool m_isStatic;
	SGS_PROPERTY_FUNC( READ IsVisible WRITE SetVisible VARNAME visible ) bool m_isVisible;
	SGS_PROPERTY_FUNC( READ GetMeshData WRITE SetMeshData VARNAME meshData ) MeshHandle m_mesh;
	SGS_PROPERTY_FUNC( READ GetMeshPath WRITE SetMeshPath ) SGS_ALIAS( StringView mesh );
	SGS_PROPERTY_FUNC( READ IsSolid WRITE SetSolid VARNAME solid ) bool m_isSolid;
	SGS_PROPERTY_FUNC( READ GetLightingMode WRITE SetLightingMode VARNAME lightingMode ) int m_lightingMode;
	// editor-only static mesh parameters
	SGS_PROPERTY_FUNC( READ WRITE WRITE_CALLBACK _UpEv VARNAME lmQuality ) float m_lmQuality;
	SGS_PROPERTY_FUNC( READ WRITE WRITE_CALLBACK _UpEv VARNAME castLMS ) bool m_castLMS;
	
	GFW_EXPORT SGS_METHOD void SetShaderConst( int v, Vec4 var );
	
	MeshInstHandle m_meshInst;
	PhyShapeHandle m_phyShape;
	PhyRigidBodyHandle m_body;
	uint32_t m_edLGCID;
};


EXP_STRUCT LightEntity : Entity
{
	SGS_OBJECT_INHERIT( Entity );
	ENT_SGS_IMPLEMENT;
	
	GFW_EXPORT LightEntity( GameLevel* lev );
	GFW_EXPORT ~LightEntity();
	virtual void OnTransformUpdate()
	{
		if( m_light )
		{
			m_light->SetTransform( GetWorldMatrix() );
			m_light->UpdateTransform();
		}
		_UpEv();
	}
	
	GFW_EXPORT void _UpdateLight();
	GFW_EXPORT void _UpdateShadows();
	GFW_EXPORT void _UpdateFlare();
	
	FINLINE void _UpEv(){ Game_FireEvent( EID_LightUpdate, this ); }
	
#define RETNIFNOLIGHT _UpEv(); if( !m_light ) return;
	bool IsStatic() const { return m_isStatic; }
	void SetStatic( bool v ){ m_isStatic = v; _UpEv(); _UpdateLight(); }
	int GetType() const { return m_type; }
	void SetType( int v ){ m_type = v; RETNIFNOLIGHT; m_light->type = v; _UpdateShadows(); }
	bool IsEnabled() const { return m_isEnabled; }
	void SetEnabled( bool v ){ m_isEnabled = v; _UpdateFlare(); RETNIFNOLIGHT; m_light->enabled = v; }
	Vec3 GetColor() const { return m_color; }
	void SetColor( Vec3 v ){ m_color = v; RETNIFNOLIGHT; m_light->color = v * m_intensity; }
	float GetIntensity() const { return m_intensity; }
	void SetIntensity( float v ){ m_intensity = v; RETNIFNOLIGHT; m_light->color = m_color * v; }
	float GetRange() const { return m_range; }
	void SetRange( float v ){ m_range = v; RETNIFNOLIGHT; m_light->range = v; m_light->UpdateTransform(); }
	float GetPower() const { return m_power; }
	void SetPower( float v ){ m_power = v; RETNIFNOLIGHT; m_light->power = v; }
	float GetAngle() const { return m_angle; }
	void SetAngle( float v ){ m_angle = v; RETNIFNOLIGHT; m_light->angle = v; m_light->UpdateTransform(); }
	float GetAspect() const { return m_aspect; }
	void SetAspect( float v ){ m_aspect = v; RETNIFNOLIGHT; m_light->aspect = v; m_light->UpdateTransform(); }
	bool HasShadows() const { return m_hasShadows; }
	void SetShadows( bool v ){ m_hasShadows = v; RETNIFNOLIGHT; m_light->hasShadows = v; _UpdateShadows(); }
	float GetFlareSize() const { return m_flareSize; }
	void SetFlareSize( float v ){ m_flareSize = v; _UpdateFlare(); }
	Vec3 GetFlareOffset() const { return m_flareOffset; }
	void SetFlareOffset( Vec3 v ){ m_flareOffset = v; _UpdateFlare(); }
	TextureHandle GetCookieTextureData() const { return m_cookieTexture; }
	void SetCookieTextureData( TextureHandle h ){ m_cookieTexture = h; RETNIFNOLIGHT; m_light->cookieTexture = h; }
	StringView GetCookieTexturePath() const { return m_cookieTexture ? m_cookieTexture->m_key : ""; }
	void SetCookieTexturePath( StringView path ){ SetCookieTextureData( GR_GetTexture( path ) ); }
	
	SGS_PROPERTY_FUNC( READ IsStatic WRITE SetStatic VARNAME isStatic ) bool m_isStatic;
	SGS_PROPERTY_FUNC( READ GetType WRITE SetType VARNAME type ) int m_type;
	SGS_PROPERTY_FUNC( READ IsEnabled WRITE SetEnabled VARNAME enabled ) bool m_isEnabled;
	SGS_PROPERTY_FUNC( READ GetColor WRITE SetColor VARNAME color ) Vec3 m_color;
	SGS_PROPERTY_FUNC( READ GetIntensity WRITE SetIntensity VARNAME intensity ) float m_intensity;
	SGS_PROPERTY_FUNC( READ GetRange WRITE SetRange VARNAME range ) float m_range;
	SGS_PROPERTY_FUNC( READ GetPower WRITE SetPower VARNAME power ) float m_power;
	SGS_PROPERTY_FUNC( READ GetAngle WRITE SetAngle VARNAME angle ) float m_angle;
	SGS_PROPERTY_FUNC( READ GetAspect WRITE SetAspect VARNAME aspect ) float m_aspect;
	SGS_PROPERTY_FUNC( READ HasShadows WRITE SetShadows VARNAME hasShadows ) bool m_hasShadows;
	SGS_PROPERTY_FUNC( READ GetCookieTextureData WRITE SetCookieTextureData VARNAME cookieTextureData ) TextureHandle m_cookieTexture;
	SGS_PROPERTY_FUNC( READ GetCookieTexturePath WRITE SetCookieTexturePath ) SGS_ALIAS( StringView cookieTexture );
	SGS_PROPERTY_FUNC( READ GetFlareSize WRITE SetFlareSize VARNAME flareSize ) float m_flareSize;
	SGS_PROPERTY_FUNC( READ GetFlareOffset WRITE SetFlareOffset VARNAME flareOffset ) Vec3 m_flareOffset;
	// editor-only static light parameters
	SGS_PROPERTY_FUNC( READ WRITE WRITE_CALLBACK _UpEv VARNAME innerAngle ) float m_innerAngle;
	SGS_PROPERTY_FUNC( READ WRITE WRITE_CALLBACK _UpEv VARNAME spotCurve ) float m_spotCurve;
	SGS_PROPERTY_FUNC( READ WRITE WRITE_CALLBACK _UpEv VARNAME lightRadius ) float m_lightRadius;
	
	LightHandle m_light;
	uint32_t m_edLGCID;
};


#define ShapeType_AABB 0
#define ShapeType_Box 1
#define ShapeType_Sphere 2
#define ShapeType_Cylinder 3
#define ShapeType_Capsule 4
#define ShapeType_Mesh 5

EXP_STRUCT RigidBodyEntity : Entity
{
	SGS_OBJECT_INHERIT( Entity );
	ENT_SGS_IMPLEMENT;
	
	GFW_EXPORT RigidBodyEntity( GameLevel* lev );
	GFW_EXPORT void FixedTick( float deltaTime );
	GFW_EXPORT void Tick( float deltaTime, float blendFactor );
	virtual void OnTransformUpdate()
	{
		m_body->SetPosition( GetWorldPosition() );
		m_body->SetRotation( GetWorldRotation() );
		m_shape->SetScale( GetWorldScale() );
	}
	GFW_EXPORT void _UpdateShape();
	
	FINLINE Vec3 GetLinearVelocity() const { return m_body->GetLinearVelocity(); }
	FINLINE void SetLinearVelocity( Vec3 v ){ m_body->SetLinearVelocity( v ); }
	FINLINE Vec3 GetAngularVelocity() const { return m_body->GetAngularVelocity(); }
	FINLINE void SetAngularVelocity( Vec3 v ){ m_body->SetAngularVelocity( v ); }
	FINLINE float GetFriction() const { return m_body->GetFriction(); }
	FINLINE void SetFriction( float v ){ m_body->SetFriction( v ); }
	FINLINE float GetRestitution() const { return m_body->GetRestitution(); }
	FINLINE void SetRestitution( float v ){ m_body->SetRestitution( v ); }
	FINLINE float GetMass() const { return m_body->GetMass(); }
	FINLINE void SetMass( float v ){ m_body->SetMassAndInertia( v, m_body->GetInertia() ); }
	FINLINE Vec3 GetInertia() const { return m_body->GetInertia(); }
	FINLINE void SetInertia( Vec3 v ){ m_body->SetMassAndInertia( m_body->GetMass(), v ); }
	FINLINE float GetLinearDamping() const { return m_body->GetLinearDamping(); }
	FINLINE void SetLinearDamping( float v ){ m_body->SetLinearDamping( v ); }
	FINLINE float GetAngularDamping() const { return m_body->GetAngularDamping(); }
	FINLINE void SetAngularDamping( float v ){ m_body->SetAngularDamping( v ); }
	FINLINE bool IsKinematic() const { return m_body->IsKinematic(); }
	FINLINE void SetKinematic( bool v ){ m_body->SetKinematic( v ); }
	FINLINE bool CanSleep() const { return m_body->CanSleep(); }
	FINLINE void SetCanSleep( bool v ){ m_body->SetCanSleep( v ); }
	FINLINE bool IsEnabled() const { return m_body->GetEnabled(); }
	FINLINE void SetEnabled( bool v ){ m_body->SetEnabled( v ); }
	FINLINE uint16_t GetGroup() const { return m_body->GetGroup(); }
	FINLINE void SetGroup( uint16_t v ){ m_body->SetGroupAndMask( v, GetMask() ); }
	FINLINE uint16_t GetMask() const { return m_body->GetMask(); }
	FINLINE void SetMask( uint16_t v ){ m_body->SetGroupAndMask( GetGroup(), v ); }
	
	void SetShapeType( int type ){ if( type == shapeType ) return; shapeType = type; _UpdateShape(); }
	void SetShapeRadius( float radius ){ if( radius == shapeRadius ) return; shapeRadius = radius;
		if( shapeType == ShapeType_Sphere || shapeType == ShapeType_Capsule ){ _UpdateShape(); } }
	void SetShapeHeight( float height ){ if( height == shapeHeight ) return; shapeHeight = height;
		if( shapeType == ShapeType_Capsule ){ _UpdateShape(); } }
	void SetShapeExtents( Vec3 extents ){ if( extents == shapeExtents ) return; shapeExtents = extents;
		if( shapeType == ShapeType_Box || shapeType == ShapeType_Cylinder || shapeType == ShapeType_AABB ) _UpdateShape(); }
	void SetShapeMinExtents( Vec3 minExtents ){ if( minExtents == shapeMinExtents ) return; shapeMinExtents = minExtents;
		if( shapeType == ShapeType_AABB ){ _UpdateShape(); } }
	void SetShapeMesh( MeshHandle mesh ){ if( mesh == shapeMesh ) return; shapeMesh = mesh;
		if( shapeType == ShapeType_Mesh ){ _UpdateShape(); } }
	
	SGS_PROPERTY_FUNC( READ GetLinearVelocity WRITE SetLinearVelocity ) SGS_ALIAS( Vec3 linearVelocity );
	SGS_PROPERTY_FUNC( READ GetAngularVelocity WRITE SetAngularVelocity ) SGS_ALIAS( Vec3 angularVelocity );
	SGS_PROPERTY_FUNC( READ GetFriction WRITE SetFriction ) SGS_ALIAS( float friction );
	SGS_PROPERTY_FUNC( READ GetRestitution WRITE SetRestitution ) SGS_ALIAS( float restitution );
	SGS_PROPERTY_FUNC( READ GetMass WRITE SetMass ) SGS_ALIAS( float mass );
	SGS_PROPERTY_FUNC( READ GetInertia WRITE SetInertia ) SGS_ALIAS( Vec3 inertia );
	SGS_PROPERTY_FUNC( READ GetLinearDamping WRITE SetLinearDamping ) SGS_ALIAS( float linearDamping );
	SGS_PROPERTY_FUNC( READ GetAngularDamping WRITE SetAngularDamping ) SGS_ALIAS( float angularDamping );
	SGS_PROPERTY_FUNC( READ IsKinematic WRITE SetKinematic ) SGS_ALIAS( float kinematic );
	SGS_PROPERTY_FUNC( READ CanSleep WRITE SetCanSleep ) SGS_ALIAS( float canSleep );
	SGS_PROPERTY_FUNC( READ IsEnabled WRITE SetEnabled ) SGS_ALIAS( float enabled );
	SGS_PROPERTY_FUNC( READ GetGroup WRITE SetGroup ) SGS_ALIAS( float group );
	SGS_PROPERTY_FUNC( READ GetMask WRITE SetMask ) SGS_ALIAS( float mask );
	
	SGS_PROPERTY_FUNC( READ WRITE SetShapeType ) int shapeType;
	SGS_PROPERTY_FUNC( READ WRITE SetShapeRadius ) float shapeRadius;
	SGS_PROPERTY_FUNC( READ WRITE SetShapeHeight ) float shapeHeight;
	SGS_PROPERTY_FUNC( READ WRITE SetShapeExtents ) Vec3 shapeExtents;
	SGS_PROPERTY_FUNC( READ WRITE SetShapeMinExtents ) Vec3 shapeMinExtents;
	SGS_PROPERTY_FUNC( READ WRITE SetShapeMesh ) MeshHandle shapeMesh;
	
	PhyRigidBodyHandle m_body;
	PhyShapeHandle m_shape;
};


EXP_STRUCT ReflectionPlaneEntity : Entity
{
	SGS_OBJECT_INHERIT( Entity );
	ENT_SGS_IMPLEMENT;
	
	GFW_EXPORT ReflectionPlaneEntity( GameLevel* lev );
};



struct SGRX_RigidBodyInfo : SGRX_PhyRigidBodyInfo
{
	typedef sgsHandle< SGRX_RigidBodyInfo > Handle;
	
	SGS_OBJECT;
	
	SGS_PROPERTY SGS_ALIAS( Vec3 position );
	SGS_PROPERTY SGS_ALIAS( Quat rotation );
	SGS_PROPERTY SGS_ALIAS( float friction );
	SGS_PROPERTY SGS_ALIAS( float restitution );
	SGS_PROPERTY SGS_ALIAS( float mass );
	SGS_PROPERTY SGS_ALIAS( Vec3 inertia );
	SGS_PROPERTY SGS_ALIAS( float linearDamping );
	SGS_PROPERTY SGS_ALIAS( float angularDamping );
	SGS_PROPERTY SGS_ALIAS( Vec3 linearFactor );
	SGS_PROPERTY SGS_ALIAS( Vec3 angularFactor );
	SGS_PROPERTY SGS_ALIAS( bool kinematic );
	SGS_PROPERTY SGS_ALIAS( bool canSleep );
	SGS_PROPERTY SGS_ALIAS( bool enabled );
	SGS_PROPERTY SGS_ALIAS( uint16_t group );
	SGS_PROPERTY SGS_ALIAS( uint16_t mask );
};

struct SGRX_HingeJointInfo : SGRX_PhyHingeJointInfo
{
	typedef sgsHandle< SGRX_HingeJointInfo > Handle;
	
	SGS_OBJECT;
	
	SGS_PROPERTY SGS_ALIAS( Vec3 pivotA );
	SGS_PROPERTY SGS_ALIAS( Vec3 pivotB );
	SGS_PROPERTY SGS_ALIAS( Vec3 axisA );
	SGS_PROPERTY SGS_ALIAS( Vec3 axisB );
};

struct SGRX_ConeTwistJointInfo : SGRX_PhyConeTwistJointInfo
{
	typedef sgsHandle< SGRX_ConeTwistJointInfo > Handle;
	
	SGS_OBJECT;
	
	SGS_PROPERTY SGS_ALIAS( Mat4 frameA );
	SGS_PROPERTY SGS_ALIAS( Mat4 frameB );
};


#define ForceType_Velocity PFT_Velocity
#define ForceType_Impulse PFT_Impulse
#define ForceType_Acceleration PFT_Acceleration
#define ForceType_Force PFT_Force


#define MULTIENT_NUM_SLOTS 4
#define MULTIENT_RANGE_STR "[0-3]"

EXP_STRUCT MultiEntity : Entity, SGRX_MeshInstUserData
{
	SGS_OBJECT_INHERIT( Entity ) SGS_NO_DESTRUCT;
	ENT_SGS_IMPLEMENT;
	
	GFW_EXPORT MultiEntity( GameLevel* lev );
	GFW_EXPORT ~MultiEntity();
	GFW_EXPORT virtual void FixedTick( float deltaTime );
	GFW_EXPORT virtual void Tick( float deltaTime, float blendFactor );
	GFW_EXPORT void PreRender();
	
	GFW_EXPORT virtual void OnEvent( SGRX_MeshInstance* MI, uint32_t evid, void* data );
	GFW_EXPORT virtual void OnTransformUpdate();
	
	// - mesh instance
	GFW_EXPORT SGS_METHOD void MICreate( int i, StringView path );
	GFW_EXPORT SGS_METHOD void MIDestroy( int i );
	GFW_EXPORT SGS_METHOD bool MIExists( int i );
	GFW_EXPORT SGS_METHOD void MISetMesh( int i, StringView path );
	GFW_EXPORT SGS_METHOD void MISetEnabled( int i, bool enabled );
	GFW_EXPORT SGS_METHOD void MISetMatrix( int i, Mat4 mtx );
	GFW_EXPORT SGS_METHOD void MISetShaderConst( int i, int v, Vec4 var );
	GFW_EXPORT SGS_METHOD void MISetLayers( int i, uint32_t layers );
	
	GFW_EXPORT MeshHandle sgsGetMI0Mesh();
	GFW_EXPORT void sgsSetMI0Mesh( MeshHandle m );
	SGS_PROPERTY_FUNC( READ sgsGetMI0Mesh WRITE sgsSetMI0Mesh ) SGS_ALIAS( MeshHandle mi0mesh );
	SGS_PROPERTY Vec3 mi0sampleOffset;
	
	// - particle system
	GFW_EXPORT SGS_METHOD void PSCreate( int i, StringView path );
	GFW_EXPORT SGS_METHOD void PSDestroy( int i );
	GFW_EXPORT SGS_METHOD bool PSExists( int i );
	GFW_EXPORT SGS_METHOD void PSLoad( int i, StringView path );
	GFW_EXPORT SGS_METHOD void PSSetMatrix( int i, Mat4 mtx );
	GFW_EXPORT SGS_METHOD void PSSetMatrixFromMeshAABB( int i, int mi );
	GFW_EXPORT SGS_METHOD void PSPlay( int i );
	GFW_EXPORT SGS_METHOD void PSStop( int i );
	GFW_EXPORT SGS_METHOD void PSTrigger( int i );
	
	// - decal system
	GFW_EXPORT SGS_METHOD void DSCreate( StringView texDmgDecalPath,
		StringView texOvrDecalPath, StringView texFalloffPath, uint32_t size );
	GFW_EXPORT SGS_METHOD void DSDestroy();
	GFW_EXPORT SGS_METHOD void DSResize( uint32_t size );
	GFW_EXPORT SGS_METHOD void DSClear();
	
	// - rigid bodies
	GFW_EXPORT SGS_METHOD void RBCreateFromMesh( int i, int mi, SGRX_RigidBodyInfo* spec );
	GFW_EXPORT SGS_METHOD void RBCreateFromConvexPointSet( int i, StringView cpset, SGRX_RigidBodyInfo* spec );
	GFW_EXPORT SGS_METHOD void RBDestroy( int i );
	GFW_EXPORT SGS_METHOD bool RBExists( int i );
	GFW_EXPORT SGS_METHOD void RBSetEnabled( int i, bool enabled );
	GFW_EXPORT SGS_METHOD Vec3 RBGetPosition( int i );
	GFW_EXPORT SGS_METHOD void RBSetPosition( int i, Vec3 v );
	GFW_EXPORT SGS_METHOD Quat RBGetRotation( int i );
	GFW_EXPORT SGS_METHOD void RBSetRotation( int i, Quat v );
	GFW_EXPORT SGS_METHOD Mat4 RBGetMatrix( int i );
	GFW_EXPORT SGS_METHOD void RBApplyForce( int i, int type, Vec3 v, /*opt*/ Vec3 p );
	
	// - joints
	GFW_EXPORT SGS_METHOD void JTCreateHingeB2W( int i, int bi, SGRX_HingeJointInfo* spec );
	GFW_EXPORT SGS_METHOD void JTCreateHingeB2B( int i, int biA, int biB, SGRX_HingeJointInfo* spec );
	GFW_EXPORT SGS_METHOD void JTCreateConeTwistB2W( int i, int bi, SGRX_ConeTwistJointInfo* spec );
	GFW_EXPORT SGS_METHOD void JTCreateConeTwistB2B( int i, int biA, int biB, SGRX_ConeTwistJointInfo* spec );
	GFW_EXPORT SGS_METHOD void JTDestroy( int i );
	GFW_EXPORT SGS_METHOD bool JTExists( int i );
	GFW_EXPORT SGS_METHOD void JTSetEnabled( int i, bool enabled );
	// ---
	
	DecalSysHandle m_dmgDecalSys;
	DecalSysHandle m_ovrDecalSys;
	
	MeshInstHandle m_meshes[ MULTIENT_NUM_SLOTS ];
	PartSysHandle m_partSys[ MULTIENT_NUM_SLOTS ];
	PhyRigidBodyHandle m_bodies[ MULTIENT_NUM_SLOTS ];
	PhyJointHandle m_joints[ MULTIENT_NUM_SLOTS ];
	IVState< Vec3 > m_bodyPos[ MULTIENT_NUM_SLOTS ];
	IVState< Quat > m_bodyRot[ MULTIENT_NUM_SLOTS ];
	Vec3 m_bodyPosLerp[ MULTIENT_NUM_SLOTS ];
	Quat m_bodyRotLerp[ MULTIENT_NUM_SLOTS ];
//	LightHandle m_lights[ MULTIENT_NUM_SLOTS ];
	
	Mat4 m_meshMatrices[ MULTIENT_NUM_SLOTS ];
	Mat4 m_partSysMatrices[ MULTIENT_NUM_SLOTS ];
//	Mat4 m_lightMatrices[ MULTIENT_NUM_SLOTS ];
};


struct StockEntityCreationSystem : IGameLevelSystem
{
	enum { e_system_uid = 999 };
	GFW_EXPORT StockEntityCreationSystem( GameLevel* lev );
	GFW_EXPORT virtual Entity* AddEntity( StringView type );
};




#if 0
// max_clip - clip capacity
// max_bag - ammo bag capacity
// bullet_speed - units per second
// bullet_time - bullet hang time
// damage, damage_rand - total damage on hit = ( damage + frandrange(-1,1) * damage_rand ) * bspeed
// fire_rate - timeout between bullets
// reload_time - time it takes to reload
// spread - bullet spread in degrees
// backfire - spread addition factor for firing time and movement speed
// -- full spread = spread + f(time,speed) * backfire
struct WeaponType
{
	StringView name;
	int max_clip_ammo;
	int max_bag_ammo;
	float bullet_speed;
	float bullet_time;
	float damage;
	float damage_rand;
	float fire_rate;
	float reload_time;
	float spread;
	float backfire;
	StringView sound_shoot;
	StringView sound_reload;
};

#define WEAPON_INACTIVE 0
#define WEAPON_EMPTY -1
#define WEAPON_SHOT 1

struct Weapon
{
	Weapon( BulletSystem* bsys, WeaponType* wt, int ammo );
	
	void SetShooting( bool shoot );
	float GetSpread();
	bool BeginReload();
	void StopReload();
	int Tick( float deltaTime, Vec2 pos, Vec2 dir, float speed );
	
	BulletSystem* m_bulletSystem;
	WeaponType* m_type;
	// TODO VARIABLE FOR WHO USES THIS WEAPON
	float m_fire_timeout;
	float m_reload_timeout;
	int m_ammo_clip;
	int m_ammo_bag;
	bool m_shooting;
	float m_shoot_factor;
	// TODO VARIABLE FOR SHOOT SOUND
	// TODO VARIABLE FOR RELOAD SOUND
	Vec2 m_position;
	Vec2 m_direction;
	float m_owner_speed;
};
#endif


