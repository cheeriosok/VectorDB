# Redis 

A simplified Redis-like in-memory data structure store, capable of supporting basic data structures such as strings, hashes, lists, sets, and more.

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Contribution](#contribution)
- [Acknowledgments](#acknowledgments)
- [License](#license)

## Features

- **In-memory Storage:** Store data directly in memory for quick access.
- **Basic Data Structures:** Support for strings, lists, hashes, and sets.
- **Command Parser:** Interpret and execute commands similar to native Redis commands.
- **Persistence:** (Optional) Save and load data from disk to retain data across sessions.

## Installation

### Prerequisites
- C++ 20
- Linux

Clone the repository:

    git clone https://github.com/cheeriosok/Redis

Navigate to the project directory:

    cd redis

Build the project (if applicable):

    make build

## Usage

Start the BYOR server:

    ./redis-server

Connect to the server using BYOR client:

    ./redis-client

Once connected, you can use commands similar to Redis:

    > SET hello "world"
    OK

    > GET hello
    "world"


