#include "../include/database.h"
#include <iostream>

Database::Database(const std::string& db_name) : db(nullptr) {
    if (sqlite3_open(db_name.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    }
}

Database::~Database() {
    if (db) {
        sqlite3_close(db);
    }
}

bool Database::initialize() {
    if (!db) return false;
    
    // Create users table
    const char* createUsersTable = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "password_hash TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    
    // Create game_history table with user_id foreign key
    const char* createGameHistoryTable = 
        "CREATE TABLE IF NOT EXISTS game_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER,"
        "target_number INTEGER NOT NULL,"
        "attempts INTEGER NOT NULL,"
        "won INTEGER NOT NULL,"
        "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY (user_id) REFERENCES users(id)"
        ");";
    
    char* errMsg = nullptr;
    
    // Create users table
    if (sqlite3_exec(db, createUsersTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Error creating users table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    // Create game_history table
    if (sqlite3_exec(db, createGameHistoryTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Error creating game_history table: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    return true;
}

bool Database::createUser(const std::string& username, const std::string& password_hash) {
    if (!db) return false;
    
    const char* insertSql = 
        "INSERT INTO users (username, password_hash) VALUES (?, ?);";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Error preparing statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_STATIC);
    
    bool success = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    
    return success;
}

bool Database::verifyUser(const std::string& username, const std::string& password_hash, int& user_id) {
    if (!db) return false;
    
    const char* selectSql = 
        "SELECT id FROM users WHERE username = ? AND password_hash = ?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Error preparing statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash.c_str(), -1, SQLITE_STATIC);
    
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
        found = true;
    }
    
    sqlite3_finalize(stmt);
    return found;
}

bool Database::userExists(const std::string& username) {
    if (!db) return false;
    
    const char* selectSql = 
        "SELECT 1 FROM users WHERE username = ?;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, selectSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Error preparing statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    
    bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    
    return exists;
}

bool Database::saveGame(int user_id, int target_number, int attempts, bool won) {
    if (!db) return false;
    
    // First, verify that the user_id exists to prevent foreign key constraint violations
    const char* checkUserSql = "SELECT 1 FROM users WHERE id = ?;";
    sqlite3_stmt* checkStmt;
    
    if (sqlite3_prepare_v2(db, checkUserSql, -1, &checkStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Error preparing user check statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(checkStmt, 1, user_id);
    bool userExists = sqlite3_step(checkStmt) == SQLITE_ROW;
    sqlite3_finalize(checkStmt);
    
    if (!userExists) {
        std::cerr << "Error: Cannot save game for non-existent user ID: " << user_id << std::endl;
        return false;
    }
    
    // Now proceed with saving the game
    const char* insertSql = 
        "INSERT INTO game_history (user_id, target_number, attempts, won) VALUES (?, ?, ?, ?);";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Error preparing save game statement: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, target_number);
    sqlite3_bind_int(stmt, 3, attempts);
    sqlite3_bind_int(stmt, 4, won ? 1 : 0);
    
    int result = sqlite3_step(stmt);
    bool success = (result == SQLITE_DONE);
    
    if (!success) {
        std::cerr << "Error saving game: " << sqlite3_errmsg(db) << " (code: " << result << ")" << std::endl;
    }
    
    sqlite3_finalize(stmt);
    return success;
}

Database::GameStats Database::getStats() {
    GameStats stats = {0, 0, 0, 0.0};
    if (!db) return stats;
    
    // Get total games and wins
    const char* statsSql = 
        "SELECT COUNT(*) as total_games, "
        "SUM(CASE WHEN won = 1 THEN 1 ELSE 0 END) as wins "
        "FROM game_history;";
    
    sqlite3_stmt* statsStmt;
    if (sqlite3_prepare_v2(db, statsSql, -1, &statsStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(statsStmt) == SQLITE_ROW) {
            stats.total_games = sqlite3_column_int(statsStmt, 0);
            stats.wins = sqlite3_column_int(statsStmt, 1);
        }
        sqlite3_finalize(statsStmt);
    }
    
    // Get best score (minimum attempts for a win)
    const char* bestScoreSql = 
        "SELECT MIN(attempts) FROM game_history WHERE won = 1;";
    
    sqlite3_stmt* bestScoreStmt;
    if (sqlite3_prepare_v2(db, bestScoreSql, -1, &bestScoreStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(bestScoreStmt) == SQLITE_ROW && sqlite3_column_type(bestScoreStmt, 0) != SQLITE_NULL) {
            stats.best_score = sqlite3_column_int(bestScoreStmt, 0);
        }
        sqlite3_finalize(bestScoreStmt);
    }
    
    // Get average attempts
    const char* avgSql = 
        "SELECT AVG(attempts) FROM game_history;";
    
    sqlite3_stmt* avgStmt;
    if (sqlite3_prepare_v2(db, avgSql, -1, &avgStmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(avgStmt) == SQLITE_ROW && sqlite3_column_type(avgStmt, 0) != SQLITE_NULL) {
            stats.avg_attempts = sqlite3_column_double(avgStmt, 0);
        }
        sqlite3_finalize(avgStmt);
    }
    
    return stats;
}

Database::GameStats Database::getUserStats(int user_id) {
    GameStats stats = {0, 0, 0, 0.0};
    if (!db) return stats;
    
    // Get total games and wins for user
    const char* statsSql = 
        "SELECT COUNT(*) as total_games, "
        "SUM(CASE WHEN won = 1 THEN 1 ELSE 0 END) as wins "
        "FROM game_history WHERE user_id = ?;";
    
    sqlite3_stmt* statsStmt;
    if (sqlite3_prepare_v2(db, statsSql, -1, &statsStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(statsStmt, 1, user_id);
        
        if (sqlite3_step(statsStmt) == SQLITE_ROW) {
            stats.total_games = sqlite3_column_int(statsStmt, 0);
            stats.wins = sqlite3_column_int(statsStmt, 1);
        }
        sqlite3_finalize(statsStmt);
    }
    
    // Get best score for user
    const char* bestScoreSql = 
        "SELECT MIN(attempts) FROM game_history WHERE user_id = ? AND won = 1;";
    
    sqlite3_stmt* bestScoreStmt;
    if (sqlite3_prepare_v2(db, bestScoreSql, -1, &bestScoreStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(bestScoreStmt, 1, user_id);
        
        if (sqlite3_step(bestScoreStmt) == SQLITE_ROW && sqlite3_column_type(bestScoreStmt, 0) != SQLITE_NULL) {
            stats.best_score = sqlite3_column_int(bestScoreStmt, 0);
        }
        sqlite3_finalize(bestScoreStmt);
    }
    
    // Get average attempts for user
    const char* avgSql = 
        "SELECT AVG(attempts) FROM game_history WHERE user_id = ?;";
    
    sqlite3_stmt* avgStmt;
    if (sqlite3_prepare_v2(db, avgSql, -1, &avgStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(avgStmt, 1, user_id);
        
        if (sqlite3_step(avgStmt) == SQLITE_ROW && sqlite3_column_type(avgStmt, 0) != SQLITE_NULL) {
            stats.avg_attempts = sqlite3_column_double(avgStmt, 0);
        }
        sqlite3_finalize(avgStmt);
    }
    
    return stats;
}

std::vector<Database::LeaderboardEntry> Database::getLeaderboard(int limit) {
    std::cout << "Database::getLeaderboard called with limit: " << limit << std::endl;
    std::vector<LeaderboardEntry> leaderboard;
    if (!db) {
        std::cerr << "Database connection is null" << std::endl;
        return leaderboard;
    }
    
    try {
        // Check if game_history table has any entries
        std::cout << "Checking if game_history table has entries..." << std::endl;
        const char* checkSql = "SELECT COUNT(*) FROM game_history;";
        sqlite3_stmt* checkStmt;
        int gameCount = 0;
        
        int rc = sqlite3_prepare_v2(db, checkSql, -1, &checkStmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Error preparing check statement: " << sqlite3_errmsg(db) << std::endl;
            return leaderboard;
        }
        
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            gameCount = sqlite3_column_int(checkStmt, 0);
            std::cout << "Found " << gameCount << " game history entries" << std::endl;
        }
        sqlite3_finalize(checkStmt);
        
        // If no game history entries, just return usernames without stats
        if (gameCount == 0) {
            std::cout << "No game history found, returning basic user list..." << std::endl;
            const char* userSql = "SELECT username FROM users LIMIT ?;";
            sqlite3_stmt* userStmt;
            
            rc = sqlite3_prepare_v2(db, userSql, -1, &userStmt, nullptr);
            if (rc != SQLITE_OK) {
                std::cerr << "Error preparing user statement: " << sqlite3_errmsg(db) << std::endl;
                return leaderboard;
            }
            
            sqlite3_bind_int(userStmt, 1, limit);
            std::cout << "Fetching user list..." << std::endl;
            
            while (sqlite3_step(userStmt) == SQLITE_ROW) {
                LeaderboardEntry entry;
                const unsigned char* username = sqlite3_column_text(userStmt, 0);
                if (username) {
                    entry.username = reinterpret_cast<const char*>(username);
                } else {
                    entry.username = "Unknown";
                }
                entry.best_score = 0;
                entry.games_played = 0;
                entry.wins = 0;
                
                leaderboard.push_back(entry);
                std::cout << "Added user to leaderboard: " << entry.username << std::endl;
            }
            
            sqlite3_finalize(userStmt);
            std::cout << "Returning leaderboard with " << leaderboard.size() << " entries" << std::endl;
            return leaderboard;
        }
        
        // Original leaderboard query for when game history exists
        std::cout << "Game history exists, running full leaderboard query..." << std::endl;
        const char* leaderboardSql = 
            "SELECT u.username, "
            "MIN(CASE WHEN g.won = 1 THEN g.attempts ELSE NULL END) as best_score, "
            "COUNT(g.id) as games_played, "
            "SUM(CASE WHEN g.won = 1 THEN 1 ELSE 0 END) as wins "
            "FROM users u "
            "LEFT JOIN game_history g ON u.id = g.user_id "
            "GROUP BY u.id "
            "ORDER BY best_score IS NULL, best_score ASC, wins DESC "
            "LIMIT ?;";
        
        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, leaderboardSql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Error preparing leaderboard statement: " << sqlite3_errmsg(db) << std::endl;
            return leaderboard;
        }
        
        sqlite3_bind_int(stmt, 1, limit);
        std::cout << "Fetching full leaderboard data..." << std::endl;
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            LeaderboardEntry entry;
            const unsigned char* username = sqlite3_column_text(stmt, 0);
            if (username) {
                entry.username = reinterpret_cast<const char*>(username);
            } else {
                entry.username = "Unknown";
            }
            
            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                entry.best_score = sqlite3_column_int(stmt, 1);
            } else {
                entry.best_score = 0;
            }
            
            entry.games_played = sqlite3_column_int(stmt, 2);
            entry.wins = sqlite3_column_int(stmt, 3);
            
            leaderboard.push_back(entry);
            std::cout << "Added user to leaderboard: " << entry.username 
                      << ", score: " << entry.best_score 
                      << ", games: " << entry.games_played
                      << ", wins: " << entry.wins << std::endl;
        }
        
        sqlite3_finalize(stmt);
        std::cout << "Returning complete leaderboard with " << leaderboard.size() << " entries" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception in getLeaderboard: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in getLeaderboard" << std::endl;
    }
    
    return leaderboard;
} 