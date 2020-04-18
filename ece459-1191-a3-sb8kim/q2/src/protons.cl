// 1.0 = p, -1.0 = e, 0.0 = u for particle type in float4 w value
#define P 1.0
#define E -1.0
#define U 0	

#define COULOMB_C (8.99 * pow((float)10, (float)9))
#define CHARGE_P 1.60217662 * pow((float)10, (float)-19)
#define CHARGE_E -CHARGE_P
#define MASS_P 1.67262190 * pow((float)10, (float)-27)
#define MASS_E 9.10938356 * pow((float)10, (float)-31)


float magnitude(float3 vec) {
	return sqrt(pow(vec.x,2) + pow(vec.y,2) + pow(vec.z,2)); 
}

float3 normal(float3 vec) {
	float factor = 1.0/magnitude(vec);
	return vec*factor;
}

float get_charge(float4 particle) {
	if (particle.w > 0) {
		return CHARGE_P;
	}
	else if (particle.w < 0) {
		return CHARGE_E;
	}
	return 0;
}

float get_mass(float4 particle) {
	if (particle.w > 0) {
		return MASS_P;
	}
	else if (particle.w < 0) {
		return MASS_E;
	}
	return 0;
}

float3 particle_force_on_me (const float4 particle1, const float4 particle2) {
	float3 force; 
	if (particle2.w > 0) {
		force.x = 0;
		force.y = 0; 
		force.z = 0;
		return force;
	}
	if (particle1.x == particle2.x && particle1.y == particle2.y && particle1.z == particle2.z && particle1.w == particle2.w) {
		force.x = 0; 
		force.y = 0;
		force.z = 0; 
		return force; 
	}

	float3 position1 = (float3)(particle1.x, particle1.y, particle1.z); 
	float3 position2 = (float3)(particle2.x, particle2.y, particle2.z); 

	float3 direction = position1 - position2; 

	float q1 = get_charge(particle1);
	float q2 = get_charge(particle2);
	float r = magnitude(direction);

	return normal(direction) * (float)(COULOMB_C * q1 * q2 / pow(r,2));
}

float3 compute_forces(__global const float4* parts, const int size, const int i) {
	float3 total_forces = (float3)(0.0f, 0.0f, 0.0f);
	for (int j = 0; j < size; j++)
		total_forces += particle_force_on_me(parts[i], parts[j]);
	return total_forces;
}

// Calculate y1 position
float4 compute_approx_pos(const float h, float4 y0, float3 k0, const int num_part) {
	float mass = get_mass(y0); 
	float3 f = k0;

	float3 delta_dist = f + pow(h,2)/mass;
	float3 y0_pos = (float3)(y0.x, y0.y, y0.z); 
	float4 y1 = (float4)(y0.w, (float3)(y0_pos+delta_dist)); 
	return y1;
}

float4 compute_better_pos(const float h, float4 y0, float3 k0, float3 k1) {
	float mass = get_mass(y0);

	float3 avg_force = (k0+k1)/(float)2.0;
	float3 delta_dist = avg_force*pow(h,2)/mass; 
	float3 y0_pos = (float3)(y0.x, y0.y, y0.z);
	float3 z1_pos = y0_pos + delta_dist; 
	float4 z1 = (float4)(y0.w, z1_pos); 
	return z1; 
}

bool is_error_acceptable(const float4 particle1, const float4 particle2, const float tolerance) {
	bool accept = true;
	float3 p1_pos = (float3)(particle1.x, particle1.y, particle1.z); 
	float3 p2_pos = (float3)(particle2.x, particle2.y, particle2.z); 

	float pos_magnitude = magnitude(p1_pos-p2_pos);
	if (pos_magnitude > tolerance)
		accept = false; 
	return accept;  
}

__kernel void calc_k0(__global const float4* particles, __global float3* k0, const int num_part) {
	int i = get_global_id(0); 
	k0[i] = compute_forces(particles, num_part, i);
}

__kernel void calc_y1(__global const float4* particles, __global const float3* k0, __global float4* y1, const float h, const int num_part) {
	int i = get_global_id(0); 
	y1[i] = compute_approx_pos(h, particles[i], k0[i], num_part);
}

__kernel void simulation(__global const float4* particles, __global float3* k0, __global float3* k1, __global float4* y1, __global float4* z1, const float h, const float tolerance, const int num_part, __global bool* error_flags) {
	int i = get_global_id(0);   
	k1[i] = compute_forces(y1, num_part, i); 
	z1[i] = compute_better_pos(h, particles[i], k0[i], k1[i]); 
	error_flags[i] = is_error_acceptable(z1[i], y1[i], tolerance);
}
