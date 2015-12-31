

#pragma once
#include "engine.hpp"



ENGINE_EXPORT void GR_ClearFactors( Array< float >& out, float factor );
ENGINE_EXPORT void GR_SetFactors( Array< float >& out, const MeshHandle& mesh, const StringView& name, float factor, bool ch = true );
ENGINE_EXPORT void GR_FindBones( int* subbones, int& numsb, const MeshHandle& mesh, const StringView& name, bool ch );



struct IF_GCC(ENGINE_EXPORT) SGRX_Animation : SGRX_RCRsrc
{
	struct Marker
	{
		char name[ MAX_ANIM_MARKER_NAME_LENGTH ]; // engine.hpp
		int frame;
		
		StringView GetName()
		{
			return StringView( name, sgrx_snlen( name, MAX_ANIM_MARKER_NAME_LENGTH ) );
		}
	};
	
	ENGINE_EXPORT ~SGRX_Animation();
	
	ENGINE_EXPORT Vec3* GetPosition( int track );
	ENGINE_EXPORT Quat* GetRotation( int track );
	ENGINE_EXPORT Vec3* GetScale( int track );
	
	ENGINE_EXPORT void GetState( int track, float framePos, Vec3& outpos, Quat& outrot, Vec3& outscl );
	ENGINE_EXPORT bool CheckMarker( const StringView& name, float fp0, float fp1 );
	
	String name;
	int frameCount;
	float speed;
	size_t nameSize;
	Array< float > data;
	Array< String > trackNames;
	Array< Marker > markers;
};

struct AnimHandle : Handle< SGRX_Animation >
{
	AnimHandle() : Handle(){}
	AnimHandle( const AnimHandle& h ) : Handle( h ){}
	AnimHandle( SGRX_Animation* anm ) : Handle( anm ){}
};

// Track-track mapping for animators to find the matching track indices
typedef HashTable< AnimHandle, int* > AnimCache;

struct AnimInfo
{
	Mat4 rootXF;
};

struct IF_GCC(ENGINE_EXPORT) Animator
{
	ENGINE_EXPORT virtual bool Prepare( const MeshHandle& mesh );
	virtual void Advance( float deltaTime, AnimInfo* info ){}
	
	void ClearFactors( float f ){ GR_ClearFactors( m_factors, f ); }
	void SetFactors( const MeshHandle& mesh, const StringView& name, float f, bool ch = true )
	{
		GR_SetFactors( m_factors, m_mesh, name, f, ch );
	}
	int GetParentBoneID( int i ) const
	{
		if( i < 0 || i >= m_mesh->m_numBones )
			return -1;
		return m_mesh->m_bones[ i ].parent_id;
	}
	ENGINE_EXPORT virtual Array< float >& GetBlendFactorArray();
	
	MeshHandle m_mesh;
	Array< Vec3 > m_positions;
	Array< Quat > m_rotations;
	Array< Vec3 > m_scales;
	Array< float > m_factors;
};

struct IF_GCC(ENGINE_EXPORT) AnimMixer : Animator
{
	enum TransformFlags
	{
		TF_None = 0,
		TF_Absolute_Rot = 0x01,
		TF_Absolute_Pos = 0x02,
		TF_Absolute_Scale = 0x04,
		TF_Absolute_All = (TF_Absolute_Rot|TF_Absolute_Pos|TF_Absolute_Scale),
		TF_Additive = 0x10,
		TF_IgnoreMeshXF = 0x20,
	};
	
	struct Layer
	{
		Layer() : anim(NULL), factor(1), tflags(0){}
		
		Animator* anim;
		float factor;
		int tflags;
	};
	
	ENGINE_EXPORT AnimMixer();
	ENGINE_EXPORT ~AnimMixer();
	ENGINE_EXPORT virtual bool Prepare( const MeshHandle& mesh );
	ENGINE_EXPORT virtual void Advance( float deltaTime, AnimInfo* info );
	
	Array< Mat4 > m_staging;
	
	Layer* layers; // INTERFACE
	int layerCount; // INTERFACE
};

struct IF_GCC(ENGINE_EXPORT) AnimPlayer : Animator
{
	struct Anim
	{
		AnimHandle anim;
		int* trackIDs;
		float at;
		float fade_at;
		float prev_fade_at;
		float fadetime;
		bool once;
	};
	
	ENGINE_EXPORT AnimPlayer();
	ENGINE_EXPORT ~AnimPlayer();
	ENGINE_EXPORT virtual bool Prepare( const MeshHandle& mesh );
	ENGINE_EXPORT virtual void Advance( float deltaTime, AnimInfo* info );
	
	ENGINE_EXPORT void Play( const AnimHandle& anim, bool once = false, float fadetime = 0.5f );
	ENGINE_EXPORT bool CheckMarker( const StringView& name );
	
	void ClearBlendFactors( float f ){ GR_ClearFactors( m_blendFactors, f ); }
	void SetBlendFactors( const StringView& name, float f, bool ch = true )
	{
		GR_SetFactors( m_blendFactors, m_mesh, name, f, ch );
	}
	ENGINE_EXPORT virtual Array< float >& GetBlendFactorArray();
	
	ENGINE_EXPORT int* _getTrackIds( const AnimHandle& anim );
	ENGINE_EXPORT void _clearAnimCache();
	
	AnimCache m_animCache;
	Array< Anim > m_currentAnims;
	Array< float > m_blendFactors;
};

struct IF_GCC(ENGINE_EXPORT) AnimInterp : Animator
{
	ENGINE_EXPORT AnimInterp();
	ENGINE_EXPORT virtual bool Prepare( const MeshHandle& mesh );
	ENGINE_EXPORT virtual void Advance( float deltaTime, AnimInfo* info );
	ENGINE_EXPORT void Transfer();
	ENGINE_EXPORT void Interpolate( float deltaTime );
	
	Array< Vec3 > m_prev_positions;
	Array< Quat > m_prev_rotations;
	Array< Vec3 > m_prev_scales;
	
	Animator* animSource; // INTERFACE
};

struct IF_GCC(ENGINE_EXPORT) AnimDeformer : Animator
{
	struct Force
	{
		int boneID; // -1 for world
		Vec3 pos; // in bone/world space
		Vec3 dir; // normalized in bone/world space
		float amount; // force = dir * amount
		float radius; // distf = 1 - min( 1, dist / radius )
		float power; // pow( distf, ->power<- )
		float lifetime; // how long since it's been created
	};
	
	ENGINE_EXPORT AnimDeformer();
	ENGINE_EXPORT virtual bool Prepare( const MeshHandle& mesh );
	ENGINE_EXPORT virtual void Advance( float deltaTime, AnimInfo* info );
	
	ENGINE_EXPORT void AddLocalForce( const Vec3& pos, const Vec3& dir,
		float rad, float power = 1, float amount = 0.0f );
	ENGINE_EXPORT void AddModelForce( const Vec3& pos, const Vec3& dir,
		float rad, float power = 1, float amount = 0.0f );
	ENGINE_EXPORT int _FindClosestBone( const Vec3& pos );
	ENGINE_EXPORT void _UpdatePoseInfo();
	
	Array< Mat4 > m_skinOffsets;
	Array< Mat4 > m_invSkinOffsets;
	Array< Vec3 > m_bonePositions;
	
	// INTERFACE
	Array< Force > forces;
	Animator* animSource;
	int numIterations;
	int numConstraintIterations;
};



ENGINE_EXPORT int GR_LoadAnims( const StringView& path, const StringView& prefix = StringView() );
ENGINE_EXPORT AnimHandle GR_GetAnim( const StringView& name );
ENGINE_EXPORT bool GR_ApplyAnimator( const Animator* animator, Mat4* out, size_t outsz, bool applyinv = true, Mat4* base = NULL );
ENGINE_EXPORT bool GR_ApplyAnimator( const Animator* animator, SGRX_MeshInstance* meshinst );



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

#define PARTICLESYSTEM_VERSION 5
// 1: initial version
// 2: added group count
// 3: added global scale
// 4: added gravity multiplier
// 5: added intersection attribs [limit, friction, bounce, fx, remove]
// 6: added intersection FX chance

#define PARTICLE_VDECL "pf3cf40b4"
#define NUM_PARTICLE_TEXTURES 4

struct IF_GCC(ENGINE_EXPORT) ParticleSystem : SGRX_RefCounted
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
	
	ParticleSystem() :
		gravity(V3(0,0,-10)), maxGroupCount(10), globalScale(1),
		looping(true), retriggerTimeExt(V2(1,0.1f)),
		m_isPlaying(false), m_retriggerTime(0), m_nextGroup(0),
		m_transform(Mat4::Identity), m_lightSampler(NULL), m_psRaycast(NULL)
	{}
	
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
	
	ENGINE_EXPORT void OnRenderUpdate();
	ENGINE_EXPORT void AddToScene( SceneHandle sh );
	ENGINE_EXPORT void SetTransform( const Mat4& mtx );
	
	ENGINE_EXPORT bool Tick( float dt );
	ENGINE_EXPORT void PreRender();
	
	ENGINE_EXPORT void Trigger();
	ENGINE_EXPORT void Play();
	ENGINE_EXPORT void Stop();
};
typedef Handle< ParticleSystem > PartSysHandle;



