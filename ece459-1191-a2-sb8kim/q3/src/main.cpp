#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <cmath>
#include <cassert>
#include <cstring>
#include <string>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>
#include <omp.h>

#include <base64/base64.h>

using std::cout;
using std::cerr;
using std::endl;

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

#define HASH_BITS    256
#define HASH_BYTES   (HASH_BITS / 8)

int gMaxSecretLen = 4;

std::string gAlphabet = "abcdefghijklmnopqrstuvwxyz"
                        "0123456789";

using namespace std;

//-----------------------------------------------------------------------------
// Helper
//-----------------------------------------------------------------------------

bool isValidSecret(const std::string &message, const std::string &origSig, const std::string &secret) {
    uint8_t sigBuffer[EVP_MAX_MD_SIZE];
    uint32_t sigBufferLen;

    HMAC(
        (EVP_MD *)EVP_sha256(),
        (const unsigned char*)secret.c_str(), secret.size(),
        (const unsigned char*)message.c_str(), message.size(),
        sigBuffer, &(sigBufferLen)
    );

    assert(origSig.size() == HASH_BYTES);
    assert(sigBufferLen == HASH_BYTES);

    return memcmp(sigBuffer, origSig.c_str(), HASH_BYTES) == 0;
}

bool brute(string message, string origSig, string &secret, int index, int depth) {
    bool stat = false;
    for (int i = 0; i < gAlphabet.length(); i++) {
        secret += gAlphabet[i];
        if (index == depth-1) {
            if (isValidSecret(message, origSig, secret)) {
                return true;
            }
        }
        else {
            if (brute(message,origSig,secret,index+1,depth)) {
                return true;
            }
        }
        if (!stat){
            #pragma omp critical
            secret.erase(index);
        }
    }
    return stat;
}

string brute_sequential(string msg, string origSig, string &secret, char start) {
    secret = start;
    bool stat = false;
    if (gMaxSecretLen == 1) {
        if (isValidSecret(msg, origSig, secret)) {
            return secret; 
        }
    }
    #pragma omp parallel for shared(msg, origSig) 
    for (int i = 2; i <= gMaxSecretLen; ++i) {
        #pragma omp task shared(msg, origSig)
        if (brute(msg, origSig, secret, 1, i)) {
            stat = true;
            i = gMaxSecretLen;             
        }
    }
    
    if (stat) {
        return secret;
    }
    else 
        return "";   
}

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
    string final = ""; // dummy string to work with parallel because reasons
    string s = "";

    #pragma omp parallel for private(final) shared(message, origSig) 
    for (int i = 0; i < gAlphabet.size(); i++) {
        #pragma omp task private(final) shared(message, origSig,i)
        s = brute_sequential(message, origSig, final, gAlphabet[i]);
        #pragma omp taskwait
        #pragma omp critical
        if (s != "") {
            secret = s; // sort in third, external string because honestly idk
            i = gAlphabet.size();
        }
    }   
    // // *****************************************************
    // // ** Your job is to brute force all possible secrets **
    // // *****************************************************
    if (isValidSecret(message, origSig, secret)) {
        cout << secret << endl;
    } else {
        cout << secret << endl;
        cerr << "General Kenobi, you are a bold one" << endl;
    }

    return 0;
}
