cmake_minimum_required(VERSION 3.10)
project(NumberGuessingGame)

set(CMAKE_CXX_STANDARD 17)

# Find SQLite package
find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIRS})

# Add header-only libraries
include_directories(include)

# Add executable
add_executable(NumberGuessingGame src/main.cpp src/database.cpp)

# Link libraries
target_link_libraries(NumberGuessingGame ${SQLite3_LIBRARIES})

# On Windows, link to ws2_32
if(WIN32)
    target_link_libraries(NumberGuessingGame ws2_32)
endif()

# Add pthread on Unix platforms
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(NumberGuessingGame ${CMAKE_THREAD_LIBS_INIT})
endif()

# Copy static files
file(COPY ${CMAKE_SOURCE_DIR}/public DESTINATION ${CMAKE_BINARY_DIR}) 