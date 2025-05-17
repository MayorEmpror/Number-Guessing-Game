# Number Guessing Game

A web-based number guessing game built with C++ backend (Crow framework + SQLite) and HTML/CSS frontend. The application is cross-platform compatible, working on both Windows and Mac.

## Features

- Three difficulty levels: Easy (1-50), Medium (1-100), and Hard (1-200)
- Game statistics tracking (total games, wins, best score, average attempts)
- Responsive UI that works on both desktop and mobile devices
- Persistent data storage using SQLite

## Prerequisites

To build and run this project, you need:

- C++17 compatible compiler
- CMake 3.10 or higher
- SQLite3 development library
- Internet connection (only during first build to download the Crow header)

### Installing Prerequisites

#### On macOS:

```bash
brew install cmake sqlite3 boost
```

#### On Windows:

1. Install [CMake](https://cmake.org/download/)
2. Install [SQLite](https://www.sqlite.org/download.html)
3. Make sure both are in your PATH

## Building the Project

1. Clone or download this repository
2. Navigate to the project directory
3. Create a build directory:

```bash
mkdir build
cd build
```

4. Configure and build the project:

```bash
cmake ..
cmake --build .
```

## Running the Game

After building, run the executable:

### On macOS/Linux:

```bash
./NumberGuessingGame
```

### On Windows:

```bash
NumberGuessingGame.exe
```

Then open your web browser and navigate to: http://localhost:8080

## How to Play

1. Select a difficulty level to start a new game
2. Enter your guess in the input field
3. The game will tell you if your guess is too high or too low
4. Keep guessing until you find the correct number
5. Try to guess the number in as few attempts as possible
6. View your game statistics at the bottom of the page

<img width="1440" alt="Image5" src="https://github.com/user-attachments/assets/70839271-6239-4e3f-b63d-b7be506319bd" />
<img width="1440" alt="Image4" src="https://github.com/user-attachments/assets/56b1a35d-974c-475e-8259-49af5a9cffda" />
<img width="1440" alt="Image3" src="https://github.com/user-attachments/assets/a603a213-bda9-4167-b5a1-1ad51eead56f" />
<img width="1440" alt="Image2" src="https://github.com/user-attachments/assets/298db62d-d9a9-42de-9b39-2bedc4c5b072" />
<img width="1440" alt="Image1" src="https://github.com/user-attachments/assets/71257b9a-2c5d-48d2-a00b-06d28acd2dac" />




## Project Structure

- `src/` - C++ source files
  - `main.cpp` - Main application and web server
  - `database.cpp` - SQLite database interaction
- `include/` - Header files
  - `crow_all.h` - Crow framework header (downloaded during build)
  - `database.h` - Database class definition
- `public/` - Static web files
  - `index.html` - Main HTML page
  - `css/` - CSS stylesheets

## License

This project is open source, free to use and modify.

## Troubleshooting

- If you see 'File not found' errors, make sure the server is running from the correct directory.
- If the server fails to start, ensure port 8080 is not already in use by another application.
- For database issues, check that SQLite is properly installed and accessible. 




