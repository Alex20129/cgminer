# CGMiner

Bitcoin miner written in C. This is the classic 'cgminer' adapted to work with Antminer devices.

## Description

This project is a Bitcoin mining implementation written primarily in C (96%), with some C++ (3.8%) and CMake (0.2%) components.

## Features

- Bitcoin mining functionality
- Written primarily in C
- Command-line interface

## Requirements

- C compiler (GCC recommended)
- CMake for building

## Building from Source

```bash
# Clone the repository
git clone https://github.com/Alex20129/cgminer.git

# Create build directory
mkdir build
cd build

# Generate build files with CMake
cmake ../cgminer

# Build the project
cmake --build ./
```

## Usage

```bash
./cgminer [options]
```
