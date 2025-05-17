#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>

class Database {
public:
    Database(const std::string& db_name);
    ~Database();

    // Initialize database tables
    bool initialize();
    
    // User operations
    bool createUser(const std::string& username, const std::string& password_hash);
    bool verifyUser(const std::string& username, const std::string& password_hash, int& user_id);
    bool userExists(const std::string& username);
    
    // Game history operations
    bool saveGame(int user_id, int target_number, int attempts, bool won);
    
    // Stats operations
    struct GameStats {
        int total_games;
        int wins;
        int best_score;
        double avg_attempts;
    };
    
    GameStats getStats();
    GameStats getUserStats(int user_id);
    
    // Leaderboard operations
    struct LeaderboardEntry {
        std::string username;
        int best_score;
        int games_played;
        int wins;
    };
    
    std::vector<LeaderboardEntry> getLeaderboard(int limit = 10);
    
private:
    sqlite3* db;
}; 