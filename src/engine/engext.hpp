

#pragma once
#include "enganim.hpp"
#include "physics.hpp"



struct IF_GCC(ENGINE_EXPORT) AnimRagdoll : Animator
{
	struct Body
	{
		Vec3 relPos;
		Quat relRot;
		PhyRigidBodyHandle bodyHandle;
		Vec3 prevPos;
		Vec3 currPos;
		Quat prevRot;
		Quat currRot;
	};
	
	ENGINE_EXPORT AnimRagdoll();
	ENGINE_EXPORT void Initialize( PhyWorldHandle world, MeshHandle mesh, struct SkeletonInfo* skinfo );
	ENGINE_EXPORT virtual void Prepare( String* new_names, int count );
	ENGINE_EXPORT virtual void Advance( float deltaTime );
	
	ENGINE_EXPORT void SetBoneTransforms( int bone_id, const Vec3& prev_pos, const Vec3& curr_pos, const Quat& prev_rot, const Quat& curr_rot );
	ENGINE_EXPORT void AdvanceTransforms( Animator* anim );
	ENGINE_EXPORT void EnablePhysics( const Mat4& worldMatrix );
	ENGINE_EXPORT void DisablePhysics();
	
	bool m_enabled;
	float m_lastTickSize;
	MeshHandle m_mesh;
	Array< Body > m_bones;
	Array< PhyJointHandle > m_joints;
};



struct IF_GCC(ENGINE_EXPORT) AnimCharacter : IMeshRaycast
{
	enum BodyType
	{
		BodyType_None = 0,
		BodyType_Sphere = 1,
		BodyType_Capsule = 2,
		BodyType_Box = 3,
	};
	enum JointType
	{
		JointType_None = 0,
		JointType_Hinge = 1,
	};
	enum TransformType
	{
		TransformType_None = 0,
		TransformType_UndoParent = 1,
		TransformType_Move = 2,
		TransformType_Rotate = 3,
	};
	
	struct HitBox
	{
		Quat rotation;
		Vec3 position;
		Vec3 extents;
		float multiplier;
		
		HitBox() : rotation( Quat::Identity ), position( V3(0) ),
			extents( V3(0.1f) ), multiplier( 1 ){}
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch.marker( "HITBOX" );
			arch( rotation );
			arch( position );
			arch( extents );
			arch( multiplier );
		}
	};
	struct Body
	{
		Quat rotation;
		Vec3 position;
		uint8_t type; // BodyType
		Vec3 size; // x = radius, z = capsule height
		
		Body() : rotation( Quat::Identity ), position( V3(0) ),
			type( BodyType_None ), size( V3(0.1f) ){}
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch.marker( "BODY" );
			arch( type );
			arch( rotation );
			arch( position );
			arch( size );
		}
	};
	struct Joint
	{
		String parent_name;
		uint8_t type; // JointType
		Vec3 local_offset1;
		Vec3 local_offset2;
		
		Joint() : type( JointType_None ), local_offset1( V3(0) ),
			local_offset2( V3(0) ){}
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch.marker( "JOINT" );
			arch( type );
			arch( parent_name );
			arch( local_offset1 );
			arch( local_offset2 );
		}
	};
	
	struct BoneInfo
	{
		String name;
		HitBox hitbox;
		Body body;
		Joint joint;
		
		int bone_id;
		
		BoneInfo() : bone_id(-1){}
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch.marker( "BONEINFO" );
			arch( name );
			arch( hitbox );
			arch( body );
		//	arch( joint );
		}
	};
	
	struct Attachment
	{
		String name;
		String bone;
		Quat rotation;
		Vec3 position;
		
		int bone_id;
		
		Attachment() : rotation( Quat::Identity ), position( V3(0) ), bone_id(-1){}
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch.marker( "ATTACHMENT" );
			arch( name );
			arch( bone );
			arch( rotation );
			arch( position );
		}
	};
	
	struct LayerTransform
	{
		String bone;
		uint8_t type; // TransformType
		Vec3 posaxis; // offset for 'move', axis for 'rotate'
		float angle; // only for rotation
		
		int bone_id;
		
		LayerTransform() : type( TransformType_None ), posaxis( V3(0,0,1) ),
			angle( 0 ), bone_id(-1){}
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch.marker( "LAYERXF" );
			arch( type );
			arch( bone );
			arch( posaxis );
			arch( angle );
		}
	};
	struct Layer
	{
		String name;
		Array< LayerTransform > transforms;
		
		float amount;
		
		Layer() : amount( 0 ){}
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch.marker( "LAYER" );
			arch( name );
			arch( transforms );
		}
	};
	
	struct MaskCmd
	{
		String bone;
		float weight;
		bool children;
		
		MaskCmd() : weight( 0 ), children( true ){}
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch.marker( "MASKCMD" );
			arch( bone );
			arch( weight );
			arch( children );
		}
	};
	struct Mask
	{
		String name;
		Array< MaskCmd > cmds;
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch.marker( "MASK" );
			arch( name );
			arch( cmds );
		}
	};
	
	template< class T > void Serialize( T& basearch )
	{
		basearch.marker( "SGRXCHAR" );
		
		// 1: initial
		// 2: added masks
		SerializeVersionHelper<T> arch( basearch, 2 );
		
		arch( mesh );
		arch( bones );
		arch( attachments );
		arch( layers );
		arch( masks, arch.version >= 2 );
	}
	
	ENGINE_EXPORT AnimCharacter();
	
	ENGINE_EXPORT bool Load( const StringView& sv );
	ENGINE_EXPORT bool Save( const StringView& sv );
	
	ENGINE_EXPORT void OnRenderUpdate();
	ENGINE_EXPORT void AddToScene( SceneHandle sh );
	ENGINE_EXPORT void SetTransform( const Mat4& mtx );
	
	ENGINE_EXPORT void FixedTick( float deltaTime );
	ENGINE_EXPORT void PreRender( float blendFactor );
	ENGINE_EXPORT void RecalcLayerState();
	
	ENGINE_EXPORT int _FindBone( const StringView& name );
	ENGINE_EXPORT void RecalcBoneIDs();
	ENGINE_EXPORT bool GetBodyMatrix( int which, Mat4& outwm );
	ENGINE_EXPORT bool GetHitboxOBB( int which, Mat4& outwm, Vec3& outext );
	ENGINE_EXPORT bool GetAttachmentMatrix( int which, Mat4& outwm );
	ENGINE_EXPORT bool ApplyMask( const StringView& name, Animator* tgt );
	ENGINE_EXPORT int FindAttachment( const StringView& name );
	ENGINE_EXPORT void SortEnsureAttachments( const StringView* atchnames, int count );
	
	ENGINE_EXPORT void RaycastAll( const Vec3& from, const Vec3& to, struct SceneRaycastCallback* cb, struct SGRX_MeshInstance* cbmi = NULL );
	
	String mesh;
	Array< BoneInfo > bones;
	Array< Attachment > attachments;
	Array< Layer > layers;
	Array< Mask > masks;
	
	SceneHandle m_scene;
	MeshHandle m_cachedMesh;
	MeshInstHandle m_cachedMeshInst;
	Animator m_layerAnimator;
	AnimMixer m_anMixer;
	AnimInterp m_anEnd;
};



struct SkeletonInfo
{
	enum BodyType
	{
		BodyType_Capsule = 1
	};
	
	struct HitBox
	{
		String name;
		Quat rotation;
		Vec3 position;
		Vec3 xdir, ydir, zdir;
		Vec3 extents;
		float multiplier;
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch( name );
			arch( rotation );
			arch( position );
			arch( xdir ); arch( ydir ); arch( zdir );
			arch( extents );
			arch( multiplier );
		}
	};
	
	struct Body
	{
		String name;
		Quat rotation;
		Vec3 position;
		uint8_t type;
		float capsule_radius;
		float capsule_height;
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch( name );
			arch( rotation );
			arch( position );
			arch( type );
			if( type == BodyType_Capsule )
			{
				arch( capsule_radius );
				arch( capsule_height );
			}
		}
	};
	
	struct Joint
	{
		String name1;
		String name2;
		Vec3 local_offset1;
		Vec3 local_offset2;
		uint8_t type;
		
		template< class T > void Serialize( SerializeVersionHelper<T>& arch )
		{
			arch( name1 );
			arch( name2 );
			arch( local_offset1 );
			arch( local_offset2 );
			arch( type );
		}
	};
	
	Array< HitBox > hitboxes;
	Array< Body > bodies;
	Array< Joint > joints;
	
	template< class T > void Serialize( T& arch )
	{
		arch.marker( "SGRXSKRI" );
		SerializeVersionHelper<T> vh( arch, 1 );
		vh( hitboxes );
		vh( bodies );
		vh( joints );
	}
};



struct DecalMapPartInfo
{
	Vec4 bbox; // left, top, right, bottom / x0,y0,x1,y1
	Vec3 size; // width / height / depth
};

struct DecalProjectionInfo
{
//	DecalProjectionInfo() : pos(V3(0)), dir(V3(0,1,0)), up(V3(0,0,1)),
//		fovAngleDeg(90), orthoScale(1), aspectMult(1), aamix(0.5f),
//		distanceScale(1.0f), perspective(false)
//	{}
//	
	Vec3 pos;
	Vec3 dir;
	Vec3 up;
	float fovAngleDeg; // perspective only
	float orthoScale; // ortho only
	float aspectMult; // perspective only
	float aamix; // perspective only
	float distanceScale;
	float pushBack;
	bool perspective;
	DecalMapPartInfo decalInfo;
};

struct IF_GCC(ENGINE_EXPORT) SGRX_DecalSystem : SGRX_RefCounted
{
	ENGINE_EXPORT SGRX_DecalSystem();
	ENGINE_EXPORT ~SGRX_DecalSystem();
	
	ENGINE_EXPORT void Init( TextureHandle texDecal, TextureHandle texFalloff );
	ENGINE_EXPORT void Free();
	ENGINE_EXPORT void SetSize( uint32_t vbSize );
	
	ENGINE_EXPORT void Upload();
	
	ENGINE_EXPORT void AddDecal( const DecalProjectionInfo& projInfo, SGRX_IMesh* targetMesh, const Mat4& worldMatrix );
	ENGINE_EXPORT void AddDecal( const DecalProjectionInfo& projInfo, SGRX_IMesh* targetMesh, int partID, const Mat4& worldMatrix );
	ENGINE_EXPORT void ClearAllDecals();
	ENGINE_EXPORT void GenerateCamera( const DecalProjectionInfo& projInfo, SGRX_Camera* out );
	
	ENGINE_EXPORT void _ScaleDecalTexcoords( const DecalProjectionInfo& projInfo, size_t vbfrom );
	ENGINE_EXPORT void _GenDecalMatrix( const DecalProjectionInfo& projInfo, Mat4* outVPM, float* out_invzn2zf );
	
	SGRX_LightSampler* m_lightSampler;
	VertexDeclHandle m_vertexDecl;
	MaterialHandle m_material;
	MeshHandle m_mesh;
	ByteArray m_vertexData;
	UInt32Array m_indexData;
	Array< uint32_t > m_decals;
	uint32_t m_vbSize;
};



