

#include "lighter_int.hpp"



LTRBOOL ltr_DefaultSizeFunc
(
	ltr_Config* config,
	const char* mesh_ident,
	size_t mesh_ident_size,
	const char* inst_ident,
	size_t inst_ident_size,
	float computed_surface_area,
	float inst_importance,
	u32 out_size[2]
)
{
	float size = config->global_size_factor * sqrtf( computed_surface_area ) * inst_importance;
	if( size < 1 )
		size = 1;
	u32 ui_size = ltr_NextPowerOfTwo( (u32) size );
	if( ui_size > config->max_lightmap_size )
		return 0;
	out_size[0] = ui_size;
	out_size[1] = ui_size;
	return 1;
}


struct ltr_MeshPart
{
	u32 m_vertexCount;
	u32 m_vertexOffset;
	u32 m_indexCount;
	u32 m_indexOffset;
	int m_tristrip;
};
typedef std::vector< ltr_MeshPart > MeshPartVector;

struct ltr_Mesh
{
	ltr_Mesh( ltr_Scene* s ) : m_scene( s ){}
	
	ltr_Scene* m_scene;
	std::string m_ident;
	Vec3Vector m_vpos;
	Vec3Vector m_vnrm;
	Vec2Vector m_vtex1;
	Vec2Vector m_vtex2;
	U32Vector m_indices;
	MeshPartVector m_parts;
};
typedef std::vector< ltr_Mesh* > MeshPtrVector;

struct ltr_MeshInstance
{
	// input
	ltr_Mesh* mesh;
	std::string m_ident;
	float m_importance;
	Mat4 matrix;
	u32 lm_width;
	u32 lm_height;
	bool m_shadow;
	bool m_samplecont;
	
	// tmp
	Vec3Vector m_vpos;
	Vec3Vector m_vnrm;
	Vec2Vector m_vtex;
	Vec2Vector m_ltex;
	
	// output
	Vec3Vector m_samples_pos;
	Vec3Vector m_samples_nrm;
	U32Vector m_samples_loc;
	Vec4Vector m_samples_radinfo;
	Vec3Vector m_lightmap;
};
typedef std::vector< ltr_MeshInstance* > MeshInstPtrVector;

struct ltr_Light
{
	u32 type;
	Vec3 position;
	Vec3 direction;
	Vec3 up_direction;
	Vec3 color_rgb;
	float range;
	float power;
	float light_radius;
	int shadow_sample_count;
	float spot_angle_out;
	float spot_angle_in;
	float spot_curve;
};
typedef std::vector< ltr_Light > LightVector;

typedef std::vector< ltr_SampleInfo > SampleVector;

struct ltr_RadSampleGeom
{
	Vec3 pos;
	Vec3 normal;
};
typedef std::vector< ltr_RadSampleGeom > RadSampleGeomVector;

struct ltr_RadSampleColors
{
	Vec3 diffuseColor;
	Vec3 totalLight;
	Vec3 outputEnergy;
	Vec3 inputEnergy;
	float area;
};
typedef std::vector< ltr_RadSampleColors > RadSampleColorsVector;

struct ltr_RadLink
{
	uint32_t other;
	float factor;
};
typedef std::vector< ltr_RadLink > RadLinkVector;

struct ltr_Scene
{
	ltr_Scene() : m_workType( LTR_WT_PREXFORM ), m_workPart( 0 )
	{
		ltr_GetConfig( &config, NULL );
		
		m_sampleMI.m_samplecont = true;
		m_sampleMI.mesh = NULL;
		m_sampleMI.m_importance = 0;
		m_sampleMI.lm_width = 0;
		m_sampleMI.lm_height = 0;
		m_meshInstances.push_back( &m_sampleMI );
	}
	~ltr_Scene()
	{
		for( size_t i = 1; i < m_meshInstances.size(); ++i )
			delete m_meshInstances[i];
		for( size_t i = 0; i < m_meshes.size(); ++i )
			delete m_meshes[i];
		for( size_t i = 0; i < m_workOutput.size(); ++i )
			delete [] m_workOutput[i].lightmap_rgb;
	}
	
	void DoWork();
	LTRCODE Advance();
	void RasterizeInstance( ltr_MeshInstance* mi, float margin );
	float VisibilityTest( const Vec3& A, ltr_Light* light );
	float VisibilityTest( const Vec3& A, const Vec3& B, Vec3* outnormal = NULL );
	
	ltr_Config config;
	
	MeshPtrVector m_meshes;
	MeshInstPtrVector m_meshInstances;
	LightVector m_lights;
	ltr_MeshInstance m_sampleMI;
	SampleVector m_samples;
	
	RadSampleGeomVector m_radSampleGeoms;
	RadSampleColorsVector m_radSampleColors;
	RadLinkVector m_radLinks;
	U32Vector m_radLinkMap;
	
	Vec3Vector m_tmpRender1;
	Vec3Vector m_tmpRender2;
	Vec4Vector m_tmpRender3;
	BSPTree m_triTree;
	WorkOutputVector m_workOutput;
	
	u32 m_workType;
	u32 m_workPart;
};


ltr_Scene* ltr_CreateScene()
{
	return new ltr_Scene;
}

void ltr_DestroyScene( ltr_Scene* scene )
{
	delete scene;
}

void ltr_Scene::RasterizeInstance( ltr_MeshInstance* mi, float margin )
{
	ltr_Mesh* mesh = mi->mesh;
	for( u32 part = 0; part < mesh->m_parts.size(); ++part )
	{
		const ltr_MeshPart& mp = mesh->m_parts[ part ];
		u32 O = mp.m_vertexOffset;
		const u32* indexBase = &mesh->m_indices[ mp.m_indexOffset ];
		if( mp.m_tristrip )
		{
			for( u32 tri = 2; tri < mp.m_indexCount; ++tri )
			{
				u32 tridx1 = tri, tridx2 = tri + 1 + tri % 2, tridx3 = tri + 2 - tri % 2;
				
				tridx1 = O + indexBase[ tridx1 ];
				tridx2 = O + indexBase[ tridx2 ];
				tridx3 = O + indexBase[ tridx3 ];
				
				float samplearea = CalculateSampleArea(
					mi->m_ltex[ tridx1 ], mi->m_ltex[ tridx2 ], mi->m_ltex[ tridx3 ],
					mi->m_vpos[ tridx1 ], mi->m_vpos[ tridx2 ], mi->m_vpos[ tridx3 ]
				);
				
				RasterizeTriangle2D_x2_ex
				(
					&m_tmpRender1[0], &m_tmpRender2[0], &m_tmpRender3[0], mi->lm_width, mi->lm_height, margin,
					mi->m_ltex[ tridx1 ], mi->m_ltex[ tridx2 ], mi->m_ltex[ tridx3 ],
					mi->m_vpos[ tridx1 ], mi->m_vpos[ tridx2 ], mi->m_vpos[ tridx3 ],
					mi->m_vnrm[ tridx1 ], mi->m_vnrm[ tridx2 ], mi->m_vnrm[ tridx3 ],
					V4( mi->m_vtex[ tridx1 ].x, mi->m_vtex[ tridx1 ].y, (float) part, samplearea ),
					V4( mi->m_vtex[ tridx2 ].x, mi->m_vtex[ tridx2 ].y, (float) part, samplearea ),
					V4( mi->m_vtex[ tridx3 ].x, mi->m_vtex[ tridx3 ].y, (float) part, samplearea )
				);
			}
		}
		else
		{
			for( u32 tri = 0; tri < mp.m_indexCount; tri += 3 )
			{
				u32 tridx1 = tri, tridx2 = tri + 1, tridx3 = tri + 2;
				
				tridx1 = O + indexBase[ tridx1 ];
				tridx2 = O + indexBase[ tridx2 ];
				tridx3 = O + indexBase[ tridx3 ];
				
				float samplearea = CalculateSampleArea(
					mi->m_ltex[ tridx1 ], mi->m_ltex[ tridx2 ], mi->m_ltex[ tridx3 ],
					mi->m_vpos[ tridx1 ], mi->m_vpos[ tridx2 ], mi->m_vpos[ tridx3 ]
				);
				
				RasterizeTriangle2D_x2_ex
				(
					&m_tmpRender1[0], &m_tmpRender2[0], &m_tmpRender3[0], mi->lm_width, mi->lm_height, margin,
					mi->m_ltex[ tridx1 ], mi->m_ltex[ tridx2 ], mi->m_ltex[ tridx3 ],
					mi->m_vpos[ tridx1 ], mi->m_vpos[ tridx2 ], mi->m_vpos[ tridx3 ],
					mi->m_vnrm[ tridx1 ], mi->m_vnrm[ tridx2 ], mi->m_vnrm[ tridx3 ],
					V4( mi->m_vtex[ tridx1 ].x, mi->m_vtex[ tridx1 ].y, (float) part, samplearea ),
					V4( mi->m_vtex[ tridx2 ].x, mi->m_vtex[ tridx2 ].y, (float) part, samplearea ),
					V4( mi->m_vtex[ tridx3 ].x, mi->m_vtex[ tridx3 ].y, (float) part, samplearea )
				);
			}
		}
	}
}

float ltr_Scene::VisibilityTest( const Vec3& A, ltr_Light* light )
{
	float total = 0.0f;
	for( int i = 0; i < light->shadow_sample_count; ++i )
	{
		const Vec3& B = light->position + Vec3::CreateRandomVector( light->light_radius );
		Vec3 diffnorm = ( B - A ).Normalized();
		Vec3 mA = A + diffnorm * SMALL_FLOAT;
		Vec3 mB = B - diffnorm * SMALL_FLOAT;
		
		float hit = m_triTree.IntersectRay( mB, mA );
		if( hit < 1.0f )
			total += TMIN( ( 1 - hit ) * ( B - A ).Length() * 1000.0f, 1.0f );
	}
	return 1.0f - total / (float) light->shadow_sample_count;
}

float ltr_Scene::VisibilityTest( const Vec3& A, const Vec3& B, Vec3* outnormal )
{
	Vec3 diffnorm = ( B - A ).Normalized();
	Vec3 mA = A + diffnorm * SMALL_FLOAT;
	Vec3 mB = B - diffnorm * SMALL_FLOAT;
	return m_triTree.IntersectRay( mA, mB, outnormal );
}

void ltr_Scene::DoWork()
{
	if( !m_meshInstances.size() || !m_lights.size() )
		return;
	
	switch( m_workType )
	{
	case LTR_WT_PREXFORM:
		{
			ltr_MeshInstance* mi = m_meshInstances[ m_workPart ];
			if( mi->m_samplecont )
				break;
			ltr_Mesh* mesh = mi->mesh;
			
			mi->m_vpos.resize( mesh->m_vpos.size() );
			mi->m_vnrm.resize( mesh->m_vnrm.size() );
			mi->m_vtex.resize( mesh->m_vtex1.size() );
			mi->m_ltex.resize( mesh->m_vtex2.size() );
			
			TransformPositions( &mi->m_vpos[0], &mesh->m_vpos[0], mesh->m_vpos.size(), mi->matrix );
			TransformNormals( &mi->m_vnrm[0], &mesh->m_vnrm[0], mesh->m_vnrm.size(), mi->matrix );
			
			// compute total area
			float total_area = 0.0f;
			for( u32 part = 0; part < mesh->m_parts.size(); ++part )
			{
				const ltr_MeshPart& mp = mesh->m_parts[ part ];
				const Vec3* vertexBase = &mi->m_vpos[ mp.m_vertexOffset ];
				const u32* indexBase = &mesh->m_indices[ mp.m_indexOffset ];
				if( mp.m_tristrip )
				{
					for( u32 tri = 2; tri < mp.m_indexCount; ++tri )
					{
						u32 tridx1 = tri, tridx2 = tri + 1 + tri % 2, tridx3 = tri + 2 - tri % 2;
						tridx1 = indexBase[ tridx1 ];
						tridx2 = indexBase[ tridx2 ];
						tridx3 = indexBase[ tridx3 ];
						total_area += TriangleArea( vertexBase[ tridx1 ], vertexBase[ tridx2 ], vertexBase[ tridx3 ] );
					}
				}
				else
				{
					for( u32 tri = 0; tri < mp.m_indexCount; tri += 3 )
					{
						u32 tridx1 = tri, tridx2 = tri + 1, tridx3 = tri + 2;
						tridx1 = indexBase[ tridx1 ];
						tridx2 = indexBase[ tridx2 ];
						tridx3 = indexBase[ tridx3 ];
						total_area += TriangleArea( vertexBase[ tridx1 ], vertexBase[ tridx2 ], vertexBase[ tridx3 ] );
					}
				}
			}
			
			// call lightmap resizer
			u32 out_size[2] = { config.default_width, config.default_height };
			if( !config.size_fn( &config, mesh->m_ident.c_str(), mesh->m_ident.size(), mi->m_ident.c_str(), mi->m_ident.size(), total_area, mi->m_importance, out_size ) )
			{
				out_size[0] = config.default_width;
				out_size[1] = config.default_height;
			}
			mi->lm_width = out_size[0];
			mi->lm_height = out_size[1];
			
			// transform texcoords
			Vec2 lsize = Vec2::Create( (float) mi->lm_width, (float) mi->lm_height );
			for( size_t i = 0; i < mi->m_ltex.size(); ++i )
			{
				mi->m_vtex[ i ] = mesh->m_vtex1[ i ];
				mi->m_ltex[ i ] = mesh->m_vtex2[ i ] * lsize - Vec2::Create(0.5f);
			}
		}
		break;
		
	case LTR_WT_COLINFO:
		{
			ltr_MeshInstance* mi = m_meshInstances[ m_workPart ];
			if( mi->m_samplecont )
				break;
			if( !mi->m_shadow )
				break;
			ltr_Mesh* mesh = mi->mesh;
			
			for( u32 part = 0; part < mesh->m_parts.size(); ++part )
			{
				const ltr_MeshPart& mp = mesh->m_parts[ part ];
				const Vec3* vertexBase = &mi->m_vpos[ mp.m_vertexOffset ];
				const u32* indexBase = &mesh->m_indices[ mp.m_indexOffset ];
				if( mp.m_tristrip )
				{
					for( u32 tri = 2; tri < mp.m_indexCount; ++tri )
					{
						u32 tridx1 = tri, tridx2 = tri + 1 + tri % 2, tridx3 = tri + 2 - tri % 2;
						tridx1 = indexBase[ tridx1 ];
						tridx2 = indexBase[ tridx2 ];
						tridx3 = indexBase[ tridx3 ];
						BSPTriangle T = { vertexBase[ tridx1 ], vertexBase[ tridx2 ], vertexBase[ tridx3 ] };
						m_triTree.AddTriangle( &T );
					}
				}
				else
				{
					for( u32 tri = 0; tri < mp.m_indexCount; tri += 3 )
					{
						u32 tridx1 = tri, tridx2 = tri + 1, tridx3 = tri + 2;
						tridx1 = indexBase[ tridx1 ];
						tridx2 = indexBase[ tridx2 ];
						tridx3 = indexBase[ tridx3 ];
						BSPTriangle T = { vertexBase[ tridx1 ], vertexBase[ tridx2 ], vertexBase[ tridx3 ] };
						m_triTree.AddTriangle( &T );
					}
				}
			}
		}
		break;
		
	case LTR_WT_SAMPLES:
		{
			float corr_min_dot = cosf( config.max_correct_angle / 180.0f * (float) M_PI );
			ltr_MeshInstance* mi = m_meshInstances[ m_workPart ];
			if( mi->m_samplecont )
				break;
			
			if( m_tmpRender1.size() < mi->lm_width * mi->lm_height )
				m_tmpRender1.resize( mi->lm_width * mi->lm_height );
			if( m_tmpRender2.size() < mi->lm_width * mi->lm_height )
				m_tmpRender2.resize( mi->lm_width * mi->lm_height );
			if( m_tmpRender3.size() < mi->lm_width * mi->lm_height )
				m_tmpRender3.resize( mi->lm_width * mi->lm_height );
			
			TMEMSET( &m_tmpRender1[0], m_tmpRender1.size(), Vec3::Create(0) );
			TMEMSET( &m_tmpRender2[0], m_tmpRender2.size(), Vec3::Create(0) );
			TMEMSET( &m_tmpRender3[0], m_tmpRender3.size(), V4(0) );
			
			// first do excessive rasterization
		//	RasterizeInstance( mi, 2.0f + SMALL_FLOAT );
		//	RasterizeInstance( mi, 1.5f + SMALL_FLOAT );
		//	RasterizeInstance( mi, 1.0f + SMALL_FLOAT );
			RasterizeInstance( mi, 0.5f + SMALL_FLOAT );
			
			// then do proper rasterization
			RasterizeInstance( mi, 0.0f );
			
			// compress samples to packed array
			u32 sample_count = 0;
			for( size_t i = 0; i < m_tmpRender2.size(); ++i )
			{
				Vec3& N = m_tmpRender2[ i ];
				if( !N.IsZero() )
					sample_count++;
			}
			mi->m_samples_pos.reserve( sample_count );
			mi->m_samples_nrm.reserve( sample_count );
			mi->m_samples_loc.reserve( sample_count );
			mi->m_samples_radinfo.reserve( sample_count );
			for( size_t i = 0; i < m_tmpRender2.size(); ++i )
			{
				Vec3& N = m_tmpRender2[ i ];
				if( !N.IsZero() )
				{
					Vec3 P = m_tmpRender1[ i ];
					Vec4 XD = m_tmpRender3[ i ];
					if( config.max_correct_dist )
					{
						int itsleft = 100;
						float md = config.max_correct_dist;
						Vec3 PEnd = P + N * md;
						while( md > SMALL_FLOAT && itsleft --> 0 )
						{
							Vec3 hitnrm = {0,0,0};
							float q = VisibilityTest( P, PEnd, &hitnrm );
							if( Vec3Dot( hitnrm, N ) < corr_min_dot )
								break;
							if( q < SMALL_FLOAT )
								q = SMALL_FLOAT;
							P = P * ( 1.0f - q ) + PEnd * q;
							md *= ( 1.0f - q );
						}
					}
					mi->m_samples_pos.push_back( P );
					mi->m_samples_nrm.push_back( N );
					mi->m_samples_loc.push_back( i );
					mi->m_samples_radinfo.push_back( XD );
				}
			}
			mi->m_lightmap.resize( sample_count );
			for( size_t i = 0; i < mi->m_lightmap.size(); ++i )
				mi->m_lightmap[ i ] = Vec3::CreateFromPtr( config.ambient_color );
		}
		break;
		
	case LTR_WT_LMRENDER:
		{
			size_t mi_id = m_workPart / m_lights.size();
			size_t light_id = m_workPart % m_lights.size();
			
			ltr_MeshInstance* mi = m_meshInstances[ mi_id ];
			ltr_Light& light = m_lights[ light_id ];
			
			if( light.type == LTR_LT_POINT )
			{
				for( size_t i = 0; i < mi->m_lightmap.size(); ++i )
				{
					Vec3& SP = mi->m_samples_pos[ i ];
					Vec3& SN = mi->m_samples_nrm[ i ];
					Vec3 sample2light = light.position - SP;
					float dist = sample2light.Length();
					if( dist )
						sample2light /= dist;
					float f_dist = pow( 1 - TMIN( 1.0f, dist / light.range ), light.power );
					float f_ndotl = TMAX( 0.0f, Vec3Dot( sample2light, SN ) );
					if( f_dist * f_ndotl < SMALL_FLOAT )
						continue;
					float f_vistest = VisibilityTest( SP, &light );
					mi->m_lightmap[ i ] += light.color_rgb * ( f_dist * f_ndotl * f_vistest );
				}
			}
			if( light.type == LTR_LT_SPOT )
			{
				float angle_out_rad = light.spot_angle_out / 180.0f * (float) M_PI, angle_in_rad = light.spot_angle_in / 180.0f * (float) M_PI;
				if( angle_in_rad == angle_out_rad )
					angle_in_rad -= SMALL_FLOAT;
				float angle_diff = angle_in_rad - angle_out_rad;
				for( size_t i = 0; i < mi->m_lightmap.size(); ++i )
				{
					Vec3& SP = mi->m_samples_pos[ i ];
					Vec3& SN = mi->m_samples_nrm[ i ];
					Vec3 sample2light = light.position - SP;
					float dist = sample2light.Length();
					if( dist )
						sample2light /= dist;
					float f_dist = pow( 1 - TMIN( 1.0f, dist / light.range ), light.power );
					float f_ndotl = TMAX( 0.0f, Vec3Dot( sample2light, SN ) );
					float angle = acosf( TMIN( 1.0f, Vec3Dot( sample2light, -light.direction ) ) );
					float f_dir = TMAX( 0.0f, TMIN( 1.0f, ( angle - angle_out_rad ) / angle_diff ) );
					f_dir = pow( f_dir, light.spot_curve );
					if( f_dist * f_ndotl * f_dir < SMALL_FLOAT )
						continue;
					float f_vistest = VisibilityTest( SP, &light );
					mi->m_lightmap[ i ] += light.color_rgb * ( f_dist * f_ndotl * f_dir * f_vistest );
				}
			}
			else if( light.type == LTR_LT_DIRECT )
			{
				int num_samples = TMAX( 1, light.shadow_sample_count );
				for( size_t i = 0; i < mi->m_lightmap.size(); ++i )
				{
					Vec3& SP = mi->m_samples_pos[ i ];
					Vec3& SN = mi->m_samples_nrm[ i ];
					float f_ndotl = 0.0f;
					float f_vistest = 0.0f;
					Vec3 ray_origin = SP;
					float randoff = randf();
					for( int s = 0; s < num_samples; ++s )
					{
					//	Vec3 adjdir = Vec3::CreateRandomVectorDirDvg( -light.direction, light.light_radius );
						Vec3 adjdir = Vec3::CreateSpiralDirVector( -light.direction, randoff, s, num_samples );
						adjdir += ( (-light.direction) * tan( ( light.light_radius - 0.5f ) * M_PI * 0.999f ) ).Normalized();
						f_ndotl += TMAX( 0.0f, Vec3Dot( adjdir, SN ) );
						float hit = VisibilityTest( ray_origin, ray_origin + adjdir * light.range );
						if( hit < 1.0f )
							f_vistest += 1.0f;
					}
					f_ndotl /= num_samples;
					f_vistest /= num_samples;
					f_vistest = 1.0f - f_vistest;
				//	if( mi->m_samples_loc[ i ] >= 209 + 14 * mi->lm_width &&
				//		mi->m_samples_loc[ i ] <= 211 + 14 * mi->lm_width && mi_id == 0 )
				//	{
				//		printf( "PIXEL: %d\n", mi->m_samples_loc[ i ] );
				//		printf( "VISTEST = %f\n", f_vistest );
				//		printf( "POS = %f %f %f\n", SP.x, SP.y, SP.z );
				//		printf( "NRM = %f %f %f\n", SN.x, SN.y, SN.z );
				//	//	if( mi->m_samples_loc[ i ] == 211 + 14 * mi->lm_width )
				//	//		exit(0);
				//	}
					mi->m_lightmap[ i ] += light.color_rgb * ( f_ndotl * f_vistest );
				}
			}
		}
		break;
		
	case LTR_WT_RDGENLNK:
		{
#ifdef LTRDEBUG
			double t0 = ltr_gettime();
#endif
			
			for( size_t mi_id = 0; mi_id < m_meshInstances.size(); ++mi_id )
			{
				ltr_MeshInstance* mi = m_meshInstances[ mi_id ];
				for( size_t i = 0; i < mi->m_lightmap.size(); ++i )
				{
					ltr_Mesh* mesh = mi->mesh;
					const Vec3& P = mi->m_samples_pos[ i ];
					const Vec3& N = mi->m_samples_nrm[ i ].Normalized();
					
					ltr_RadSampleGeom RSG = { P, N };
					m_radSampleGeoms.push_back( RSG );
					
					Vec3 s_diff = {1,1,1};
					Vec3 s_emit = mi->m_lightmap[ i ];
					float s_area = mi->m_samplecont ? 0 : mi->m_samples_radinfo[ i ].w;
					if( mi->m_samplecont )
					{
						s_diff = Vec3::Create(0);
					}
					else if( config.sample_fn )
					{
						const size_t L = mi->m_samples_loc[ i ];
						int LMX = L % mi->lm_width;
						int LMY = L / mi->lm_width;
						ltr_SampleRequest REQ =
						{
							{ P.x, P.y, P.z },
							{ N.x, N.y, N.z },
							mi->m_samples_radinfo[ i ].x, mi->m_samples_radinfo[ i ].y,
							( LMX + 0.5f ) / mi->lm_width, ( LMY + 0.5f ) / mi->lm_height,
							(uint32_t) mi->m_samples_radinfo[ i ].z, // should not be interpolated between triangles
							mesh->m_ident.c_str(), mesh->m_ident.size(),
							mi->m_ident.c_str(), mi->m_ident.size(),
							{ 1, 1, 1 },
							{ 0, 0, 0 },
						};
						if( config.sample_fn( &config, &REQ ) )
						{
							s_diff = Vec3::CreateFromPtr( REQ.out_diffuse_color );
							s_emit += Vec3::CreateFromPtr( REQ.out_emissive_color );
						}
					}
					
					ltr_RadSampleColors RSC = { s_diff, s_emit, s_emit, Vec3::Create(0), s_area };
					m_radSampleColors.push_back( RSC );
				}
			}
			
#ifdef LTRDEBUG
			double t1 = ltr_gettime();
			printf( "RAD sample concat: %g seconds\n", t1 - t0 );
			
			double t2 = ltr_gettime();
#endif
			
			for( size_t i = 0; i < m_radSampleGeoms.size(); ++i )
			{
				m_radLinkMap.push_back( m_radLinks.size() ); // offset
				m_radLinkMap.push_back( 0 ); // count
				
				const Vec3& A_SP = m_radSampleGeoms[ i ].pos;
				const Vec3& A_SN = m_radSampleGeoms[ i ].normal;
				for( size_t j = i + 1; j < m_radSampleGeoms.size(); ++j )
				{
					const Vec3& B_SP = m_radSampleGeoms[ j ].pos;
					const Vec3& B_SN = m_radSampleGeoms[ j ].normal;
					
					Vec3 posdiff = B_SP - A_SP;
					float dotA = Vec3Dot( A_SN, posdiff );
					float dotB = Vec3Dot( B_SN, -posdiff );
					if( dotA <= SMALL_FLOAT || dotB <= SMALL_FLOAT )
						continue;
					
					float lensq = posdiff.LengthSq();
					float factor = dotA * dotB / ( lensq * lensq * (float) M_PI );
					if( factor < SMALL_FLOAT )
						continue;
					
					if( VisibilityTest( A_SP, B_SP ) < 1.0f )
						continue;
					
					// add sample
					ltr_RadLink RL = { (uint32_t) j, factor };
					m_radLinks.push_back( RL );
					m_radLinkMap.back()++;
				}
			}
			
#ifdef LTRDEBUG
			double t3 = ltr_gettime();
			printf( "RAD sample processing: %g seconds\n", t3 - t2 );
#endif
		}
		break;
		
	case LTR_WT_RDBOUNCE:
		for( size_t i = 0; i < m_radLinkMap.size() / 2; ++i )
		{
			size_t sidbegin = m_radLinkMap[ i*2+0 ];
			size_t sidend = sidbegin + m_radLinkMap[ i*2+1 ];
			for( size_t sid = sidbegin; sid < sidend; ++sid )
			{
				size_t j = m_radLinks[ sid ].other;
				float f = m_radLinks[ sid ].factor;
				
				ltr_RadSampleColors& A_RSC = m_radSampleColors[ i ];
				ltr_RadSampleColors& B_RSC = m_radSampleColors[ j ];
				
				Vec3 A_out = ( A_RSC.outputEnergy * A_RSC.diffuseColor ) * A_RSC.area * f;
				Vec3 B_out = ( B_RSC.outputEnergy * B_RSC.diffuseColor ) * B_RSC.area * f;
				
				A_RSC.totalLight += B_out;
				B_RSC.totalLight += A_out;
				
				A_RSC.inputEnergy += B_out;
				B_RSC.inputEnergy += A_out;
			}
		}
		for( size_t i = 0; i < m_radSampleColors.size(); ++i )
		{
			ltr_RadSampleColors& RSC = m_radSampleColors[ i ];
			RSC.outputEnergy = RSC.inputEnergy;
			RSC.inputEnergy = Vec3::Create(0);
		}
		break;
		
	case LTR_WT_RDCOMMIT:
		{
			size_t sp = 0;
			for( size_t mi_id = 0; mi_id < m_meshInstances.size(); ++mi_id )
			{
				ltr_MeshInstance* mi = m_meshInstances[ mi_id ];
				for( size_t i = 0; i < mi->m_lightmap.size(); ++i )
				{
					mi->m_lightmap[ i ] = m_radSampleColors[ sp++ ].totalLight;
				}
			}
		}
		break;
		
	case LTR_WT_AORENDER:
		{
			ltr_MeshInstance* mi = m_meshInstances[ m_workPart ];
		//	float ao_divergence = config.ao_divergence * 0.5f + 0.5f;
			float ao_distance = config.ao_distance,
				ao_falloff = config.ao_falloff,
				ao_multiplier = config.ao_multiplier,
				ao_effect = config.ao_effect;
			int num_samples = config.ao_num_samples;
			Vec3 ao_color = Vec3::CreateFromPtr( config.ao_color_rgb );
			
			for( size_t i = 0; i < mi->m_lightmap.size(); ++i )
			{
				Vec3& SP = mi->m_samples_pos[ i ];
				Vec3& SN = mi->m_samples_nrm[ i ];
				Vec3& OutColor = mi->m_lightmap[ i ];
				
				Vec3 ray_origin = SP + SN * ( SMALL_FLOAT * 2 );
				
				float ao_factor = 0;
				float randoff = randf();
				for( int s = 0; s < num_samples; ++s )
				{
					Vec3 ray_dir = Vec3::CreateSpiralDirVector( SN, randoff, s, num_samples ) * ao_distance;
					float hit = VisibilityTest( ray_origin, ray_origin + ray_dir );
					if( hit < 1.0f )
						ao_factor += 1.0f - hit;
				}
				ao_factor /= num_samples;
				ao_factor = TMIN( ao_factor * ao_multiplier, 1.0f );
				if( ao_falloff )
					ao_factor = pow( ao_factor, ao_falloff );
				
				if( ao_effect >= 0 )
				{
					OutColor = OutColor * ( 1 - ao_factor * ( 1 - ao_effect ) ) + ao_color * ao_factor;
				}
				else
				{
					OutColor = TLERP( OutColor, TLERP( ao_color, ao_color * OutColor, -ao_effect ), ao_factor );
				}
			}
		}
		break;
		
	case LTR_WT_FINALIZE:
		{
			ltr_MeshInstance* mi = m_meshInstances[ m_workPart ];
			if( mi->m_samplecont )
			{
				for( size_t i = 0; i < mi->m_lightmap.size(); ++i )
				{
					m_samples[ i ].out_color[0] = mi->m_lightmap[ i ].x;
					m_samples[ i ].out_color[1] = mi->m_lightmap[ i ].y;
					m_samples[ i ].out_color[2] = mi->m_lightmap[ i ].z;
				}
				break;
			}
			ltr_Mesh* mesh = mi->mesh;
			
			float* image_rgb = new float[ mi->lm_width * mi->lm_height * 3 ];
			char* image_set = new char[ mi->lm_width * mi->lm_height * 2 ];
			TMEMSET( image_rgb, mi->lm_width * mi->lm_height * 3, 0.0f );
			TMEMSET( image_set, mi->lm_width * mi->lm_height * 2, (char) 0 );
			for( size_t i = 0; i < mi->m_samples_loc.size(); ++i )
			{
				size_t loc = mi->m_samples_loc[ i ] * 3;
				Vec3& LMC = mi->m_lightmap[ i ];
				image_rgb[ loc+0 ] = LMC.x;
				image_rgb[ loc+1 ] = LMC.y;
				image_rgb[ loc+2 ] = LMC.z;
				image_set[ mi->m_samples_loc[ i ] ] = 1;
			}
			
			// extrapolate for interpolation
			u32 w = mi->lm_width, h = mi->lm_height;
#define PX_ISSET( x, y ) ( (x) < w && (y) < h && image_set[ (x) + (y) * w ] )
#define PX_GETCOL( x, y ) Vec3::Create(image_rgb[((x)+(y)*w)*3+0],image_rgb[((x)+(y)*w)*3+1],image_rgb[((x)+(y)*w)*3+2])
			for( int its = 1; its <= 3; ++its )
			{
				memcpy( image_set + w*h, image_set, w*h );
				for( u32 y = 0; y < h; ++y )
				{
					for( u32 x = 0; x < w; ++x )
					{
						if( image_set[ x + y * w ] )
							continue;
						
						Vec3 col = {0,0,0};
						int count = 0;
						// H
						if( PX_ISSET( x-1, y ) )
						{
							if( PX_ISSET( x-2, y ) )
								col += TLERP( PX_GETCOL( x-2, y ), PX_GETCOL( x-1, y ), 2.0f );
							else
								col += PX_GETCOL( x-1, y );
							count++;
						}
						if( PX_ISSET( x+1, y ) )
						{
							if( PX_ISSET( x+2, y ) )
								col += TLERP( PX_GETCOL( x+2, y ), PX_GETCOL( x+1, y ), 2.0f );
							else
								col += PX_GETCOL( x+1, y );
							count++;
						}
						// V
						if( PX_ISSET( x, y-1 ) )
						{
							if( PX_ISSET( x, y-2 ) )
								col += TLERP( PX_GETCOL( x, y-2 ), PX_GETCOL( x, y-1 ), 2.0f );
							else
								col += PX_GETCOL( x, y-1 );
							count++;
						}
						if( PX_ISSET( x, y+1 ) )
						{
							if( PX_ISSET( x, y+2 ) )
								col += TLERP( PX_GETCOL( x, y+2 ), PX_GETCOL( x, y+1 ), 2.0f );
							else
								col += PX_GETCOL( x, y+1 );
							count++;
						}
						
						if( count )
						{
							col /= (float) count;
							u32 off = ( x + y * w ) * 3;
							image_rgb[ off+0 ] = TMAX( col.x, 0.0f );
							image_rgb[ off+1 ] = TMAX( col.y, 0.0f );
							image_rgb[ off+2 ] = TMAX( col.z, 0.0f );
							image_set[ x + y * w + w*h ] = 1;
						}
					}
				}
				memcpy( image_set, image_set + w*h, w*h );
			}
#undef PX_ISSET
#undef PX_GETCOL
			
			if( config.blur_size )
			{
				int blur_ext = (int) ceil( config.blur_size );
				int blurbuf_size = ( TMAX( mi->lm_width, mi->lm_height ) + blur_ext * 2 ) * 3;
				int kernel_size = blur_ext * 2 + 1;
				float* image_tmp_rgb = new float[ mi->lm_width * mi->lm_height * 3 + blurbuf_size + kernel_size ];
				float* blur_tmp_rgb = image_tmp_rgb + mi->lm_width * mi->lm_height * 3;
				float* kernel_rgb = blur_tmp_rgb + blurbuf_size;
				
				Generate_Gaussian_Kernel( kernel_rgb, blur_ext, config.blur_size );
				Convolve_Transpose( image_rgb, image_tmp_rgb, mi->lm_width, mi->lm_height, blur_ext, kernel_rgb, blur_tmp_rgb );
				Convolve_Transpose( image_tmp_rgb, image_rgb, mi->lm_height, mi->lm_width, blur_ext, kernel_rgb, blur_tmp_rgb );
				
				delete [] image_tmp_rgb;
			}
			
			ltr_WorkOutput wo =
			{
				m_workPart,
				mesh->m_ident.c_str(),
				mesh->m_ident.size(),
				mi->m_ident.c_str(),
				mi->m_ident.size(),
				image_rgb,
				mi->lm_width,
				mi->lm_height
			};
			
			m_workOutput.push_back( wo );
		}
		break;
	}
}

LTRCODE ltr_Scene::Advance()
{
	u32 count = m_meshInstances.size();
	if( m_workType == LTR_WT_LMRENDER )
		count *= m_lights.size();
	if( m_workType == LTR_WT_RDGENLNK || m_workType == LTR_WT_RDCOMMIT )
		count = 1;
	if( m_workType == LTR_WT_RDBOUNCE )
		count = config.bounce_count;
	
	m_workPart++;
	if( m_workPart >= count )
	{
		if( m_workType == LTR_WT_FINALIZE && m_workPart >= m_meshInstances.size() )
			return LTRC_ATEND;
		
		m_workType++;
		m_workPart = 0;
		
		if( ( config.bounce_count == 0 &&
			( m_workType == LTR_WT_RDGENLNK
			|| m_workType == LTR_WT_RDBOUNCE
			|| m_workType == LTR_WT_RDCOMMIT ) ) ||
			( m_workType == LTR_WT_AORENDER && config.ao_distance == 0 ) )
		{
			m_workType++;
		}
	}
	
	return LTRC_SUCCESS;
}

LTRCODE ltr_DoWork( ltr_Scene* scene, ltr_WorkInfo* info )
{
	scene->DoWork();
	
	info->type = scene->m_workType;
	info->part = scene->m_workPart + 1;
	switch( scene->m_workType )
	{
	case LTR_WT_PREXFORM:
		info->stage = "transforming spatial data";
		info->item_count = scene->m_meshInstances.size();
		break;
	case LTR_WT_COLINFO:
		info->stage = "generating data structures";
		info->item_count = scene->m_meshInstances.size();
		break;
	case LTR_WT_SAMPLES:
		info->stage = "generating samples";
		info->item_count = scene->m_meshInstances.size();
		break;
	case LTR_WT_LMRENDER:
		info->stage = "rendering lightmaps";
		info->item_count = scene->m_meshInstances.size() * scene->m_lights.size();
		break;
	case LTR_WT_RDGENLNK:
		info->stage = "calculating radiosity";
		info->item_count = 1;
		break;
	case LTR_WT_RDBOUNCE:
		info->stage = "bouncing light";
		info->item_count = scene->config.bounce_count;
		break;
	case LTR_WT_RDCOMMIT:
		info->stage = "committing radiosity";
		info->item_count = 1;
		break;
	case LTR_WT_AORENDER:
		info->stage = "rendering ambient occlusion";
		info->item_count = scene->m_meshInstances.size();
		break;
	case LTR_WT_FINALIZE:
		info->stage = "exporting lightmaps";
		info->item_count = scene->m_meshInstances.size();
		break;
	default:
		info->stage = "unknown";
		info->item_count = 1;
		break;
	}
	
	return scene->Advance();
}

void ltr_GetConfig( ltr_Config* cfg, ltr_Scene* opt_scene )
{
	if( opt_scene )
	{
		memcpy( cfg, &opt_scene->config, sizeof(*cfg) );
		return;
	}
	
	cfg->userdata = NULL;
	cfg->size_fn = ltr_DefaultSizeFunc;
	
	cfg->max_tree_memory = 128 * 1024 * 1024;
	cfg->max_lightmap_size = 1024;
	cfg->default_width = 64;
	cfg->default_height = 64;
	cfg->global_size_factor = 4;
	cfg->max_correct_dist = 0.1f;
	cfg->max_correct_angle = 60;
	
	LTR_VEC3_SET( cfg->clear_color, 0, 0, 0 );
	LTR_VEC3_SET( cfg->ambient_color, 0, 0, 0 );
	
	cfg->bounce_count = 0;
	cfg->sample_fn = NULL;
	
	cfg->ao_distance = 0;
	cfg->ao_multiplier = 1.2f;
	cfg->ao_falloff = 1;
	cfg->ao_effect = 0;
	cfg->ao_divergence = 0;
	LTR_VEC3_SET( cfg->ao_color_rgb, 0, 0, 0 );
	cfg->ao_num_samples = 17;
	
	cfg->blur_size = 0.5f;
}

LTRCODE ltr_SetConfig( ltr_Scene* scene, ltr_Config* cfg )
{
	memcpy( &scene->config, cfg, sizeof(*cfg) );
	return 1;
}


ltr_Mesh* ltr_CreateMesh( ltr_Scene* scene, const char* ident, size_t ident_size )
{
	ltr_Mesh* mesh = new ltr_Mesh( scene );
	if( ident )
		mesh->m_ident.assign( ident, ident_size );
	scene->m_meshes.push_back( mesh );
	return mesh;
}

LTRBOOL ltr_MeshAddPart( ltr_Mesh* mesh, ltr_MeshPartInfo* mpinfo )
{
	ltr_MeshPart mp =
	{
		mpinfo->vertex_count,
		(u32) mesh->m_vpos.size(),
		mpinfo->index_count,
		(u32) mesh->m_indices.size(),
		mpinfo->tristrip
	};
	
	if( mpinfo->index_count < 3 && ( mpinfo->tristrip || mpinfo->index_count % 3 != 0 ) )
		return 0;
	
	size_t newsize = mesh->m_vpos.size() + mpinfo->vertex_count;
	mesh->m_vpos.resize( newsize );
	mesh->m_vnrm.resize( newsize );
	mesh->m_vtex1.resize( newsize );
	mesh->m_vtex2.resize( newsize );
	mesh->m_indices.resize( mesh->m_indices.size() + mpinfo->index_count );
	
	Vec3* vpos_ptr = &mesh->m_vpos[ mp.m_vertexOffset ];
	Vec3* vnrm_ptr = &mesh->m_vnrm[ mp.m_vertexOffset ];
	Vec2* vtex1_ptr = &mesh->m_vtex1[ mp.m_vertexOffset ];
	Vec2* vtex2_ptr = &mesh->m_vtex2[ mp.m_vertexOffset ];
	u32* iptr = &mesh->m_indices[ mp.m_indexOffset ];
	
	for( u32 i = 0; i < mp.m_vertexCount; ++i )
	{
		vpos_ptr[ i ] = *(Vec3*)((char*) mpinfo->positions_f3 + mpinfo->stride_positions * i );
		vnrm_ptr[ i ] = *(Vec3*)((char*) mpinfo->normals_f3 + mpinfo->stride_normals * i );
		vtex1_ptr[ i ] = *(Vec2*)((char*) mpinfo->texcoords1_f2 + mpinfo->stride_texcoords1 * i );
		vtex2_ptr[ i ] = *(Vec2*)((char*) mpinfo->texcoords2_f2 + mpinfo->stride_texcoords2 * i );
	}
	memcpy( iptr, mpinfo->indices, sizeof(*iptr) * mpinfo->index_count );
	
	mesh->m_parts.push_back( mp );
	return 1;
}

LTRBOOL ltr_MeshAddInstance( ltr_Mesh* mesh, ltr_MeshInstanceInfo* mii )
{
	ltr_MeshInstance* mi = new ltr_MeshInstance;
	mi->mesh = mesh;
	mi->m_importance = mii->importance;
	mi->m_shadow = !!mii->shadow;
	mi->m_samplecont = false;
	if( mii->ident )
		mi->m_ident.assign( mii->ident, mii->ident_size );
	memcpy( mi->matrix.a, mii->matrix, sizeof(Mat4) );
	mesh->m_scene->m_meshInstances.push_back( mi );
	mi->lm_width = 128;
	mi->lm_height = 128;
	return 1;
}

void ltr_LightAdd( ltr_Scene* scene, ltr_LightInfo* li )
{
	ltr_Light L =
	{
		li->type,
		Vec3::CreateFromPtr( li->position ),
		Vec3::CreateFromPtr( li->direction ).Normalized(),
		Vec3::CreateFromPtr( li->up_direction ).Normalized(),
		Vec3::CreateFromPtr( li->color_rgb ),
		li->range,
		li->power,
		li->light_radius,
		TMAX( 1, li->shadow_sample_count ),
		li->spot_angle_out,
		li->spot_angle_in,
		li->spot_curve,
	};
	scene->m_lights.push_back( L );
}

void ltr_SampleAdd( ltr_Scene* scene, ltr_SampleInfo* si )
{
	ltr_SampleInfo S = *si;
	S.out_color[0] = 0;
	S.out_color[1] = 0;
	S.out_color[2] = 0;
	scene->m_sampleMI.m_samples_pos.push_back( Vec3::CreateFromPtr( S.position ) );
	scene->m_sampleMI.m_samples_nrm.push_back( Vec3::CreateFromPtr( S.normal ) );
	scene->m_sampleMI.m_lightmap.push_back( Vec3::Create(0,0,0) );
	scene->m_samples.push_back( S );
}


void ltr_GetWorkOutputInfo( ltr_Scene* scene, ltr_WorkOutputInfo* woutinfo )
{
	woutinfo->lightmap_count = scene->m_workOutput.size();
	woutinfo->sample_count = scene->m_samples.size();
	woutinfo->samples = &scene->m_samples[0];
}

LTRBOOL ltr_GetWorkOutput( ltr_Scene* scene, u32 which, ltr_WorkOutput* wout )
{
	if( which < 0 || which >= scene->m_workOutput.size() )
		return 0;
	*wout = scene->m_workOutput[ which ];
	return 1;
}


u32 ltr_NextPowerOfTwo( u32 x )
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}


