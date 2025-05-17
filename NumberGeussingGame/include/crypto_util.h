#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <functional>

class CryptoUtil {
public:
    // Simple SHA-256 like hash function
    // Note: This is a simplified example and not cryptographically secure
    // In production, use a proper cryptography library
    static std::string hashPassword(const std::string& password) {
        // Combine a simple hash function with the password
        std::size_t hash = std::hash<std::string>{}(password);
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << hash;
        
        // Additional mixing to make it harder to reverse
        std::string result = ss.str();
        for (int i = 0; i < 10; i++) {
            result = simpleHash(result);
        }
        
        return result;
    }

private:
    static std::string simpleHash(const std::string& input) {
        const std::size_t len = input.length();
        std::string output(len, '0');
        
        for (std::size_t i = 0; i < len; i++) {
            char c = input[i];
            c = (c << 3) | (c >> 5);  // Rotate bits
            c = c ^ 0x5A;  // XOR with a constant
            output[i] = c;
        }
        
        std::stringstream ss;
        for (char c : output) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)c;
        }
        
        return ss.str();
    }
}; 