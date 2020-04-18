#include "utils.h"
#include <openssl/sha.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <iostream>
#include <stdio.h>
#include <mutex>

Container<std::string> readFile(std::string fileName) {
    std::ifstream file(fileName);
    std::string line;
    Container<std::string> result;

    while (std::getline(file, line)) {
        if (!line.empty()) {
            result.push(line);
        }
    }

    return result;
}

std::string readFileLine(std::string fileName, int targetLineNum) {
    int counter = -1;
    std::string line; 
    std::ifstream file(fileName);

    while (counter < targetLineNum && std::getline(file,line)) { 
        if (!line.empty()) 
            counter++;
        if (file.eof()) {
            file.clear();
            file.seekg(0, file.beg);
        }
    }

    return line;
}

std::string readFileLineTest(std::ifstream& file, int targetLineNum) {
    std::string line; 
    if (file.eof()) {
        file.clear(); 
        file.seekg(0, file.beg);
    }   
    std::getline(file, line);
    return line;
}

const char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

std::string bytesToString(uint8_t* bytes, int len) {
    std::string str(len*2, ' ');
    for (int i = 0; i < len; i++) {
        str[2*i] = hex[(bytes[i] & 0xF0) >> 4]; 
        str[2*i + 1] = hex[bytes[i] & 0x0F];
    } 
    return str;
}

std::string sha256(const std::string str) {
    uint8_t* hash = new uint8_t[SHA256_DIGEST_LENGTH];

    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);

    std::string s = bytesToString(hash, SHA256_DIGEST_LENGTH);
    delete [] hash;
    return s;
}

std::string initChecksum() {
    std::string checksum;

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        checksum += "00";
    }

    return checksum;
}

uint8_t hexCharToByte(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    exit(1);
}

uint8_t hexStrToByte(char c0, char c1) {

    unsigned int x = (hexCharToByte(c0) << 4) | hexCharToByte(c1);

    if (!(x >= 0x00 && x <= 0xFF)) {
        printf("char1: %c\n", c0);
        printf("char2: %c\n", c1);
        printf("error x: %d\n", x);
    }
    assert(x >= 0x00 && x <= 0xFF); 

    return x; 
}

std::string xorChecksum(std::string baseLayer, std::string newLayer) {
    std::string checksum;

    assert(newLayer.size() == baseLayer.size());

    for (int i = 0; i < newLayer.size(); i += 2) {
        uint8_t* byte = new uint8_t[1];

        uint8_t a = hexStrToByte(baseLayer[i], baseLayer[i+1]);
        uint8_t b = hexStrToByte(newLayer[i], newLayer[i+1]); 

        byte[0] = a^b; 

        checksum += bytesToString(byte, 1);

        delete [] byte;
    }

    return checksum;
}
