#define __CL_ENABLE_EXCEPTIONS

#include <CL/cl.hpp>

#include <string>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>


#include <base64/base64.h>

using std::cout;
using std::cerr;
using std::endl;

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

int gMaxSecretLen = 4;

std::string gAlphabet = "abcdefghijklmnopqrstuvwxyz"
                        "0123456789";

using namespace std;

#define BLOCK_BITS      512 // from jwtcracker.cl 
#define BLOCK_BYTES     (BLOCK_BITS / 8)
#define HASH_BITS       256
#define HASH_BYTES      (HASH_BITS / 8) // 32

#define LIMIT 7500

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

string secrets[LIMIT];
char secret_buf[LIMIT*6];
int secret_count = 0;

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

void usage(const char *cmd) {
    cout << cmd << " <token> [maxLen] [alphabet]" << endl;
    cout << endl;

    cout << "Defaults:" << endl;
    cout << "maxLen = " << gMaxSecretLen << endl;
    cout << "alphabet = " << gAlphabet << endl;
}

bool isValidSecret(string &message, string &origSig, cl::Context context, cl::CommandQueue queue, cl::Kernel kernel, int secretLen, int limit, string &ans) {
    int messageLen = message.size();

    cl::Buffer secretBuffer = cl::Buffer(context, CL_MEM_READ_ONLY, (strlen(secret_buf)+1)*sizeof(unsigned char));
    cl::Buffer msgBuffer = cl::Buffer(context, CL_MEM_READ_ONLY, (messageLen+1)*sizeof(unsigned char));
    cl::Buffer messageBuffer = cl::Buffer(context, CL_MEM_WRITE_ONLY, (BLOCK_BYTES+message.size())*sizeof(unsigned char));
    cl::Buffer origSigBuffer = cl::Buffer(context, CL_MEM_READ_ONLY, HASH_BYTES*sizeof(unsigned char));
    cl::Buffer boolBuffer = cl::Buffer(context, CL_MEM_WRITE_ONLY, limit*sizeof(bool));
    cl::Buffer test = cl::Buffer(context, CL_MEM_READ_WRITE, secretLen*sizeof(unsigned char));

    // Write buffers
    queue.enqueueWriteBuffer(secretBuffer, CL_TRUE, 0, (strlen(secret_buf)+1)*sizeof(unsigned char), secret_buf);
    queue.enqueueWriteBuffer(msgBuffer, CL_TRUE, 0, (messageLen+1)*sizeof(unsigned char), message.c_str());
    queue.enqueueWriteBuffer(origSigBuffer, CL_TRUE, 0, HASH_BYTES*sizeof(unsigned char), origSig.c_str());

    // Set arguments to kernel
    kernel.setArg(0, secretBuffer);
    kernel.setArg(1, secretLen);
    kernel.setArg(2, msgBuffer);
    kernel.setArg(3, messageLen);
    // kernel.setArg(4, messageBuffer);
    kernel.setArg(4, (messageLen+BLOCK_BYTES)*sizeof(unsigned char), NULL);
    kernel.setArg(5, origSigBuffer);
    kernel.setArg(6, boolBuffer);

    // Run the kernel on specific ND range
    cl::NDRange globalSize(limit);
    queue.enqueueNDRangeKernel(kernel, cl::NullRange, globalSize); 

    bool* stat = new bool[limit]; 
    queue.enqueueReadBuffer(boolBuffer, CL_TRUE, 0, limit*sizeof(bool),stat);
    for (int i = 0; i < limit; i++) {
        if (stat[i]) {
            for (int j = 0; j < secretLen; j++) {
                ans += secret_buf[i*secretLen+j];
            }
            free(stat);
            return true;
        }
    }
    free(stat);
    return false;
}

bool brute(string message, string origSig, string &secret, int index, int depth, cl::Context context, cl::CommandQueue queue, cl::Kernel kernel, string &ans) {
    bool stat = false;
    int limit = LIMIT;
    for (int i = 0; i < gAlphabet.length(); i++) {
        secret += gAlphabet[i];
        if (index == depth-1) {
            if (secret.size() <= depth) {
                strcat(secret_buf, secret.c_str());
                secret_count++;
            }
            else 
                limit = secret_count;
            if (secret_count == limit) {
                secret_count = 0;
                if (isValidSecret(message, origSig, context, queue, kernel, depth, limit, ans)) {
                    return true;
                }    
                for (int i = 0; i < limit; i++) {
                    // secrets[i] = "";
                    strcpy(secret_buf,"");
                } 
            }
        }
        else {
            if (brute(message,origSig,secret,index+1,depth, context, queue, kernel, ans)) {
                return true;
            }
        }
        if (!stat){
            secret.erase(index);
        }
    }
    return stat;
}

string brute_sequential(string msg, string origSig, string &secret, char start, cl::Context context, cl::CommandQueue queue, cl::Kernel kernel) {
    secret = start;
    string ans = "";
    for (int i = 2; i <= gMaxSecretLen; ++i) {
        if (brute(msg, origSig, secret, 1, i, context, queue, kernel, ans)) {
            break;            
        }
    } 
    return ans;
}

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::stringstream jwt;
    jwt << argv[1];

    if (argc > 2) {
        gMaxSecretLen = atoi(argv[2]);
    }
    if (argc > 3) {
        gAlphabet = argv[3];
    }

    std::string header64;
    getline(jwt, header64, '.');

    std::string payload64;
    getline(jwt, payload64, '.');

    std::string origSig64;
    getline(jwt, origSig64, '.');

    // Our goal is to find the secret to HMAC this string into our origSig
    std::string message = header64 + '.' + payload64;
    std::string origSig = base64_decode(origSig64);

    string secret = ""; 
    string final = ""; 
    string s = "";

    strcpy(secret_buf, "");
    // Use OpenCL to brute force JWT
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
        std::ifstream sourceFile("src/jwtcracker.cl");
            if(!sourceFile.is_open()){
                std::cerr << "Cannot find kernel file" << std::endl;
                throw;
            }
        std::string sourceCode(std::istreambuf_iterator<char>(sourceFile), (std::istreambuf_iterator<char>()));
        cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length() + 1));
 
        // Make program of the source code in the context
        cl::Program program = cl::Program(context, source);
 
        // Build program for these specific devices
        try {
            program.build(devices);
        } catch(cl::Error error) {
            std::cerr << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0]) << std::endl;
            throw;
        }
 
        // Make kernels
        cl::Kernel kernel(program, "bruteForceJWT");
 
        // Create buffers
        // cout << "Message Len: " << message.size() << endl;
        for (int i = 0; i < gAlphabet.size(); i++) {
            s = brute_sequential(message, origSig, final, gAlphabet[i], context, queue, kernel);
            if (s != "") {
                secret = s;
                break;
            }
        }        
        cout << "Secret: " << secret << endl; 

    } catch(cl::Error error) {
        std::cout << error.what() << "(" << error.err() << ")" << std::endl;
    }

    return 0;
}
