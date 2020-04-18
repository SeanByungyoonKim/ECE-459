#include "Simulation.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>

using std::cerr;
using std::cout;
using std::endl;

#define P 1.0
#define E -1.0
// #define USE_OPENCL

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------
bool retIsEmpty(const std::vector<Particle*> &ret) {
    for (Particle* p : ret) {
        if (p != nullptr) {
            return false;
        }
    }
    return true;
}
void reset(std::vector<Particle*> &v) {
    for (int i = 0; i < v.size(); i++) {
        delete v[i];
        v[i] = nullptr;
    }
}

//-----------------------------------------------------------------------------
// Simulation
//-----------------------------------------------------------------------------
Simulation::Simulation(float initTimeStep, float errorTolerance)
    : initTimeStep(initTimeStep)
    , errorTolerance(errorTolerance) {
    // nop
}
Simulation::~Simulation() {
    reset(y0);
    reset(y1);
    reset(z1);
}

//-----------------------------------------------------------------------------
// Input/output
//-----------------------------------------------------------------------------
 #ifndef USE_OPENCL
void Simulation::readInputFile(std::string inputFile) {
    std::ifstream file(inputFile);
    if (!file.is_open()) {
        cerr << "Unable to open file: " << inputFile << endl;
        exit(1);
    }
    std::string line;
    while (getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        getline(ss, token, ',');
        Particle::ParticleType type = (token == "p" ? Particle::PROTON : Particle::ELECTRON);
        getline(ss, token, ',');
        float x = std::stod(token);

        getline(ss, token, ',');
        float y = std::stod(token);

        getline(ss, token, ',');
        float z = std::stod(token);

        y0.push_back(new Particle(
            type,
            Vec3(x, y, z)
        ));
    }
}
#endif

#ifdef USE_OPENCL
#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
std::vector<cl_float4> y0_cl;

void Simulation::readInputFile(std::string inputFile) {
    std::ifstream file(inputFile);
    if (!file.is_open()) {
        cerr << "Unable to open file: " << inputFile << endl;
        exit(1);
    }
    std::string line;
    while (getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        getline(ss, token, ',');
        float type = (token == "p" ? P : E);

        getline(ss, token, ',');
        float x = std::stod(token);

        getline(ss, token, ',');
        float y = std::stod(token);

        getline(ss, token, ',');
        float z = std::stod(token);
        cl_float4 part = {type, x, y, z}; 
        y0_cl.push_back(part);
    }
}
#endif

#ifndef USE_OPENCL
void Simulation::print() {
    int digitsAfterDecimal = 5;
    int width = digitsAfterDecimal + std::string("-0.e+00").length();
    for (const Particle* p : z1) {
        char type;
        switch (p->type) {
            case Particle::PROTON:
                type = 'p';
                break;
            case Particle::ELECTRON:
                type = 'e';
                break;
            default:
                type = 'u';
        }
        cout << type << ","
             << std::scientific
             << std::setprecision(digitsAfterDecimal)
             << std::setw(width)
             << p->position.x << ","
             << std::setw(width)
             << p->position.y << ","
             << std::setw(width)
             << p->position.z << endl;
    }
}
#endif
#ifdef USE_OPENCL

void Simulation::print() {
    printf("potaot\n");
}
#endif

//-----------------------------------------------------------------------------
// OpenCL Simulation
//-----------------------------------------------------------------------------
#ifdef USE_OPENCL
#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>
void Simulation::run() {
    float h = initTimeStep; 
    float tol = errorTolerance;
    int num_part = y0_cl.size();
    printf("num_part: %d\n", num_part); 
    // convert vector to array to transfer over as buffer 
    cl_float4 arr[num_part];
    for (int i = 0; i < num_part; i++) {
        arr[i] = y0_cl[i];
    }
    try { 
        // Get available platforms
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        // Select the default platform and create a context using this platform and the GPU
        cl_context_properties cps[3] = { 
            CL_CONTEXT_PLATFORM, 
            (cl_context_properties)(platforms[0])(), 
            0 
        };
        cl::Context context(CL_DEVICE_TYPE_GPU, cps);
 
        // Get a list of devices on this platform
        std::vector<cl::Device> devices = context.getInfo<CL_CONTEXT_DEVICES>();
 
        // Create a command queue and use the first device
        cl::CommandQueue queue = cl::CommandQueue(context, devices[0]);
 
        // Read source file
        std::ifstream sourceFile("src/protons.cl");
            if(!sourceFile.is_open()){
                std::cerr << "Cannot find kernel file" << std::endl;
                throw;
            }
        std::string sourceCode(std::istreambuf_iterator<char>(sourceFile), (std::istreambuf_iterator<char>()));
        cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length()+1));
 
        // Make program of the source code in the context
        cl::Program program = cl::Program(context, source);
 
        // Build program for these specific devices
        try {
            program.build(devices);
        } catch(cl::Error error) {
            std::cerr << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0]) << std::endl;
            throw;
        }
        bool main_flag = false;
 
        // Make kernels
        cl::Kernel kernel_k0(program, "calc_k0"); 
        cl::Kernel kernel_y1(program, "calc_y1");
        cl::Kernel kernel(program, "simulation");
        cl::Buffer y0_buf = cl::Buffer(context, CL_MEM_READ_ONLY,num_part*sizeof(cl_float4)); 
        cl::Buffer k0_buf = cl::Buffer(context, CL_MEM_READ_WRITE, num_part*sizeof(cl_float3)); 
        queue.enqueueWriteBuffer(y0_buf, CL_TRUE, 0, num_part*sizeof(cl_float4), arr);

        kernel_k0.setArg(0, y0_buf); 
        kernel_k0.setArg(1, k0_buf); 
        kernel_k0.setArg(2, num_part); 

        cl::NDRange globalSize(num_part); 
        queue.enqueueNDRangeKernel(kernel_k0, cl::NullRange, globalSize); 
        cl_float3* k0_calc = new cl_float3[num_part]; 
        queue.enqueueReadBuffer(k0_buf, CL_TRUE, 0, num_part*sizeof(cl_float3), k0_calc);
 
        while (!main_flag) {
            // Create buffers
            cl::Buffer k1_buf = cl::Buffer(context, CL_MEM_READ_WRITE, num_part*sizeof(cl_float3));
            cl::Buffer y1_buf = cl::Buffer(context, CL_MEM_READ_WRITE, num_part*sizeof(cl_float4));
            cl::Buffer z1_buf = cl::Buffer(context, CL_MEM_READ_WRITE, num_part*sizeof(cl_float4));
            cl::Buffer error_buf = cl::Buffer(context, CL_MEM_WRITE_ONLY, num_part*sizeof(bool));

            // Write buffers
            queue.enqueueWriteBuffer(y0_buf, CL_TRUE, 0, num_part*sizeof(cl_float4), arr);
            queue.enqueueWriteBuffer(k0_buf, CL_TRUE, 0, num_part*sizeof(cl_float3), k0_calc);

            // Set arguments to kernel
            kernel_y1.setArg(0, y0_buf); 
            kernel_y1.setArg(1, k0_buf); 
            kernel_y1.setArg(2, y1_buf);
            kernel_y1.setArg(3, h);
            kernel_y1.setArg(4, num_part);

            cl::NDRange y1_size(num_part);
            queue.enqueueNDRangeKernel(kernel_y1, cl::NullRange, y1_size);

            cl_float4* y1_calc = new cl_float4[num_part]; 
            queue.enqueueReadBuffer(y1_buf, CL_TRUE, 0, num_part*sizeof(cl_float4), y1_calc);  
            queue.enqueueWriteBuffer(y1_buf, CL_TRUE, 0, num_part*sizeof(cl_float4), y1_calc);

            kernel.setArg(0, y0_buf); 
            kernel.setArg(1, k0_buf); 
            kernel.setArg(2, k1_buf); 
            kernel.setArg(3, y1_buf); 
            kernel.setArg(4, z1_buf);
            kernel.setArg(5, h);
            kernel.setArg(6, tol);
            kernel.setArg(7, num_part);
            kernel.setArg(8, error_buf);

            // Run the kernel on specific ND range
            cl::NDRange z1_size(num_part);
            queue.enqueueNDRangeKernel(kernel, cl::NullRange, z1_size);      
     
            // Read buffer(s)
            cl_float4* read_arr = new cl_float4[num_part]; 
            queue.enqueueReadBuffer(z1_buf, CL_TRUE, 0, num_part*sizeof(cl_float4),read_arr); 
            bool* error = new bool[num_part]; 
            queue.enqueueReadBuffer(error_buf, CL_TRUE, 0, num_part*sizeof(bool), error); 

            main_flag = true;
            for (int i = 0; i < num_part; i++) {
                if (!error[i]) {
                    main_flag = false;
                    h /= 2.0;
                    break;
                }
            }

            free(y1_calc);
            free(error);

            if (main_flag) {
                printf("we done yay\n");
                for (int i = 0; i < num_part; i++) {
                    printf("j: %d, A: %f, x: %0.10f, y: %0.10f, z: %0.10f\n", i, read_arr[i].s[1], read_arr[i].s[2], read_arr[i].s[3], read_arr[i].s[4]); 
                }
                free(read_arr);
                break;
            }
        }
        free(k0_calc);
        
    } catch(cl::Error error) {
        std::cout << error.what() << "(" << error.err() << ")" << std::endl;
    }
}
#endif

//-----------------------------------------------------------------------------
// CPU Simulation
//-----------------------------------------------------------------------------

#ifndef USE_OPENCL

void Simulation::computeForces(std::vector<Vec3> &ret, const std::vector<Particle*> &particles) {
    assert(ret.size() == 0);
    ret.resize(particles.size());
    #pragma omp parallel for
    for (int i = 0; i < particles.size(); i++) {
        Vec3 totalForces;
        for (int j = 0; j < particles.size(); j++) {
            totalForces += particles[i]->computeForceOnMe(particles[j]);
        }
        ret[i] = totalForces;
    }
    assert(ret.size() == particles.size());
}
void Simulation::computeApproxPositions(const float h) {
    assert(y0.size() == k0.size());
    assert(retIsEmpty(y1));
    #pragma omp parallel for
    for (int i = 0; i < y0.size(); i++) {
        float mass = y0[i]->getMass();
        Vec3 f = k0[i];
        // h's unit is in seconds
        //
        //         F = ma
        //     F / m = a
        //   h F / m = v
        // h^2 F / m = d
        Vec3 deltaDist = f * std::pow(h, 2) / mass;
        y1[i] = new Particle(y0[i]->type, y0[i]->position + deltaDist);
    }
}
void Simulation::computeBetterPositions(const float h) {
    assert(y0.size() == k0.size());
    assert(y0.size() == k1.size());
    assert(retIsEmpty(z1));
    #pragma omp parallel for
    for (int i = 0; i < y0.size(); i++) {
        float mass = y0[i]->getMass();
        Vec3 f0 = k0[i];
        Vec3 f1 = k1[i];
        Vec3 avgForce = (f0 + f1) / 2.0;
        Vec3 deltaDist = avgForce * std::pow(h, 2) / mass;
        Vec3 y1 = y0[i]->position + deltaDist;
        z1[i] = new Particle(y0[i]->type, y1);
    }
}
bool Simulation::isErrorAcceptable(const std::vector<Particle*> &p0, const std::vector<Particle*> &p1) {
    assert(p0.size() == p1.size());
    bool errorAcceptable = true;
    #pragma omp parallel for
    for (int i = 0; i < p0.size(); i++) {
        if ((p0[i]->position - p1[i]->position).magnitude() > errorTolerance) {
            #pragma omp critical
            {
                errorAcceptable = false;
            }
        }
    }
    return errorAcceptable;
}
void Simulation::run() {
    const int numParticles = y0.size();
    k0.reserve(numParticles);
    k1.reserve(numParticles);
    y1.resize(numParticles);
    z1.resize(numParticles);
    float h = initTimeStep;
    k0.clear();
    computeForces(k0, y0); // Compute k0
    while (true) {
        k1.clear();
        reset(y1);
        reset(z1);
        computeApproxPositions(h); // Compute y1
        computeForces(k1, y1); // Compute k1
        computeBetterPositions(h); // Compute z1
        if (isErrorAcceptable(z1, y1)) {
            // Error is acceptable so we can stop simulation
            break;
        }
        h /= 2.0;
    }
}
#endif