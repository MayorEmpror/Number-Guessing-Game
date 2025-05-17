#include <iostream>
#include <random>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <thread>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    typedef int socket_t;
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_CODE errno
    #define INVALID_SOCKET -1
#endif

#include "../include/database.h"
#include "../include/crypto_util.h"

namespace fs = std::filesystem;

// Function to generate a random number between min and max (inclusive)
int generateRandomNumber(int min, int max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

// Function to generate a clue based on how close the guess is to the target
std::string generateClue(int guess, int target, int min, int max) {
    int diff = std::abs(guess - target);
    int range = max - min;
    double percentDiff = static_cast<double>(diff) / range;
    
    std::string direction = (guess < target) ? "higher" : "lower";
    
    if (diff == 0) {
        return "Correct!";
    } else if (percentDiff <= 0.05) {
        return "Very hot! The number is " + direction + ".";
    } else if (percentDiff <= 0.1) {
        return "Hot! The number is " + direction + ".";
    } else if (percentDiff <= 0.2) {
        return "Warm. The number is " + direction + ".";
    } else if (percentDiff <= 0.4) {
        return "Cool. The number is " + direction + ".";
    } else {
        return "Cold! The number is " + direction + ".";
    }
}

// Function to read a file into a string
std::string readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Structure to hold HTTP request data
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;
};

// Parse HTTP request
HttpRequest parseHttpRequest(const std::string& requestStr) {
    HttpRequest request;
    std::istringstream stream(requestStr);
    std::string line;
    
    // Parse request line
    if (std::getline(stream, line)) {
        std::istringstream requestLine(line);
        requestLine >> request.method;
        
        std::string pathWithQuery;
        requestLine >> pathWithQuery;
        
        // Split path and query parameters
        size_t queryPos = pathWithQuery.find('?');
        if (queryPos != std::string::npos) {
            request.path = pathWithQuery.substr(0, queryPos);
            
            // Parse query parameters
            std::string queryString = pathWithQuery.substr(queryPos + 1);
            std::istringstream queryStream(queryString);
            std::string param;
            
            while (std::getline(queryStream, param, '&')) {
                size_t equalsPos = param.find('=');
                if (equalsPos != std::string::npos) {
                    std::string key = param.substr(0, equalsPos);
                    std::string value = param.substr(equalsPos + 1);
                    request.query_params[key] = value;
                }
            }
        } else {
            request.path = pathWithQuery;
        }
    }
    
    // Parse headers
    while (std::getline(stream, line) && line != "\r") {
        if (line.back() == '\r') {
            line.pop_back();  // Remove \r
        }
        
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim leading spaces from value
            while (!value.empty() && value.front() == ' ') {
                value.erase(0, 1);
            }
            
            request.headers[key] = value;
        }
    }
    
    // Parse body if Content-Length header exists
    if (request.headers.count("Content-Length") > 0) {
        int contentLength = std::stoi(request.headers["Content-Length"]);
        
        // Read body
        char* bodyBuffer = new char[contentLength + 1];
        stream.read(bodyBuffer, contentLength);
        bodyBuffer[contentLength] = '\0';
        request.body = bodyBuffer;
        delete[] bodyBuffer;
    }
    
    return request;
}

// Simple JSON parser
class Json {
public:
    Json() = default;
    
    Json(const std::string& json) {
        parse(json);
    }
    
    bool parse(const std::string& json) {
        size_t pos = 0;
        
        // Skip whitespace
        while (pos < json.size() && std::isspace(json[pos])) {
            pos++;
        }
        
        if (pos < json.size() && json[pos] == '{') {
            pos++;  // Skip {
            parseObject(json, pos);
            return true;
        }
        
        return false;
    }
    
    bool has(const std::string& key) const {
        return data.count(key) > 0;
    }
    
    std::string s(const std::string& key) const {
        auto it = data.find(key);
        if (it != data.end() && it->second.type == ValueType::String) {
            return it->second.stringValue;
        }
        return "";
    }
    
    int i(const std::string& key) const {
        auto it = data.find(key);
        if (it != data.end()) {
            if (it->second.type == ValueType::Number) {
                return static_cast<int>(it->second.numberValue);
            } else if (it->second.type == ValueType::String) {
                // Try to convert string to integer
                try {
                    return std::stoi(it->second.stringValue);
                } catch (...) {
                    std::cerr << "Failed to convert string value to int for key: " << key << std::endl;
                }
            }
        }
        return 0;
    }
    
    bool b(const std::string& key) const {
        auto it = data.find(key);
        if (it != data.end() && it->second.type == ValueType::Boolean) {
            return it->second.boolValue;
        }
        return false;
    }
    
private:
    enum class ValueType {
        String,
        Number,
        Boolean,
        Null
    };
    
    struct Value {
        ValueType type;
        std::string stringValue;
        double numberValue = 0.0;
        bool boolValue = false;
    };
    
    std::unordered_map<std::string, Value> data;
    
    void parseObject(const std::string& json, size_t& pos) {
        while (pos < json.size()) {
            // Skip whitespace
            while (pos < json.size() && std::isspace(json[pos])) {
                pos++;
            }
            
            if (pos < json.size() && json[pos] == '}') {
                pos++;  // Skip }
                break;
            }
            
            if (pos < json.size() && json[pos] == ',') {
                pos++;  // Skip ,
                continue;
            }
            
            // Parse key
            std::string key;
            if (pos < json.size() && json[pos] == '"') {
                pos++;  // Skip "
                while (pos < json.size() && json[pos] != '"') {
                    key += json[pos++];
                }
                if (pos < json.size()) pos++;  // Skip "
            }
            
            // Skip whitespace and :
            while (pos < json.size() && (std::isspace(json[pos]) || json[pos] == ':')) {
                pos++;
            }
            
            // Parse value
            Value value;
            if (pos < json.size()) {
                if (json[pos] == '"') {
                    value.type = ValueType::String;
                    pos++;  // Skip "
                    while (pos < json.size() && json[pos] != '"') {
                        value.stringValue += json[pos++];
                    }
                    if (pos < json.size()) pos++;  // Skip "
                } else if (std::isdigit(json[pos]) || json[pos] == '-') {
                    value.type = ValueType::Number;
                    std::string numStr;
                    while (pos < json.size() && (std::isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-')) {
                        numStr += json[pos++];
                    }
                    value.numberValue = std::stod(numStr);
                } else if (json.substr(pos, 4) == "true") {
                    value.type = ValueType::Boolean;
                    value.boolValue = true;
                    pos += 4;
                } else if (json.substr(pos, 5) == "false") {
                    value.type = ValueType::Boolean;
                    value.boolValue = false;
                    pos += 5;
                } else if (json.substr(pos, 4) == "null") {
                    value.type = ValueType::Null;
                    pos += 4;
                }
            }
            
            data[key] = value;
        }
    }
};

// Simple JSON builder
class JsonBuilder {
public:
    JsonBuilder() {
        data = "{";
    }
    
    JsonBuilder& add(const std::string& key, const std::string& value) {
        if (data.length() > 1) {
            data += ",";
        }
        data += "\"" + key + "\":\"" + value + "\"";
        return *this;
    }
    
    JsonBuilder& add(const std::string& key, int value) {
        if (data.length() > 1) {
            data += ",";
        }
        data += "\"" + key + "\":" + std::to_string(value);
        return *this;
    }
    
    JsonBuilder& add(const std::string& key, double value) {
        if (data.length() > 1) {
            data += ",";
        }
        data += "\"" + key + "\":" + std::to_string(value);
        return *this;
    }
    
    JsonBuilder& add(const std::string& key, bool value) {
        if (data.length() > 1) {
            data += ",";
        }
        data += "\"" + key + "\":" + (value ? "true" : "false");
        return *this;
    }
    
    JsonBuilder& addArray(const std::string& key, const std::string& jsonArray) {
        if (data.length() > 1) {
            data += ",";
        }
        data += "\"" + key + "\":" + jsonArray;
        return *this;
    }
    
    std::string build() {
        data += "}";
        return data;
    }
    
private:
    std::string data;
};

// HTTP response with JSON
std::string createJsonResponse(const std::string& json) {
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << json.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "\r\n";
    response << json;
    return response.str();
}

// HTTP response with HTML/CSS
std::string createHtmlResponse(const std::string& content, const std::string& contentType) {
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << contentType << "\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "\r\n";
    response << content;
    return response.str();
}

// HTTP 404 response
std::string create404Response() {
    std::string body = "<html><body><h1>404 Not Found</h1></body></html>";
    std::stringstream response;
    response << "HTTP/1.1 404 Not Found\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Content-Length: " << body.length() << "\r\n";
    response << "\r\n";
    response << body;
    return response.str();
}

// Create leaderboard JSON
std::string createLeaderboardJson(const std::vector<Database::LeaderboardEntry>& leaderboard) {
    try {
        std::stringstream json;
        json << "[";
        for (size_t i = 0; i < leaderboard.size(); ++i) {
            const auto& entry = leaderboard[i];
            if (i > 0) json << ",";
            json << "{";
            json << "\"username\":\"" << (entry.username.empty() ? "Unknown" : entry.username) << "\",";
            json << "\"best_score\":" << entry.best_score << ",";
            json << "\"games_played\":" << entry.games_played << ",";
            json << "\"wins\":" << entry.wins;
            json << "}";
        }
        json << "]";
        return json.str();
    } catch (const std::exception& e) {
        std::cerr << "Error creating leaderboard JSON: " << e.what() << std::endl;
        return "[]"; // Return empty array on error
    } catch (...) {
        std::cerr << "Unknown error creating leaderboard JSON" << std::endl;
        return "[]"; // Return empty array on error
    }
}

void handleClient(socket_t clientSocket, Database& db) {
    std::cout << "Enter handleClient" << std::endl;
    const int bufferSize = 4096;
    char buffer[bufferSize] = {0};
    std::string request;
    int bytesRead;
    
    try {
        // Read client request
        std::cout << "Reading client request..." << std::endl;
        bytesRead = recv(clientSocket, buffer, bufferSize - 1, 0);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            request = buffer;
            
            // Parse request
            std::cout << "Parsing request..." << std::endl;
            HttpRequest req = parseHttpRequest(request);
            std::string response;
            
            std::cout << "Request path: " << req.path << std::endl;
            
            // Handle different routes
            if (req.path == "/" || req.path == "/index.html") {
                std::cout << "Serving index.html..." << std::endl;
                // Serve index.html
                std::string html = readFile("public/index.html");
                if (!html.empty()) {
                    std::cout << "Index file read successfully" << std::endl;
                    response = createHtmlResponse(html, "text/html");
                } else {
                    std::cout << "Failed to read index file" << std::endl;
                    response = create404Response();
                }
            } else if (req.path == "/login.html") {
                std::cout << "Serving login.html..." << std::endl;
                // Serve login.html
                std::string html = readFile("public/login.html");
                if (!html.empty()) {
                    response = createHtmlResponse(html, "text/html");
                } else {
                    response = create404Response();
                }
            } else if (req.path == "/signup.html") {
                std::cout << "Serving signup.html..." << std::endl;
                // Serve signup.html
                std::string html = readFile("public/signup.html");
                if (!html.empty()) {
                    response = createHtmlResponse(html, "text/html");
                } else {
                    response = create404Response();
                }
            } else if (req.path.find("/css/") == 0) {
                std::cout << "Serving CSS file: " << req.path << std::endl;
                // Serve CSS files
                std::string css = readFile("public" + req.path);
                if (!css.empty()) {
                    response = createHtmlResponse(css, "text/css");
                } else {
                    response = create404Response();
                }
            } else if (req.path == "/api/signup" && req.method == "POST") {
                std::cout << "Handling signup..." << std::endl;
                // Handle signup
                Json json(req.body);
                if (json.has("username") && json.has("password")) {
                    std::string username = json.s("username");
                    std::string password = json.s("password");
                    
                    // Check if username already exists
                    if (db.userExists(username)) {
                        JsonBuilder builder;
                        builder.add("success", false)
                               .add("message", "Username already exists");
                        
                        response = createJsonResponse(builder.build());
                    } else {
                        // Hash the password
                        std::string passwordHash = CryptoUtil::hashPassword(password);
                        
                        // Create user
                        if (db.createUser(username, passwordHash)) {
                            // Get user ID
                            int userId = 0;
                            db.verifyUser(username, passwordHash, userId);
                            
                            JsonBuilder builder;
                            builder.add("success", true)
                                   .add("user_id", userId);
                            
                            response = createJsonResponse(builder.build());
                        } else {
                            JsonBuilder builder;
                            builder.add("success", false)
                                   .add("message", "Failed to create user");
                            
                            response = createJsonResponse(builder.build());
                        }
                    }
                } else {
                    JsonBuilder builder;
                    builder.add("success", false)
                           .add("message", "Invalid request");
                    
                    response = createJsonResponse(builder.build());
                }
            } else if (req.path == "/api/login" && req.method == "POST") {
                std::cout << "Handling login..." << std::endl;
                // Handle login
                try {
                    std::cout << "Parsing login JSON..." << std::endl;
                    Json json(req.body);
                    std::cout << "Checking username/password fields..." << std::endl;
                    if (json.has("username") && json.has("password")) {
                        std::string username = json.s("username");
                        std::string password = json.s("password");
                        
                        std::cout << "Login attempt for user: " << username << std::endl;
                        
                        // Hash the password
                        std::cout << "Hashing password..." << std::endl;
                        std::string passwordHash = CryptoUtil::hashPassword(password);
                        
                        // Verify user
                        std::cout << "Verifying user credentials..." << std::endl;
                        int userId = 0;
                        bool loginSuccess = false;
                        
                        try {
                            loginSuccess = db.verifyUser(username, passwordHash, userId);
                            std::cout << "Verification result: " << (loginSuccess ? "success" : "failed") 
                                      << ", userId: " << userId << std::endl;
                        } catch (const std::exception& e) {
                            std::cerr << "Exception in verifyUser: " << e.what() << std::endl;
                            loginSuccess = false;
                        }
                        
                        if (loginSuccess && userId > 0) {
                            std::cout << "Login successful, building response..." << std::endl;
                            JsonBuilder builder;
                            builder.add("success", true)
                                   .add("user_id", userId);
                            
                            response = createJsonResponse(builder.build());
                        } else {
                            std::cout << "Login failed, invalid credentials" << std::endl;
                            JsonBuilder builder;
                            builder.add("success", false)
                                   .add("message", "Invalid username or password");
                            
                            response = createJsonResponse(builder.build());
                        }
                    } else {
                        std::cout << "Login failed, invalid request format" << std::endl;
                        JsonBuilder builder;
                        builder.add("success", false)
                               .add("message", "Invalid request");
                        
                        response = createJsonResponse(builder.build());
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error in login handling: " << e.what() << std::endl;
                    
                    JsonBuilder builder;
                    builder.add("success", false)
                           .add("message", "An error occurred during login");
                    
                    response = createJsonResponse(builder.build());
                }
            } else if (req.path == "/api/new-game" && req.method == "POST") {
                // Start new game
                Json json(req.body);
                if (json.has("user_id")) {
                    int userId = json.i("user_id");
                    int min = 1;
                    int max = 100;
                    
                    if (json.has("difficulty")) {
                        std::string difficulty = json.s("difficulty");
                        if (difficulty == "easy") {
                            min = 1;
                            max = 50;
                        } else if (difficulty == "hard") {
                            min = 1;
                            max = 200;
                        }
                    }
                    
                    int targetNumber = generateRandomNumber(min, max);
                    std::cout << "New game started! Target number to guess: " << targetNumber << std::endl;
                    
                    JsonBuilder builder;
                    builder.add("success", true)
                           .add("min", min)
                           .add("max", max)
                           .add("gameId", targetNumber);
                    
                    response = createJsonResponse(builder.build());
                } else {
                    JsonBuilder builder;
                    builder.add("success", false)
                           .add("message", "Invalid request: missing user_id");
                    
                    response = createJsonResponse(builder.build());
                }
            } else if (req.path == "/api/guess" && req.method == "POST") {
                // Handle guess
                try {
                    std::cout << "Received guess request with body: " << req.body << std::endl;
                    Json json(req.body);
                    if (json.has("gameId") && json.has("guess") && json.has("attempts") && json.has("user_id")) {
                        int gameId = json.i("gameId");
                        int guess = json.i("guess");
                        int attempts = json.i("attempts");
                        int userId = json.i("user_id");
                        
                        std::cout << "Guess request - gameId: " << gameId << ", guess: " << guess 
                                  << ", attempts: " << attempts << ", userId: " << userId << std::endl;
                        
                        // Get min and max values if provided
                        int min = 1;
                        int max = 100;
                        if (json.has("min")) min = json.i("min");
                        if (json.has("max")) max = json.i("max");
                        
                        // In this simple implementation, gameId is the target number
                        int targetNumber = gameId;
                        
                        JsonBuilder builder;
                        builder.add("success", true);
                        
                        if (guess == targetNumber) {
                            // Correct guess
                            std::cout << "CORRECT GUESS! User: " << userId << ", Attempts: " << attempts << std::endl;
                            bool saveSuccess = db.saveGame(userId, targetNumber, attempts, true);
                            
                            if (!saveSuccess) {
                                std::cerr << "Failed to save game for user ID: " << userId << std::endl;
                                // Even if save fails, we still want to tell the user they were correct
                            } else {
                                std::cout << "Successfully saved win to database!" << std::endl;
                            }
                            
                            std::cout << "Sending correct=true in response" << std::endl;
                            builder.add("message", "Correct!");
                            builder.add("correct", true);
                        } else {
                            // Generate clue based on how close the guess is
                            std::string clue = generateClue(guess, targetNumber, min, max);
                            
                            builder.add("message", clue);
                            builder.add("correct", false);
                        }
                        
                        response = createJsonResponse(builder.build());
                    } else {
                        JsonBuilder builder;
                        builder.add("success", false)
                               .add("message", "Invalid request - missing required parameters");
                        
                        response = createJsonResponse(builder.build());
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error in guess handling: " << e.what() << std::endl;
                    
                    JsonBuilder errorBuilder;
                    errorBuilder.add("success", false)
                               .add("message", "An internal error occurred: " + std::string(e.what()));
                    
                    response = createJsonResponse(errorBuilder.build());
                }
            } else if (req.path == "/api/give-up" && req.method == "POST") {
                // Handle give up
                Json json(req.body);
                if (json.has("gameId") && json.has("attempts") && json.has("user_id")) {
                    int gameId = json.i("gameId");
                    int attempts = json.i("attempts");
                    int userId = json.i("user_id");
                    
                    // In this simple implementation, gameId is the target number
                    int targetNumber = gameId;
                    
                    // Save the game as lost
                    bool saveSuccess = db.saveGame(userId, targetNumber, attempts, false);
                    
                    JsonBuilder builder;
                    if (saveSuccess) {
                        builder.add("success", true)
                               .add("targetNumber", targetNumber);
                    } else {
                        std::cerr << "Failed to save game after give up for user ID: " << userId << std::endl;
                        builder.add("success", true) // Still return success to the client
                               .add("targetNumber", targetNumber)
                               .add("saveError", true); // Add a flag to indicate save error
                    }
                    
                    response = createJsonResponse(builder.build());
                } else {
                    JsonBuilder builder;
                    builder.add("success", false)
                           .add("message", "Invalid request");
                    
                    response = createJsonResponse(builder.build());
                }
            } else if (req.path == "/api/stats") {
                // Get stats
                try {
                    if (req.query_params.count("user_id") > 0) {
                        // Get user-specific stats
                        int userId = std::stoi(req.query_params["user_id"]);
                        Database::GameStats stats = db.getUserStats(userId);
                        
                        JsonBuilder builder;
                        builder.add("totalGames", stats.total_games)
                               .add("wins", stats.wins)
                               .add("bestScore", stats.best_score)
                               .add("avgAttempts", stats.avg_attempts);
                        
                        response = createJsonResponse(builder.build());
                    } else {
                        // Get global stats
                        Database::GameStats stats = db.getStats();
                        
                        JsonBuilder builder;
                        builder.add("totalGames", stats.total_games)
                               .add("wins", stats.wins)
                               .add("bestScore", stats.best_score)
                               .add("avgAttempts", stats.avg_attempts);
                        
                        response = createJsonResponse(builder.build());
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error fetching stats: " << e.what() << std::endl;
                    
                    // Return empty stats on error
                    JsonBuilder builder;
                    builder.add("totalGames", 0)
                           .add("wins", 0)
                           .add("bestScore", 0)
                           .add("avgAttempts", 0.0);
                    
                    response = createJsonResponse(builder.build());
                }
            } else if (req.path == "/api/leaderboard") {
                std::cout << "Handling leaderboard request..." << std::endl;
                // Get leaderboard
                try {
                    std::cout << "Fetching leaderboard data..." << std::endl;
                    std::vector<Database::LeaderboardEntry> leaderboard = db.getLeaderboard();
                    std::cout << "Leaderboard fetched, entries: " << leaderboard.size() << std::endl;
                    
                    std::cout << "Creating leaderboard JSON..." << std::endl;
                    std::string leaderboardJson = createLeaderboardJson(leaderboard);
                    std::cout << "JSON created, size: " << leaderboardJson.size() << " bytes" << std::endl;
                    
                    response = createJsonResponse(leaderboardJson);
                    std::cout << "Leaderboard response created" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Error fetching leaderboard: " << e.what() << std::endl;
                    
                    // Return empty array on error
                    response = createJsonResponse("[]");
                    std::cout << "Returned empty leaderboard due to error" << std::endl;
                }
            } else {
                // 404 for not found
                response = create404Response();
            }
            
            // Send response
            std::cout << "Sending response..." << std::endl;
            try {
                if (send(clientSocket, response.c_str(), response.length(), 0) < 0) {
                    std::cerr << "Failed to send response: " << SOCKET_ERROR_CODE << std::endl;
                } else {
                    std::cout << "Response sent successfully" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception when sending response: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception when sending response" << std::endl;
            }
        }
        
        // Close connection
        std::cout << "Closing connection..." << std::endl;
        CLOSE_SOCKET(clientSocket);
        std::cout << "Connection closed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception in handleClient: " << e.what() << std::endl;
        CLOSE_SOCKET(clientSocket); // Make sure socket is closed
    } catch (...) {
        std::cerr << "Unknown exception in handleClient" << std::endl;
        CLOSE_SOCKET(clientSocket); // Make sure socket is closed
    }
}

int main() {
    try {
        // Initialize socket library on Windows
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "Failed to initialize Winsock" << std::endl;
            return 1;
        }
#endif
        
        std::cout << "Initializing database..." << std::endl;
        // Initialize database
        Database db("number_guessing_game.db");
        if (!db.initialize()) {
            std::cerr << "Failed to initialize database!" << std::endl;
            return 1;
        }
        std::cout << "Database initialized successfully" << std::endl;
        
        std::cout << "Creating server socket..." << std::endl;
        // Create server socket
        socket_t serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET) {
            std::cerr << "Failed to create socket: " << SOCKET_ERROR_CODE << std::endl;
            return 1;
        }
        
        // Set socket options to allow reuse
        int opt = 1;
#ifdef _WIN32
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
#else
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
#endif
            std::cerr << "Failed to set socket options: " << SOCKET_ERROR_CODE << std::endl;
            CLOSE_SOCKET(serverSocket);
            return 1;
        }
        
        std::cout << "Setting up socket address..." << std::endl;
        // Set up socket address
        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        int port = 8081;
        serverAddr.sin_port = htons(port);
        
        std::cout << "Binding socket..." << std::endl;
        // Bind socket
        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Failed to bind socket: " << SOCKET_ERROR_CODE << std::endl;
            CLOSE_SOCKET(serverSocket);
            return 1;
        }
        
        std::cout << "Listening on socket..." << std::endl;
        // Listen for connections
        if (listen(serverSocket, 10) < 0) {
            std::cerr << "Failed to listen on socket: " << SOCKET_ERROR_CODE << std::endl;
            CLOSE_SOCKET(serverSocket);
            return 1;
        }
        
        std::cout << "Server running on port " << port << std::endl;
        std::cout << "Access the game at http://localhost:" << port << "/login.html" << std::endl;
        
        // Server loop - no threading for now to simplify debugging
        while (true) {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            
            std::cout << "Waiting for connection..." << std::endl;
            // Accept connection
            socket_t clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Failed to accept connection: " << SOCKET_ERROR_CODE << std::endl;
                continue;
            }
            
            std::cout << "Connection accepted, handling client..." << std::endl;
            // Handle client directly - no thread
            try {
                handleClient(clientSocket, std::ref(db));
                std::cout << "Client handled successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Exception in handleClient: " << e.what() << std::endl;
                CLOSE_SOCKET(clientSocket); // Make sure socket is closed
            } catch (...) {
                std::cerr << "Unknown exception in handleClient" << std::endl;
                CLOSE_SOCKET(clientSocket); // Make sure socket is closed
            }
        }
        
        // Close server socket
        CLOSE_SOCKET(serverSocket);
        
        // Cleanup socket library on Windows
#ifdef _WIN32
        WSACleanup();
#endif
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN FATAL ERROR" << std::endl;
        return 1;
    }
    
    return 0;
}