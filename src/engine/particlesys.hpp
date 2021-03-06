

#pragma once
#include "engine.hpp"



struct IF_GCC(ENGINE_EXPORT) SGRX_IPSRaycast
{
	struct DATA_IN
	{
		Vec3 p1;
		Vec3 p2;
		uint32_t isect_fx;
	};
	struct DATA_OUT
	{
		Vec3 normal;
		float factor;
	};
	
	ENGINE_EXPORT virtual void Raycast( DATA_IN* rays, DATA_OUT* isects, size_t count ) = 0;
};

struct IF_GCC(ENGINE_EXPORT) SGRX_ScenePSRaycast : SGRX_IPSRaycast
{
	ENGINE_EXPORT SGRX_ScenePSRaycast();
	ENGINE_EXPORT virtual void Raycast( DATA_IN* rays, DATA_OUT* isects, size_t count );
	ENGINE_EXPORT virtual void DoFX( const Vec3& pos, const Vec3& nrm, uint32_t fx ); // empty
	
	SceneHandle scene;
	uint32_t layers;
};

#define PARTICLESYSTEM_VERSION 6
// 1: initial version
// 2: added group count
// 3: added global scale
// 4: added gravity multiplier
// 5: added intersection attribs [limit, friction, bounce, fx, remove]
// 6: added intersection FX chance

#define PARTICLE_VDECL "pf3cf40b4"
#define NUM_PARTICLE_TEXTURES 4

struct IF_GCC(ENGINE_EXPORT) ParticleSystem : SGRX_RefCounted, SGRX_IEventHandler
{
	struct Vertex
	{
		Vec3 pos;
		Vec4 color;
		uint8_t u;
		uint8_t v;
		uint16_t pad;
	};
	
	struct ps_prerender_info
	{
		SceneHandle scene;
		Mat4 transform;
		Mat4 viewProj;
		Vec3 basis[3];
	};
	
	struct Group
	{
		Vec3 color;
	};
	
	struct Emitter
	{
		struct Curve
		{
			Curve() : randomValDvg(V2(0)), valueRange(V2(0)){}
			
			Array< Vec2 > values;
			Vec2 randomValDvg;
			Vec2 valueRange;
			
			template< class T > void Serialize( T& arch )
			{
				arch << values;
				arch << randomValDvg;
				arch << valueRange;
			}
			
			FINLINE float GetValue( float t, float r )
			{
				if( values.size() == 0 )
					return valueRange.x;
				size_t end = values.size() - 1;
				if( t < 0 ) t = 0;
				size_t i = t * end;
				if( i >= end ) i = end;
				size_t i1 = i + 1;
				if( i1 >= end ) i1 = end;
				float q = t * end - i;
				Vec2 interp = TLERP( values[ i ], values[ i1 ], q );
				return TLERP( valueRange.x, valueRange.y, interp.x + interp.y * r );
			}
		};
		
		Array< Vec3 > particles_Position;
		Array< Vec3 > particles_Velocity;
		Array< Vec2 > particles_Lifetime; // 0-1, increment
		Array< Vec3 > particles_RandSizeAngVel;
		Array< Vec4 > particles_RandColor; // HSV, opacity
		Array< uint16_t > particles_Group;
		
		Curve curve_Size;
		Curve curve_ColorHue;
		Curve curve_ColorSat;
		Curve curve_ColorVal;
		Curve curve_Opacity;
		
		int spawn_MaxCount;
		int spawn_Count;
		int spawn_CountExt;
		Vec2 spawn_TimeExt;
		
		Vec3 create_Pos;
		Vec3 create_PosBox;
		float create_PosRadius;
		Vec3 create_VelMicroDir;
		float create_VelMicroDvg;
		Vec2 create_VelMicroDistExt;
		Vec3 create_VelMacroDir;
		float create_VelMacroDvg;
		Vec2 create_VelMacroDistExt;
		int create_VelCluster;
		int create_VelClusterExt;
		Vec2 create_LifetimeExt;
		Vec2 create_AngleDirDvg;
		Vec2 create_AngleVelDvg;
		
		float tick_AngleAcc;
		float tick_GravityMult;
		bool absolute;
		
		int isect_Limit;
		float isect_Friction;
		float isect_Bounce;
		uint32_t isect_FXID;
		float isect_FXChance;
		bool isect_Remove;
		
		TextureHandle render_Textures[ NUM_PARTICLE_TEXTURES ];
		String render_Shader;
		bool render_Additive;
		bool render_Stretch;
		
		int state_SpawnTotalCount;
		int state_SpawnCurrCount;
		float state_SpawnTotalTime;
		float state_SpawnCurrTime;
		float state_lastDelta;
		
		Emitter() :
			spawn_MaxCount(100), spawn_Count(10), spawn_CountExt(2), spawn_TimeExt(V2(1,0.2f)),
			create_Pos(V3(0)), create_PosBox(V3(0)), create_PosRadius(0),
			create_VelMicroDir(V3(0,0,1)), create_VelMicroDvg(0), create_VelMicroDistExt(V2(0)),
			create_VelMacroDir(V3(0,0,1)), create_VelMacroDvg(0), create_VelMacroDistExt(V2(1,0.1f)),
			create_VelCluster(1), create_VelClusterExt(0),
			create_LifetimeExt(V2(3,0.1f)), create_AngleDirDvg(V2(0,(float)M_PI)), create_AngleVelDvg(V2(0)),
			tick_AngleAcc(0), tick_GravityMult(1), absolute(false),
			isect_Limit(0), isect_Friction(0), isect_Bounce(1), isect_FXID(0), isect_FXChance(1), isect_Remove(false),
			render_Shader("particle"), render_Additive(false), render_Stretch(false),
			state_SpawnTotalCount(0), state_SpawnCurrCount(0), state_SpawnTotalTime(0), state_SpawnCurrTime(0),
			state_lastDelta(0)
		{}
		
		template< class T > void Serialize( T& arch, bool incl_state )
		{
			arch.marker( "EMITTER" );
			
			arch << curve_Size;
			arch << curve_ColorHue;
			arch << curve_ColorSat;
			arch << curve_ColorVal;
			arch << curve_Opacity;
			
			arch << spawn_MaxCount;
			arch << spawn_Count;
			arch << spawn_CountExt;
			arch << spawn_TimeExt;
			
			arch << create_Pos;
			arch << create_PosBox;
			arch << create_PosRadius;
			arch << create_VelMicroDir;
			arch << create_VelMicroDvg;
			arch << create_VelMicroDistExt;
			arch << create_VelMacroDir;
			arch << create_VelMacroDvg;
			arch << create_VelMacroDistExt;
			arch << create_VelCluster;
			arch << create_VelClusterExt;
			arch << create_LifetimeExt;
			arch << create_AngleDirDvg;
			arch << create_AngleVelDvg;
			
			arch << tick_AngleAcc;
			arch( tick_GravityMult, arch.version >= 4, 1 );
			arch << absolute;
			
			arch( isect_Limit, arch.version >= 5, 0 );
			arch( isect_Friction, arch.version >= 5, 0.0f );
			arch( isect_Bounce, arch.version >= 5, 1.0f );
			arch( isect_FXID, arch.version >= 5, 0 );
			arch( isect_FXChance, arch.version >= 6, 1.0f );
			arch( isect_Remove, arch.version >= 5, false );
			
			for( int i = 0; i < NUM_PARTICLE_TEXTURES; ++i )
			{
				if( T::IsReader )
				{
					bool hastex = false;
					arch << hastex;
					if( hastex )
					{
						String texname;
						arch << texname;
						render_Textures[ i ] = GR_GetTexture( texname );
					}
				}
				else
				{
					bool hastex = !!render_Textures[ i ];
					arch << hastex;
					if( hastex )
						arch << render_Textures[ i ]->m_key;
				}
			}
			arch << render_Shader;
			arch << render_Additive;
			arch << render_Stretch;
			
			if( incl_state )
			{
				arch << state_SpawnTotalCount;
				arch << state_SpawnCurrCount;
				arch << state_SpawnTotalTime;
				arch << state_SpawnCurrTime;
				arch << state_lastDelta;
				
				arch << particles_Position;
				arch << particles_Velocity;
				arch << particles_Lifetime;
				arch << particles_RandSizeAngVel;
				arch << particles_RandColor;
			}
			else if( T::IsReader )
			{
				// clear state on read data
				state_SpawnTotalCount = 0;
				state_SpawnCurrCount = 0;
				state_SpawnTotalTime = 0;
				state_SpawnCurrTime = 0;
				state_lastDelta = 0;
				
				particles_Position.clear();
				particles_Velocity.clear();
				particles_Lifetime.clear();
				particles_RandSizeAngVel.clear();
				particles_RandColor.clear();
				particles_Group.clear();
			}
		}
		
		void Tick( ParticleSystem* PS, float dt );
		void Generate( ParticleSystem* PS, int count );
		void Trigger( ParticleSystem* PS );
		
		void PreRender( ParticleSystem* PS, ps_prerender_info& info );
	};
	
	Array< Emitter > emitters;
	Vec3 gravity;
	uint16_t maxGroupCount;
	float globalScale;
	bool looping;
	Vec2 retriggerTimeExt;
	
	bool m_isPlaying;
	float m_retriggerTime;
	uint16_t m_nextGroup;
	Mat4 m_transform;
	SGRX_LightSampler* m_lightSampler;
	SGRX_IPSRaycast* m_psRaycast;
	
	Array< Group > m_groups;
	Array< Vertex > m_vertices;
	Array< uint16_t > m_indices;
	SceneHandle m_scene;
	VertexDeclHandle m_vdecl;
	
	Array< MeshInstHandle > m_meshInsts;
	
	ParticleSystem();
	
	template< class T > void Serialize( T& arch, bool incl_state )
	{
		arch.marker( "SGRX_PARTSYS" );
		
		SerializeVersionHelper<T> svh( arch, PARTICLESYSTEM_VERSION );
		
		uint32_t emcnt = 0;
		if( T::IsReader )
		{
			svh << emcnt;
			emitters.resize( emcnt );
		}
		else
		{
			emcnt = emitters.size();
			svh << emcnt;
		}
		for( uint32_t i = 0; i < emcnt; ++i )
		{
			emitters[ i ].Serialize( svh, incl_state );
		}
		
		svh << gravity;
		svh( maxGroupCount, svh.version >= 2, 10 );
		svh( globalScale, svh.version >= 3, 1 );
		svh << looping;
		svh << retriggerTimeExt;
		
		if( incl_state )
		{
			svh << m_isPlaying;
			svh << m_retriggerTime;
			svh << m_transform;
		}
		else if( T::IsReader )
		{
			m_isPlaying = false;
			m_retriggerTime = 0;
			m_transform = Mat4::Identity;
		}
		
		if( T::IsReader )
			OnRenderUpdate();
	}
	ENGINE_EXPORT bool Load( const StringView& sv );
	ENGINE_EXPORT bool Save( const StringView& sv );
	
	ENGINE_EXPORT void HandleEvent( SGRX_EventID eid, const EventData& edata );
	
	ENGINE_EXPORT void OnRenderUpdate();
	ENGINE_EXPORT void AddToScene( SceneHandle sh );
	ENGINE_EXPORT void SetTransform( const Mat4& mtx );
	
	ENGINE_EXPORT bool Tick( float dt );
	ENGINE_EXPORT void _PreRender();
	
	ENGINE_EXPORT void Trigger();
	ENGINE_EXPORT void Play();
	ENGINE_EXPORT void Stop();
};
typedef Handle< ParticleSystem > PartSysHandle;


