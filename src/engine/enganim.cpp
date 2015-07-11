

#define USE_VEC4
#define USE_QUAT
#define USE_MAT4
#define USE_ARRAY
#define USE_HASHTABLE
#include "enganim.hpp"
#include "physics.hpp"


SGRX_Animation::~SGRX_Animation()
{
}

Vec3* SGRX_Animation::GetPosition( int track )
{
	return (Vec3*) &data[ track * 10 * frameCount ];
}

Quat* SGRX_Animation::GetRotation( int track )
{
	return (Quat*) &data[ track * 10 * frameCount + 3 * frameCount ];
}

Vec3* SGRX_Animation::GetScale( int track )
{
	return (Vec3*) &data[ track * 10 * frameCount + 7 * frameCount ];
}

void SGRX_Animation::GetState( int track, float framePos, Vec3& outpos, Quat& outrot, Vec3& outscl )
{
	Vec3* pos = GetPosition( track );
	Quat* rot = GetRotation( track );
	Vec3* scl = GetScale( track );
	
	if( framePos < 0 )
		framePos = 0;
	else if( framePos > frameCount )
		framePos = frameCount;
	
	int f0 = floor( framePos );
	int f1 = f0 + 1;
	if( f1 >= frameCount )
		f1 = f0;
	float q = framePos - f0;
	
	outpos = TLERP( pos[ f0 ], pos[ f1 ], q );
	outrot = TLERP( rot[ f0 ], rot[ f1 ], q );
	outscl = TLERP( scl[ f0 ], scl[ f1 ], q );
}

void Animator::Prepare( String* new_names, int count )
{
	names.assign( new_names, count );
	position.clear();
	rotation.clear();
	scale.clear();
	factor.clear();
	position.resize( count );
	rotation.resize( count );
	scale.resize( count );
	factor.resize( count );
	for( int i = 0; i < count; ++i )
	{
		position[ i ] = V3(0);
		rotation[ i ] = Quat::Identity;
		scale[ i ] = V3(1);
		factor[ i ] = 0;
	}
}

bool Animator::PrepareForMesh( const MeshHandle& mesh )
{
	SGRX_IMesh* M = mesh;
	if( !M )
		return false;
	Array< String > bonenames;
	bonenames.resize( M->m_numBones );
	for( int i = 0; i < M->m_numBones; ++i )
		bonenames[ i ] = M->m_bones[ i ].name;
	Prepare( bonenames.data(), bonenames.size() );
	return true;
}

Array< float >& Animator::GetBlendFactorArray()
{
	return factor;
}


AnimMixer::AnimMixer() : layers(NULL), layerCount(0)
{
}

AnimMixer::~AnimMixer()
{
}

void AnimMixer::Prepare( String* new_names, int count )
{
	Animator::Prepare( new_names, count );
	m_staging.resize( count );
	for( int i = 0; i < layerCount; ++i )
	{
		layers[ i ].anim->Prepare( new_names, count );
	}
}

void AnimMixer::Advance( float deltaTime )
{
	// generate output
	for( size_t i = 0; i < names.size(); ++i )
	{
		position[ i ] = V3(0);
		rotation[ i ] = Quat::Identity;
		scale[ i ] = V3(1);
		factor[ i ] = 0;
	}
	
	for( int layer = 0; layer < layerCount; ++layer )
	{
		Animator* AN = layers[ layer ].anim;
		AN->Advance( deltaTime );
		
		int tflags = layers[ layer ].tflags;
		bool abslayer = ( tflags & TF_Absolute_All ) && mesh && mesh->m_numBones == (int) names.size();
		SGRX_MeshBone* MB = mesh->m_bones;
		
		for( size_t i = 0; i < names.size(); ++i )
		{
			Vec3 P = AN->position[ i ];
			Quat R = AN->rotation[ i ];
			Vec3 S = AN->scale[ i ];
			float q = AN->factor[ i ] * layers[ layer ].factor;
			
			if( abslayer )
			{
				// to matrix
				Mat4 tfm = Mat4::CreateSRT( S, R, P );
				
				// extract diff
				Mat4 orig = MB[ i ].boneOffset;
				if( MB[ i ].parent_id >= 0 )
				{
					orig = orig * m_staging[ MB[ i ].parent_id ];
				}
				Mat4 inv = Mat4::Identity;
				orig.InvertTo( inv );
				Mat4 diff = tflags & TF_Additive ?
					Mat4::CreateSRT( scale[ i ], rotation[ i ], position[ i ] ) * orig * tfm * inv :
					tfm * inv;
		//		if( q )
		//			LOG << i << ":\t" << R;
			//	if( q )
			//		LOG << i << ":\t" << diff.GetRotationQuaternion() << rotation[i];
				
				// convert back to SRP
				P = ( tflags & TF_Absolute_Pos ) ? diff.GetTranslation() : position[ i ];
				R = ( tflags & TF_Absolute_Rot ) ? diff.GetRotationQuaternion() : rotation[ i ];
				S = ( tflags & TF_Absolute_Scale ) ? diff.GetScale() : scale[ i ];
				
				m_staging[ i ] = Mat4::CreateSRT( scale[ i ], rotation[ i ], position[ i ] ) * orig;
			}
			
			if( !factor[ i ] )
			{
				position[ i ] = P;
				rotation[ i ] = R;
				scale[ i ] = S;
				factor[ i ] = q;
			}
			else
			{
				position[ i ] = TLERP( position[ i ], P, q );
				rotation[ i ] = TLERP( rotation[ i ], R, q );
				scale[ i ] = TLERP( scale[ i ], S, q );
				factor[ i ] = TLERP( factor[ i ], 1.0f, q );
			}
		}
	}
}

AnimPlayer::AnimPlayer()
{
}

AnimPlayer::~AnimPlayer()
{
	_clearAnimCache();
}

void AnimPlayer::Prepare( String* new_names, int count )
{
	currentAnims.clear();
	_clearAnimCache();
	Animator::Prepare( new_names, count );
	blendFactor.resize( count );
	for( int i = 0; i < count; ++i )
		blendFactor[ i ] = 1;
}

void AnimPlayer::Advance( float deltaTime )
{
	// process tracks
	for( size_t i = currentAnims.size(); i > 0; )
	{
		--i;
		
		Anim& A = currentAnims[ i ];
		SGRX_Animation* AN = A.anim;
		A.at += deltaTime;
		A.fade_at += deltaTime;
		
		float animTime = AN->frameCount / AN->speed;
		if( !A.once )
		{
			A.at = fmodf( A.at, animTime );
			// permanent animation faded fully in, no need to keep previous tracks
			if( A.fade_at > A.fadetime )
			{
				currentAnims.erase( 0, i );
				break;
			}
		}
		// temporary animation has finished playback
		else if( A.at >= animTime )
		{
			currentAnims.erase( i );
			continue;
		}
	}
	
	// generate output
	for( size_t i = 0; i < names.size(); ++i )
	{
		position[ i ] = V3(0);
		rotation[ i ] = Quat::Identity;
		scale[ i ] = V3(1);
		factor[ i ] = 0;
	}
	for( size_t an = 0; an < currentAnims.size(); ++an )
	{
		Anim& A = currentAnims[ an ];
		SGRX_Animation* AN = A.anim;
		
		Vec3 P = V3(0), S = V3(1);
		Quat R = Quat::Identity;
		for( size_t i = 0; i < names.size(); ++i )
		{
			int tid = A.trackIDs[ i ];
			if( tid < 0 )
				continue;
			
			AN->GetState( tid, A.at * AN->speed, P, R, S );
			float animTime = AN->frameCount / AN->speed;
			float q = A.once ?
				smoothlerp_range( A.fade_at, 0, A.fadetime, animTime - A.fadetime, animTime ) :
				smoothlerp_oneway( A.fade_at, 0, A.fadetime );
			if( !factor[ i ] )
			{
				position[ i ] = P;
				rotation[ i ] = R;
				scale[ i ] = S;
				factor[ i ] = q;
			}
			else
			{
				position[ i ] = TLERP( position[ i ], P, q );
				rotation[ i ] = TLERP( rotation[ i ], R, q );
				scale[ i ] = TLERP( scale[ i ], S, q );
				factor[ i ] = TLERP( factor[ i ], 1.0f, q );
			}
		}
	}
	for( size_t i = 0; i < names.size(); ++i )
		factor[ i ] *= blendFactor[ i ];
}

void AnimPlayer::Play( const AnimHandle& anim, bool once, float fadetime )
{
	if( !anim )
		return;
	if( !once && currentAnims.size() && currentAnims.last().once == false && currentAnims.last().anim == anim )
		return; // ignore repetitive invocations
	Anim A = { anim, _getTrackIds( anim ), 0, 0, fadetime, once };
	currentAnims.push_back( A );
}

int* AnimPlayer::_getTrackIds( const AnimHandle& anim )
{
	if( !names.size() )
		return NULL;
	int* ids = animCache.getcopy( anim );
	if( !ids )
	{
		ids = new int [ names.size() ];
		for( size_t i = 0; i < names.size(); ++i )
		{
			ids[ i ] = anim->trackNames.find_first_at( names[ i ] );
		}
		animCache.set( anim, ids );
	}
	return ids;
}

void AnimPlayer::_clearAnimCache()
{
	for( size_t i = 0; i < animCache.size(); ++i )
	{
		delete [] animCache.item( i ).value;
	}
	animCache.clear();
}

Array< float >& AnimPlayer::GetBlendFactorArray()
{
	return blendFactor;
}


AnimInterp::AnimInterp() : animSource( NULL )
{
}

void AnimInterp::Prepare( String* new_names, int count )
{
	Animator::Prepare( new_names, count );
	prev_position.resize( count );
	prev_rotation.resize( count );
	prev_scale.resize( count );
	if( animSource )
		animSource->Prepare( new_names, count );
}

void AnimInterp::Advance( float deltaTime )
{
	Transfer();
	animSource->Advance( deltaTime );
}

void AnimInterp::Transfer()
{
	for( size_t i = 0; i < names.size(); ++i )
	{
		prev_position[ i ] = animSource->position[ i ];
		prev_rotation[ i ] = animSource->rotation[ i ];
		prev_scale[ i ] = animSource->scale[ i ];
	}
}

void AnimInterp::Interpolate( float deltaTime )
{
	for( size_t i = 0; i < names.size(); ++i )
	{
		position[ i ] = TLERP( prev_position[ i ], animSource->position[ i ], deltaTime );
		rotation[ i ] = TLERP( prev_rotation[ i ], animSource->rotation[ i ], deltaTime );
		scale[ i ] = TLERP( prev_scale[ i ], animSource->scale[ i ], deltaTime );
	}
}


void GR_ClearFactors( Array< float >& out, float factor )
{
	TMEMSET( out.data(), out.size(), factor );
}

void GR_SetFactors( Array< float >& out, const MeshHandle& mesh, const StringView& name, float factor, bool ch )
{
	int subbones[ MAX_MESH_BONES ];
	int numsb = 0;
	GR_FindBones( subbones, numsb, mesh, name, ch );
	for( int i = 0; i < numsb; ++i )
	{
		out[ subbones[ i ] ] = factor;
	}
}

void GR_FindBones( int* subbones, int& numsb, const MeshHandle& mesh, const StringView& name, bool ch )
{
	if( !mesh )
		return;
	
	int numbones = mesh->m_numBones;
	SGRX_MeshBone* mbones = mesh->m_bones;
	int b = 0;
	for( ; b < numbones; ++b )
	{
		if( mbones[ b ].name == name )
			break;
	}
	if( b >= numbones )
		return;
	
	int sbstart = numsb;
	subbones[ numsb++ ] = b;
	if( ch )
	{
		for( ; b < numbones; ++b )
		{
			for( int sb = sbstart; sb < numsb; ++sb )
			{
				if( subbones[ sb ] == mbones[ b ].parent_id )
				{
					subbones[ numsb++ ] = b;
					break;
				}
			}
		}
	}
}


bool GR_ApplyAnimator( const Animator* animator, MeshHandle mh, Mat4* out, size_t outsz, bool applyinv, Mat4* base )
{
	SGRX_IMesh* mesh = mh;
	if( !mesh )
		return false;
	if( outsz != animator->position.size() )
		return false;
	if( outsz != (size_t) mesh->m_numBones )
		return false;
	SGRX_MeshBone* MB = mesh->m_bones;
	
	for( size_t i = 0; i < outsz; ++i )
	{
		Mat4& M = out[ i ];
		M = Mat4::CreateSRT( animator->scale[ i ], animator->rotation[ i ], animator->position[ i ] ) * MB[ i ].boneOffset;
		if( MB[ i ].parent_id >= 0 )
			M = M * out[ MB[ i ].parent_id ];
		else if( base )
			M = M * *base;
	}
	if( applyinv )
	{
		for( size_t i = 0; i < outsz; ++i )
		{
			Mat4& M = out[ i ];
			M = MB[ i ].invSkinOffset * M;
		}
	}
	return true;
}

bool GR_ApplyAnimator( const Animator* animator, MeshInstHandle mih )
{
	if( !mih )
		return false;
	return GR_ApplyAnimator( animator, mih->mesh, mih->skin_matrices.data(), mih->skin_matrices.size() );
}



AnimCharacter::AnimCharacter()
{
	m_anEnd.animSource = &m_anMixer;
}

bool AnimCharacter::Load( const StringView& sv )
{
	ByteArray ba;
	if( !FS_LoadBinaryFile( sv, ba ) )
		return false;
	ByteReader br( &ba );
	Serialize( br );
	if( br.error )
		return false;
	
	if( m_scene )
		OnRenderUpdate();
	else
	{
		m_cachedMesh = GR_GetMesh( mesh );
		RecalcBoneIDs();
	}
	return true;
}

bool AnimCharacter::Save( const StringView& sv )
{
	ByteArray ba;
	ByteWriter bw( &ba );
	Serialize( bw );
	return FS_SaveBinaryFile( sv, ba.data(), ba.size() );
}

void AnimCharacter::OnRenderUpdate()
{
	if( m_scene == NULL )
		return;
	
	if( m_cachedMeshInst == NULL )
	{
		m_cachedMeshInst = m_scene->CreateMeshInstance();
	}
	m_cachedMesh = GR_GetMesh( mesh );
	m_cachedMeshInst->mesh = m_cachedMesh;
	m_cachedMeshInst->skin_matrices.resize( m_cachedMesh ? m_cachedMesh->m_numBones : 0 );
	RecalcBoneIDs();
	m_anMixer.mesh = m_cachedMesh;
	m_anEnd.PrepareForMesh( m_cachedMesh );
	if( m_cachedMesh && (int) m_layerAnimator.names.size() != m_cachedMesh->m_numBones )
		m_layerAnimator.PrepareForMesh( m_cachedMesh );
	m_layerAnimator.ClearFactors( 1.0f );
}

void AnimCharacter::AddToScene( SceneHandle sh )
{
	m_scene = sh;
	
	OnRenderUpdate();
}

void AnimCharacter::SetTransform( const Mat4& mtx )
{
	if( m_cachedMeshInst )
		m_cachedMeshInst->matrix = mtx;
}

void AnimCharacter::FixedTick( float deltaTime )
{
	m_anEnd.Advance( deltaTime );
}

void AnimCharacter::PreRender( float blendFactor )
{
	m_anEnd.Interpolate( blendFactor );
	GR_ApplyAnimator( &m_anEnd, m_cachedMeshInst );
}

void AnimCharacter::RecalcLayerState()
{
	if( m_cachedMesh == NULL )
		return;
	
	TMEMSET( m_layerAnimator.position.data(), m_layerAnimator.position.size(), V3(0) );
	TMEMSET( m_layerAnimator.rotation.data(), m_layerAnimator.rotation.size(), Quat::Identity );
	for( size_t i = 0; i < layers.size(); ++i )
	{
		Layer& L = layers[ i ];
		for( size_t j = 0; j < L.transforms.size(); ++j )
		{
			LayerTransform& LT = L.transforms[ j ];
			if( LT.bone_id < 0 )
				continue;
			switch( LT.type )
			{
			case TransformType_UndoParent:
				{
					int parent_id = m_cachedMesh->m_bones[ LT.bone_id ].parent_id;
					if( parent_id >= 0 )
					{
						m_layerAnimator.position[ LT.bone_id ] -= m_layerAnimator.position[ parent_id ];
						m_layerAnimator.rotation[ LT.bone_id ] =
							m_layerAnimator.rotation[ LT.bone_id ] * m_layerAnimator.rotation[ parent_id ].Inverted();
					}
				}
				break;
			case TransformType_Move:
				m_layerAnimator.position[ LT.bone_id ] += LT.posaxis * L.amount;
				break;
			case TransformType_Rotate:
				m_layerAnimator.rotation[ LT.bone_id ] = m_layerAnimator.rotation[ LT.bone_id ]
					* Quat::CreateAxisAngle( LT.posaxis.Normalized(), DEG2RAD( LT.angle ) * L.amount );
				break;
			}
		}
	}
}

int AnimCharacter::_FindBone( const StringView& name )
{
	if( !m_cachedMesh )
		return -1;
	int bid = 0;
	for( ; bid < m_cachedMesh->m_numBones; ++bid )
	{
		if( m_cachedMesh->m_bones[ bid ].name == name )
			break;
	}
	return bid < m_cachedMesh->m_numBones ? bid : -1;
}

void AnimCharacter::RecalcBoneIDs()
{
	for( size_t i = 0; i < bones.size(); ++i )
	{
		BoneInfo& BI = bones[ i ];
		BI.bone_id = _FindBone( BI.name );
	}
	for( size_t i = 0; i < attachments.size(); ++i )
	{
		Attachment& AT = attachments[ i ];
		AT.bone_id = _FindBone( AT.bone );
	}
	for( size_t i = 0; i < layers.size(); ++i )
	{
		Layer& LY = layers[ i ];
		for( size_t j = 0; j < LY.transforms.size(); ++j )
		{
			LayerTransform& LT = LY.transforms[ j ];
			LT.bone_id = _FindBone( LT.bone );
		}
	}
}

bool AnimCharacter::GetBodyMatrix( int which, Mat4& outwm )
{
	if( !m_cachedMesh || !m_cachedMeshInst )
		return false;
	if( which < 0 || which >= (int) bones.size() )
		return false;
	BoneInfo& BI = bones[ which ];
	
	outwm = m_cachedMeshInst->matrix;
	if( BI.bone_id >= 0 )
	{
		if( m_cachedMeshInst->skin_matrices.size() )
		{
			outwm = m_cachedMeshInst->skin_matrices[ BI.bone_id ] * outwm;
		}
		outwm = m_cachedMesh->m_bones[ BI.bone_id ].skinOffset * outwm;
	}
	outwm = Mat4::CreateRotationFromQuat( BI.body.rotation ) *
		Mat4::CreateTranslation( BI.body.position ) * outwm;
	return true;
}

bool AnimCharacter::GetHitboxOBB( int which, Mat4& outwm, Vec3& outext )
{
	if( !m_cachedMesh || !m_cachedMeshInst )
		return false;
	if( which < 0 || which >= (int) bones.size() )
		return false;
	BoneInfo& BI = bones[ which ];
	if( BI.hitbox.multiplier == 0 )
		return false; // a way to disable it
	
	outwm = m_cachedMeshInst->matrix;
	if( BI.bone_id >= 0 )
	{
		if( m_cachedMeshInst->skin_matrices.size() )
		{
			outwm = m_cachedMeshInst->skin_matrices[ BI.bone_id ] * outwm;
		}
		outwm = m_cachedMesh->m_bones[ BI.bone_id ].skinOffset * outwm;
	}
	outwm = Mat4::CreateRotationFromQuat( BI.hitbox.rotation ) *
		Mat4::CreateTranslation( BI.hitbox.position ) * outwm;
	outext = BI.hitbox.extents;
	return true;
}

bool AnimCharacter::GetAttachmentMatrix( int which, Mat4& outwm )
{
	if( !m_cachedMesh || !m_cachedMeshInst )
		return false;
	if( which < 0 || which >= (int) attachments.size() )
		return false;
	Attachment& AT = attachments[ which ];
	
	outwm = m_cachedMeshInst->matrix;
	if( AT.bone_id >= 0 )
	{
		if( m_cachedMeshInst->skin_matrices.size() )
		{
			outwm = m_cachedMeshInst->skin_matrices[ AT.bone_id ] * outwm;
		}
		outwm = m_cachedMesh->m_bones[ AT.bone_id ].skinOffset * outwm;
	}
	outwm = Mat4::CreateRotationFromQuat( AT.rotation ) *
		Mat4::CreateTranslation( AT.position ) * outwm;
	return true;
}

bool AnimCharacter::ApplyMask( const StringView& name, Animator* tgt )
{
	if( !m_cachedMesh )
		return false;
	
	for( size_t i = 0; i < masks.size(); ++i )
	{
		Mask& M = masks[ i ];
		if( M.name != name )
			continue;
		
		Array< float >& factors = tgt->GetBlendFactorArray();
		GR_ClearFactors( factors, 0 );
		for( size_t j = 0; j < M.cmds.size(); ++j )
		{
			MaskCmd& MC = M.cmds[ j ];
			GR_SetFactors( factors, m_cachedMesh, MC.bone, MC.weight, MC.children );
		}
		return true;
	}
	return false;
}

int AnimCharacter::FindAttachment( const StringView& name )
{
	for( size_t i = 0; i < attachments.size(); ++i )
	{
		if( attachments[ i ].name == name )
			return i;
	}
	return -1;
}

void AnimCharacter::SortEnsureAttachments( const StringView* atchnames, int count )
{
	for( int i = 0; i < count; ++i )
	{
		int aid = -1;
		for( size_t j = i; j < attachments.size(); ++j )
		{
			if( attachments[ i ].name == atchnames[ i ] )
			{
				aid = j;
				break;
			}
		}
		if( aid == i )
			continue; // at the right place already
		if( aid != -1 )
		{
			TMEMSWAP( attachments[ i ], attachments[ aid ] );
		}
		else
		{
			attachments.insert( i, Attachment() );
			attachments[ i ].name = atchnames[ i ];
		}
	}
}

void AnimCharacter::RaycastAll( const Vec3& from, const Vec3& to, SceneRaycastCallback* cb, SGRX_MeshInstance* cbmi )
{
	UNUSED( cbmi ); // always use own mesh instance
	if( !m_cachedMesh || !m_cachedMeshInst )
		return;
	for( size_t i = 0; i < bones.size(); ++i )
	{
		BoneInfo& BI = bones[ i ];
		Mat4 bxf = Mat4::CreateRotationFromQuat( BI.hitbox.rotation ) *
			Mat4::CreateTranslation( BI.hitbox.position );
		if( BI.bone_id >= 0 )
		{
			bxf = bxf * m_cachedMesh->m_bones[ BI.bone_id ].skinOffset;
			if( m_cachedMeshInst->skin_matrices.size() )
			{
				bxf = bxf * m_cachedMeshInst->skin_matrices[ BI.bone_id ];
			}
		}
		bxf = bxf * m_cachedMeshInst->matrix;
		
		Mat4 inv;
		if( bxf.InvertTo( inv ) )
		{
			Vec3 p0 = inv.TransformPos( from );
			Vec3 p1 = inv.TransformPos( to );
			float dst[1];
			if( SegmentAABBIntersect2( p0, p1, -BI.hitbox.extents, BI.hitbox.extents, dst ) )
			{
				Vec3 N = ( from - to ).Normalized();
				SceneRaycastInfo srci = { dst[0], N, 0, 0, -1, -1, BI.bone_id, m_cachedMeshInst };
				cb->AddResult( &srci );
			}
		}
	}
}



void ParticleSystem::Emitter::Tick( float dt, const Vec3& accel, const Mat4& mtx )
{
	if( state_SpawnCurrCount < state_SpawnTotalCount )
	{
		state_SpawnCurrTime = clamp( state_SpawnCurrTime + dt, 0, state_SpawnTotalTime );
		int currcount = state_SpawnCurrTime / state_SpawnTotalTime * state_SpawnTotalCount;
		if( state_SpawnCurrCount < currcount )
		{
			Generate( currcount - state_SpawnCurrCount, mtx );
			state_SpawnCurrCount = currcount;
		}
	}
	
	for( size_t i = 0; i < particles_Position.size(); ++i )
	{
		Vec2& LT = particles_Lifetime[ i ];
		LT.x += LT.y * dt;
		if( LT.x >= 1 )
		{
			// remove particle
			particles_Position.uerase( i );
			particles_Velocity.uerase( i );
			particles_Lifetime.uerase( i );
			particles_RandSizeAngVel.uerase( i );
			particles_RandColor.uerase( i );
			i--;
			continue;
		}
		Vec3& P = particles_Position[ i ];
		Vec3& V = particles_Velocity[ i ];
		V += accel * dt;
		P += V * dt;
	}
	
	state_lastDelta = dt;
}


static FINLINE Vec3 _ps_rotate( const Vec3& v, const Vec3& axis, float angle )
{
	// http://en.wikipedia.org/wiki/Axis–angle_representation#Rotating_a_vector
	
	float cos_a = cosf( angle );
	float sin_a = sinf( angle );
	Vec3 cross = Vec3Cross( axis, v );
	float dot = Vec3Dot( axis, v );
	
	return cos_a * v + sin_a * cross + ( 1 - cos_a ) * dot * axis;
}

static FINLINE Vec3 _ps_diverge( const Vec3& dir, float dvg )
{
	float baseangle = randf() * M_PI * 2;
	float rotangle = randf() * M_PI * dvg;
	Vec3 axis = { cosf( baseangle ), sinf( baseangle ), 0 };
	
	return _ps_rotate( dir, axis, rotangle );
}

static FINLINE int randi( int x )
{
	if( !x )
		return 0;
	return rand() % x;
}

void ParticleSystem::Emitter::Generate( int count, const Mat4& mtx )
{
	Vec3 velMicroDir = create_VelMicroDir.Normalized();
	Vec3 velMacroDir = create_VelMacroDir.Normalized();
	
	int clusterleft = 0;
	float clusterdist = 0;
	Vec3 clusteraxis = {0,0,0};
	float clusterangle = 0;
	
	int allocpos = 0;
	
	for( int i = 0; i < count; ++i )
	{
		if( clusterleft <= 0 )
		{
			clusterleft = create_VelCluster + randi( create_VelClusterExt );
			clusterdist = create_VelMacroDistExt.x + create_VelMacroDistExt.y * randf();
			Vec3 clusterdir = _ps_diverge( velMacroDir, create_VelMacroDvg );
			clusteraxis = Vec3Cross( V3(0,0,1), clusterdir ).Normalized();
			clusterangle = acos( clamp( Vec3Dot( V3(0,0,1), clusterdir ), -1, 1 ) );
		}
		clusterleft--;
		
		// Lifetime
		float LT = create_LifetimeExt.x + create_LifetimeExt.y * randf();
		if( LT <= 0 )
			continue;
		Vec2 LTV = { 0, 1.0f / LT };
		
		// Position
		Vec3 P = create_Pos;
		P += V3( create_PosBox.x * randf11(), create_PosBox.y * randf11(), create_PosBox.z * randf11() );
		float pos_ang = randf() * M_PI * 2, pos_zang = randf() * M_PI, pos_len = randf() * create_PosRadius;
		float cos_zang = cos( pos_zang ), sin_zang = sin( pos_zang );
		P += V3( cos( pos_ang ) * sin_zang, sin( pos_ang ) * sin_zang, cos_zang ) * pos_len;
		
		// Velocity
		Vec3 V = _ps_diverge( velMicroDir, create_VelMicroDvg );
		V = _ps_rotate( V, clusteraxis, clusterangle );
		V *= clusterdist + create_VelMicroDistExt.x + create_VelMicroDistExt.y * randf();
		
		// absolute positioning
		if( absolute )
		{
			P = mtx.TransformPos( P );
			V = mtx.TransformNormal( V );
		}
		
		// size, angle
		Vec3 randSAV =
		{
			randf(),
			create_AngleDirDvg.x + create_AngleDirDvg.y * randf11(),
			create_AngleVelDvg.x + create_AngleVelDvg.y * randf11()
		};
		// color [HSV], opacity
		Vec4 randHSVO = { randf(), randf(), randf(), randf() };
		
		if( particles_Position.size() < (size_t) spawn_MaxCount )
		{
			particles_Position.push_back( P );
			particles_Velocity.push_back( V );
			particles_Lifetime.push_back( LTV );
			particles_RandSizeAngVel.push_back( randSAV );
			particles_RandColor.push_back( randHSVO );
		}
		else
		{
			int i = ( allocpos++ ) % spawn_MaxCount;
			particles_Position[ i ] = P;
			particles_Velocity[ i ] = V;
			particles_Lifetime[ i ] = LTV;
			particles_RandSizeAngVel[ i ] = randSAV;
			particles_RandColor[ i ] = randHSVO;
		}
	}
}

void ParticleSystem::Emitter::Trigger( const Mat4& mtx )
{
	state_SpawnTotalCount = spawn_Count + randi( spawn_CountExt );
	state_SpawnCurrCount = 0;
	state_SpawnTotalTime = spawn_TimeExt.x + spawn_TimeExt.y * randf();
	state_SpawnCurrTime = 0;
	if( state_SpawnTotalTime == 0 )
	{
		Generate( state_SpawnTotalCount, mtx );
		state_SpawnCurrCount += state_SpawnTotalCount;
	}
}


void ParticleSystem::Emitter::PreRender( Array< Vertex >& vertices, Array< uint16_t >& indices, ps_prerender_info& info )
{
	const Vec3 S_X = info.basis[0];
	const Vec3 S_Y = info.basis[1];
	const Vec3 S_Z = info.basis[2];
	
	size_t bv = vertices.size();
	
	for( size_t i = 0; i < particles_Position.size(); ++i )
	{
		// step 1: extract data
		Vec3 POS = particles_Position[ i ];
		Vec3 VEL = particles_Velocity[ i ];
		Vec2 LFT = particles_Lifetime[ i ];
		Vec3 SAV = particles_RandSizeAngVel[ i ];
		Vec4 RCO = particles_RandColor[ i ];
		
		if( absolute == false )
		{
			POS = info.transform.TransformPos( POS );
			VEL = info.transform.TransformNormal( VEL );
		}
		
		// step 2: fill in the missing data
		float q = LFT.x;
		float t = LFT.x / LFT.y;
		float ANG = SAV.y + ( SAV.z + tick_AngleAcc * t ) * t;
		float SIZ = curve_Size.GetValue( q, SAV.x );
		Vec4 COL = V4
		(
			HSV(
				V3(
					curve_ColorHue.GetValue( q, RCO.x ),
					curve_ColorSat.GetValue( q, RCO.y ),
					curve_ColorVal.GetValue( q, RCO.z )
				)
			),
			curve_Opacity.GetValue( q, RCO.w )
		);
		
		// step 3: generate vertex/index data
		if( render_Stretch )
		{
			uint16_t cv = vertices.size() - bv;
			uint16_t idcs[18] =
			{
				cv+0, cv+2, cv+3, cv+3, cv+1, cv+0,
				cv+2, cv+4, cv+5, cv+5, cv+3, cv+2,
				cv+4, cv+6, cv+7, cv+7, cv+5, cv+4,
			};
			indices.append( idcs, 18 );
			
			Vec3 P_2 = POS - VEL * state_lastDelta;
			
			Vec2 proj1 = info.viewProj.TransformPos( POS ).ToVec2();
			Vec2 proj2 = info.viewProj.TransformPos( P_2 ).ToVec2();
			Vec2 dir2d = ( proj2 - proj1 ).Normalized();
			
			// invert Y
			Vec3 D_X = ( S_X * dir2d.x - S_Y * dir2d.y ).Normalized();
			Vec3 D_Y = Vec3Cross( S_Z, D_X ).Normalized();
			
			Vertex verts[8] =
			{
				{ P_2 + ( -D_X -D_Y ) * SIZ, COL,   0,   0, 0 },
				{ P_2 + ( -D_X +D_Y ) * SIZ, COL,   0, 255, 0 },
				{ P_2 + (      -D_Y ) * SIZ, COL, 127,   0, 0 },
				{ P_2 + (      +D_Y ) * SIZ, COL, 127, 255, 0 },
				{ POS + (      -D_Y ) * SIZ, COL, 127,   0, 0 },
				{ POS + (      +D_Y ) * SIZ, COL, 127, 255, 0 },
				{ POS + ( +D_X -D_Y ) * SIZ, COL, 255,   0, 0 },
				{ POS + ( +D_X +D_Y ) * SIZ, COL, 255, 255, 0 },
			};
			vertices.append( verts, 8 );
		}
		else
		{
			uint16_t cv = vertices.size() - bv;
			uint16_t idcs[6] = { cv, cv+1, cv+2, cv+2, cv+3, cv };
			indices.append( idcs, 6 );
			
			float ang = M_PI + ANG;
			Vec3 RSX = _ps_rotate( S_X, S_Z, ang );
			Vec3 RSY = _ps_rotate( S_Y, S_Z, ang );
			
			Vertex verts[4] =
			{
				{ POS + ( -RSX -RSY ) * SIZ, COL,   0,   0, 0 },
				{ POS + ( +RSX -RSY ) * SIZ, COL, 255,   0, 0 },
				{ POS + ( +RSX +RSY ) * SIZ, COL, 255, 255, 0 },
				{ POS + ( -RSX +RSY ) * SIZ, COL,   0, 255, 0 },
			};
			vertices.append( verts, 4 );
		}
	}
}


bool ParticleSystem::Load( const StringView& sv )
{
	ByteArray ba;
	if( !FS_LoadBinaryFile( sv, ba ) )
		return false;
	ByteReader br( &ba );
	Serialize( br, false );
	return !br.error;
}

bool ParticleSystem::Save( const StringView& sv )
{
	ByteArray ba;
	ByteWriter bw( &ba );
	Serialize( bw, false );
	return FS_SaveBinaryFile( sv, ba.data(), ba.size() );
}

void ParticleSystem::OnRenderUpdate()
{
	if( !m_scene )
		return;
	
	if( !m_vdecl )
		m_vdecl = GR_GetVertexDecl( PARTICLE_VDECL );
	
	m_meshInsts.resize( emitters.size() );
	
	for( size_t i = 0; i < emitters.size(); ++i )
	{
		Emitter& E = emitters[ i ];
		
		if( !m_meshInsts[ i ] )
			m_meshInsts[ i ] = m_scene->CreateMeshInstance();
		if( !m_meshInsts[ i ]->mesh )
			m_meshInsts[ i ]->mesh = GR_CreateMesh();
		
	//	m_meshInsts[ i ]->matrix = Mat4::Identity; // E.absolute ? Mat4::Identity : m_transform;
	//	m_meshInsts[ i ]->transparent = 1;
	//	m_meshInsts[ i ]->additive = E.render_Additive;
	//	m_meshInsts[ i ]->unlit = E.render_Additive;
		
		SGRX_MeshPart MP = { 0, 0, 0, 0 };
		
		MaterialHandle mh = GR_CreateMaterial();
		mh->transparent = 1;
		mh->additive = E.render_Additive;
		mh->unlit = E.render_Additive;
		mh->shader = GR_GetSurfaceShader( String_Concat( E.render_Shader, "+PARTICLE" ) );
		for( int t = 0; t < NUM_PARTICLE_TEXTURES; ++t )
			mh->textures[ t ] = E.render_Textures[ t ];
		MP.material = mh;
		
		m_meshInsts[ i ]->mesh->SetPartData( &MP, 1 );
	}
}

void ParticleSystem::AddToScene( SceneHandle sh )
{
	if( !m_vdecl )
		m_vdecl = GR_GetVertexDecl( PARTICLE_VDECL );
	
	m_scene = sh;
	
	OnRenderUpdate();
}

void ParticleSystem::SetTransform( const Mat4& mtx )
{
	m_transform = mtx;
}

bool ParticleSystem::Tick( float dt )
{
	bool retval = false;
	
	float prevrt = m_retriggerTime;
	m_retriggerTime -= dt;
	if( m_retriggerTime <= 0 && prevrt > 0 )
	{
		Trigger();
		m_retriggerTime = looping ? ( retriggerTimeExt.x + retriggerTimeExt.y * randf() ) : 0;
		retval = true;
	}
	
	for( size_t i = 0; i < emitters.size(); ++i )
	{
		emitters[ i ].Tick( dt, gravity, m_transform );
	}
	return retval;
}

void ParticleSystem::PreRender()
{
	if( !m_scene )
		return;
	
	const Mat4& invmtx = m_scene->camera.mInvView;
	ps_prerender_info info =
	{
		m_scene,
		m_transform,
		m_scene->camera.mView * m_scene->camera.mProj,
		{
			invmtx.TransformNormal( V3(1,0,0) ),
			invmtx.TransformNormal( V3(0,1,0) ),
			invmtx.TransformNormal( V3(0,0,1) )
		}
	};
	
	if( m_meshInsts.size() != emitters.size() )
		OnRenderUpdate();
	
	for( size_t i = 0; i < emitters.size(); ++i )
	{
		MeshHandle mesh = m_meshInsts[ i ]->mesh;
		
		m_vertices.clear();
		m_indices.clear();
		
		SGRX_MeshPart& MP = mesh->m_meshParts[ 0 ];
		MP.vertexOffset = 0;
		MP.indexOffset = 0;
		
		emitters[ i ].PreRender( m_vertices, m_indices, info );
		
		MP.vertexCount = m_vertices.size();
		MP.indexCount = m_indices.size();
		
		if( m_vertices.size() )
		{
			if( m_vertices.size_bytes() > mesh->m_vertexDataSize )
				mesh->SetVertexData( m_vertices.data(), m_vertices.size_bytes(), m_vdecl, false );
			else
				mesh->UpdateVertexData( m_vertices.data(), m_vertices.size_bytes(), false );
			mesh->SetAABBFromVertexData( m_vertices.data(), m_vertices.size_bytes(), m_vdecl );
		}
		
		if( m_indices.size() )
		{
			if( m_indices.size_bytes() > mesh->m_indexDataSize )
				mesh->SetIndexData( m_indices.data(), m_indices.size_bytes(), false );
			else
				mesh->UpdateIndexData( m_indices.data(), m_indices.size_bytes() );
		}
	}
}

void ParticleSystem::Trigger()
{
	for( size_t i = 0; i < emitters.size(); ++i )
	{
		emitters[ i ].Trigger( m_transform );
	}
}

void ParticleSystem::Play()
{
	if( !m_isPlaying )
	{
		m_isPlaying = true;
		m_retriggerTime = 0.00001f;
	}
}

void ParticleSystem::Stop()
{
	if( m_isPlaying )
	{
		m_isPlaying = false;
		m_retriggerTime = 0;
	}
}




//////////////////////
//  R A G D O L L
////////////////////

AnimRagdoll::AnimRagdoll() :
	m_enabled( false ),
	m_lastTickSize( 0 )
{
}

void AnimRagdoll::Initialize( PhyWorldHandle world, MeshHandle mesh, SkeletonInfo* skinfo )
{
	m_mesh = mesh;
	
	SGRX_PhyRigidBodyInfo rbinfo;
	rbinfo.enabled = false;
	rbinfo.friction = 0.8f;
	rbinfo.restitution = 0.02f;
	
	for( size_t bid = 0; bid < skinfo->bodies.size(); ++bid )
	{
		SkeletonInfo::Body& SB = skinfo->bodies[ bid ];
		
		Body* TB = NULL;
		for( size_t i = 0; i < m_bones.size(); ++i )
		{
			if( names[ i ] == SB.name )
			{
				TB = &m_bones[ i ];
				break;
			}
		}
		if( !TB )
			continue;
		
	//	LOG << SB.name << " > " << SB.capsule_radius << "|" << SB.capsule_height;
		rbinfo.shape = world->CreateCapsuleShape( SB.capsule_radius, SB.capsule_height );
		rbinfo.mass = 0;//4.0f / 3.0f * M_PI * SB.capsule_radius * SB.capsule_radius * SB.capsule_radius + M_PI * SB.capsule_radius * SB.capsule_radius * SB.capsule_height;
		
		rbinfo.inertia = rbinfo.shape->CalcInertia( rbinfo.mass );
		
		TB->relPos = SB.position;
		TB->relRot = SB.rotation;
		TB->bodyHandle = world->CreateRigidBody( rbinfo );
	}
	
	for( size_t jid = 0; jid < skinfo->joints.size(); ++jid )
	{
		SkeletonInfo::Joint& SJ = skinfo->joints[ jid ];
		UNUSED( SJ ); // TODO
	}
}

void AnimRagdoll::Prepare( String* new_names, int count )
{
	Animator::Prepare( new_names, count );
	m_bones.resize( count );
	for( int i = 0; i < count; ++i )
	{
		Body B = { V3(0), Quat::Identity, NULL, V3(0), V3(0), Quat::Identity, Quat::Identity };
		m_bones[ i ] = B;
		position[ i ] = V3( 0 );
		rotation[ i ] = Quat::Identity;
		scale[ i ] = V3( 1 );
		factor[ i ] = 1;
	}
}

void AnimRagdoll::Advance( float deltaTime )
{
	m_lastTickSize = deltaTime;
	if( m_enabled == false )
		return;
	for( size_t i = 0; i < names.size(); ++i )
	{
		Body& B = m_bones[ i ];
		if( B.bodyHandle )
		{
			Vec3 pos = B.bodyHandle->GetPosition();
			Quat rot = B.bodyHandle->GetRotation();
			Vec3 invRelPos = -B.relPos;
			Quat invRelRot = B.relRot.Inverted();
			position[ i ] = invRelRot.Transform( pos - invRelPos );
			rotation[ i ] = rot * invRelRot;
		}
	}
}

void AnimRagdoll::SetBoneTransforms( int bone_id, const Vec3& prev_pos, const Vec3& curr_pos, const Quat& prev_rot, const Quat& curr_rot )
{
	ASSERT( bone_id >= 0 && bone_id < (int) names.size() );
	Body& B = m_bones[ bone_id ];
	B.prevPos = prev_pos;
	B.currPos = curr_pos;
	B.prevRot = prev_rot;
	B.currRot = curr_rot;
}

void AnimRagdoll::AdvanceTransforms( Animator* anim )
{
	ASSERT( names.size() == anim->names.size() );
	for( size_t i = 0; i < names.size(); ++i )
	{
		Body& B = m_bones[ i ];
		B.prevPos = B.currPos;
		B.prevRot = B.currRot;
		B.currPos = anim->position[ i ];
		B.currRot = anim->rotation[ i ];
	}
}

void AnimRagdoll::EnablePhysics( const Mat4& worldMatrix )
{
	if( m_enabled )
		return;
	m_enabled = true;
	
	Mat4 prev_boneToWorldMatrices[ MAX_MESH_BONES ];
	Mat4 curr_boneToWorldMatrices[ MAX_MESH_BONES ];
	SGRX_MeshBone* MB = m_mesh->m_bones;
	for( size_t i = 0; i < names.size(); ++i )
	{
		Body& B = m_bones[ i ];
		Mat4& prev_M = prev_boneToWorldMatrices[ i ];
		Mat4& curr_M = curr_boneToWorldMatrices[ i ];
		prev_M = Mat4::CreateSRT( V3(1), B.prevRot, B.prevPos ) * MB[ i ].boneOffset;
		curr_M = Mat4::CreateSRT( V3(1), B.currRot, B.currPos ) * MB[ i ].boneOffset;
		if( MB[ i ].parent_id >= 0 )
		{
			prev_M = prev_M * prev_boneToWorldMatrices[ MB[ i ].parent_id ];
			curr_M = curr_M * curr_boneToWorldMatrices[ MB[ i ].parent_id ];
		}
		else
		{
			prev_M = prev_M * worldMatrix;
			curr_M = curr_M * worldMatrix;
		}
	}
	
	for( size_t i = 0; i < m_bones.size(); ++i )
	{
		Body& B = m_bones[ i ];
		if( B.bodyHandle )
		{
			// calculate world transforms
			Mat4 rtf = Mat4::CreateSRT( V3(1), B.relRot, B.relPos );
			Mat4 prev_wtf = rtf * prev_boneToWorldMatrices[ i ];
			Mat4 curr_wtf = rtf * curr_boneToWorldMatrices[ i ];
			
			Vec3 prev_wpos = prev_wtf.GetTranslation();
			Vec3 curr_wpos = curr_wtf.GetTranslation();
			Quat prev_wrot = prev_wtf.GetRotationQuaternion();
			Quat curr_wrot = curr_wtf.GetRotationQuaternion();
			
			B.bodyHandle->SetPosition( curr_wpos );
			B.bodyHandle->SetRotation( curr_wrot );
			if( m_lastTickSize > SMALL_FLOAT )
			{
				float speed = 1.0f / m_lastTickSize;
				B.bodyHandle->SetLinearVelocity( ( curr_wpos - prev_wpos ) * speed );
				B.bodyHandle->SetAngularVelocity( CalcAngularVelocity( prev_wrot, curr_wrot ) * speed );
			}
			B.bodyHandle->SetEnabled( true );
		}
	}
	for( size_t i = 0; i < m_joints.size(); ++i )
	{
		m_joints[ i ]->SetEnabled( true );
	}
}

void AnimRagdoll::DisablePhysics()
{
	if( m_enabled == false )
		return;
	m_enabled = false;
	for( size_t i = 0; i < m_bones.size(); ++i )
	{
		Body& B = m_bones[ i ];
		if( B.bodyHandle )
			B.bodyHandle->SetEnabled( false );
	}
	for( size_t i = 0; i < m_joints.size(); ++i )
	{
		m_joints[ i ]->SetEnabled( false );
	}
}



DecalSystem::DecalSystem() : m_vbSize(0)
{
}

DecalSystem::~DecalSystem()
{
	Free();
}

void DecalSystem::Init( TextureHandle texDecal, TextureHandle texFalloff, DecalMapPartInfo* decalCoords, int numDecals )
{
	m_vertexDecl = GR_GetVertexDecl( SGRX_VDECL_DECAL );
	m_mesh = GR_CreateMesh();
	m_decalBounds.assign( decalCoords, numDecals );
	m_material = GR_CreateMaterial(); // TODO: pass loaded material when there is something to load
	m_material->transparent = true;
	m_material->shader = GR_GetSurfaceShader( "default" );
	m_material->textures[0] = texDecal;
	m_material->textures[1] = texFalloff;
}

void DecalSystem::Free()
{
	ClearAllDecals();
	m_mesh = NULL;
	m_decalBounds.clear();
	m_vbSize = 0;
}

void DecalSystem::SetSize( uint32_t vbSize )
{
	m_vbSize = vbSize;
}

void DecalSystem::Upload()
{
	// cut excess decals
	size_t vbcutsize = 0, vbsize = m_vertexData.size(), ibcutsize = 0, cutcount = 0;
	while( vbsize > m_vbSize + vbcutsize )
	{
		vbcutsize += m_decals[ cutcount++ ];
		ibcutsize += m_decals[ cutcount++ ];
	}
	if( cutcount )
	{
		m_vertexData.erase( 0, vbcutsize );
		m_indexData.erase( 0, ibcutsize );
		m_decals.erase( 0, cutcount );
		uint32_t iboff = vbcutsize / sizeof(SGRX_Vertex_Decal);
		for( size_t i = 0; i < m_indexData.size(); ++i )
		{
			m_indexData[ i ] -= iboff;
		}
	}
	
	// apply data
	if( m_vertexData.size() )
	{
		m_mesh->SetVertexData( m_vertexData.data(), m_vertexData.size_bytes(), m_vertexDecl, true );
		m_mesh->SetIndexData( m_indexData.data(), m_indexData.size_bytes(), true );
		SGRX_MeshPart mp = { 0, m_vertexData.size() / sizeof(SGRX_Vertex_Decal), 0, m_indexData.size(), m_material };
		m_mesh->SetPartData( &mp, 1 );
	}
}

void DecalSystem::AddDecal( int decalID, SGRX_IMesh* targetMesh, const Mat4& worldMatrix, DecalProjectionInfo* projInfo )
{
	ASSERT( decalID >= 0 && decalID < (int) m_decalBounds.size() );
	
	float inv_zn2zf;
	Mat4 vpmtx;
	_GenDecalMatrix( decalID, projInfo, &vpmtx, &inv_zn2zf );
	
	size_t origvbsize = m_vertexData.size(), origibsize = m_indexData.size();
	targetMesh->Clip( worldMatrix, vpmtx, m_vertexData, true, inv_zn2zf );
	if( m_vertexData.size() > origvbsize )
	{
		_ScaleDecalTexcoords( origvbsize, decalID );
		SGRX_DoIndexTriangleMeshVertices( m_indexData, m_vertexData, origvbsize, sizeof(SGRX_Vertex_Decal) );
		m_decals.push_back( m_vertexData.size() - origvbsize );
		m_decals.push_back( m_indexData.size() - origibsize );
	}
}

void DecalSystem::AddDecal( int decalID, SGRX_IMesh* targetMesh, int partID, const Mat4& worldMatrix, DecalProjectionInfo* projInfo )
{
	ASSERT( decalID >= 0 && decalID < (int) m_decalBounds.size() );
	
	float inv_zn2zf;
	Mat4 vpmtx;
	_GenDecalMatrix( decalID, projInfo, &vpmtx, &inv_zn2zf );
	
	size_t origvbsize = m_vertexData.size(), origibsize = m_indexData.size();
	targetMesh->Clip( worldMatrix, vpmtx, m_vertexData, true, inv_zn2zf, partID, 1 );
	if( m_vertexData.size() > origvbsize )
	{
		_ScaleDecalTexcoords( origvbsize, decalID );
		SGRX_DoIndexTriangleMeshVertices( m_indexData, m_vertexData, origvbsize, sizeof(SGRX_Vertex_Decal) );
		m_decals.push_back( m_vertexData.size() - origvbsize );
		m_decals.push_back( m_indexData.size() - origibsize );
	}
}

void DecalSystem::ClearAllDecals()
{
	m_vertexData.clear();
	m_indexData.clear();
	m_decals.clear();
}

void DecalSystem::GenerateCamera( int decalID, DecalProjectionInfo& projInfo, SGRX_Camera* out )
{
	ASSERT( decalID >= 0 && decalID < (int) m_decalBounds.size() );
	
	// TODO for now...
	ASSERT( projInfo.perspective );
	
	DecalMapPartInfo& DMPI = m_decalBounds[ decalID ];
	float dist = DMPI.size.z * projInfo.distanceScale;
	
	out->position = projInfo.pos - projInfo.dir * projInfo.pushBack * dist;
	out->direction = projInfo.dir;
	out->updir = projInfo.up;
	out->angle = projInfo.fovAngleDeg;
	out->aspect = projInfo.aspectMult;
	out->aamix = projInfo.aamix;
	out->znear = dist * 0.001f;
	out->zfar = dist;
	
	out->UpdateMatrices();
}

void DecalSystem::_ScaleDecalTexcoords( size_t vbfrom, int decalID )
{
	DecalMapPartInfo& DMPI = m_decalBounds[ decalID ];
	
	SGRX_CAST( SGRX_Vertex_Decal*, vdata, m_vertexData.data() );
	SGRX_Vertex_Decal* vdend = vdata + m_vertexData.size() / sizeof(SGRX_Vertex_Decal);
	vdata += vbfrom / sizeof(SGRX_Vertex_Decal);
	while( vdata < vdend )
	{
		vdata->texcoord.x = TLERP( DMPI.bbox.x, DMPI.bbox.z, vdata->texcoord.x );
		vdata->texcoord.y = TLERP( DMPI.bbox.y, DMPI.bbox.w, vdata->texcoord.y );
		vdata++;
	}
}

void DecalSystem::_GenDecalMatrix( int decalID, DecalProjectionInfo* projInfo, Mat4* outVPM, float* out_invzn2zf )
{
	DecalMapPartInfo& DMPI = m_decalBounds[ decalID ];
	
	float znear = 0, dist = DMPI.size.z * projInfo->distanceScale;
	Mat4 projMtx, viewMtx = Mat4::CreateLookAt(
		projInfo->pos - projInfo->dir * projInfo->pushBack * dist,
		projInfo->dir, projInfo->up );
	if( projInfo->perspective )
	{
		float aspect = DMPI.size.x / DMPI.size.y * projInfo->aspectMult;
		znear = dist * 0.001f;
		projMtx = Mat4::CreatePerspective( projInfo->fovAngleDeg, aspect, projInfo->aamix, znear, dist );
	}
	else
	{
		Vec2 psz = DMPI.size.ToVec2() * 0.5f * projInfo->orthoScale;
		projMtx = Mat4::CreateOrtho( V3( -psz.x, -psz.y, 0 ), V3( psz.x, psz.y, DMPI.size.z * projInfo->distanceScale ) );
	}
	*outVPM = viewMtx * projMtx;
	*out_invzn2zf = safe_fdiv( 1, dist - znear );
}




