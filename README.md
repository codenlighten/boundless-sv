BSV Fork - A New Bitcoin SV Fork
================================

## Overview
This is a fork of Bitcoin SV that launched on April 1st, 2025. It features a new genesis block and network parameters while maintaining the core functionality of BSV.

## Key Features
- New genesis block dated April 1st, 2025
- Different network magic bytes to prevent cross-network communication
- Custom network ports (P2P: 8333, RPC: 8332)
- Maintains BSV's scaling capabilities

## Building from Source

### Prerequisites
```bash
sudo apt-get update
sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils python3
```

### Build Instructions
```bash
./autogen.sh
./configure
make
```

## Running a Node

### Configuration
Create a bitcoin.conf file in your data directory:
```ini
# Network Settings
port=8333
rpcport=8332

# RPC Settings
rpcuser=your_username
rpcpassword=your_secure_password
rpcallowip=127.0.0.1

# Node Settings
maxconnections=40
dbcache=4096
maxmempool=2000

# Consensus Settings
excessiveblocksize=4000000000
maxstackmemoryusageconsensus=100000000
```

### Starting the Node
```bash
./src/bitcoind -daemon
```

## Running with Docker

### Using Docker Compose (Recommended)
```bash
# Build and start the node
docker-compose up -d

# View logs
docker-compose logs -f

# Stop the node
docker-compose down
```

### Using Docker Directly
```bash
# Build the image
docker build -t bsv_fork .

# Run the container
docker run -d \
  --name bsv_fork_node \
  -p 8333:8333 \
  -p 8332:8332 \
  -v blockchain_data:/root/.bitcoin \
  bsv_fork
```

### Docker Volume Management
The blockchain data is stored in a Docker volume for persistence. You can manage it using:
```bash
# List volumes
docker volume ls

# Inspect volume
docker volume inspect blockchain_data

# Remove volume (WARNING: This will delete all blockchain data!)
docker volume rm blockchain_data
```

## Genesis Block Information
- Time: April 1st, 2025 (1712152800)
- Message: "The Times 01/Apr/2025 Launching a new blockchain from BSV fork"
- Initial Difficulty: 0x1d00ffff

## Network Information
- P2P Port: 8333
- RPC Port: 8332
- Network Magic: 0xf9beb4d9

## Seed Nodes
- seed1.yourfork.org
- seed2.yourfork.org

## Contributing
We welcome contributions! Please submit pull requests to our GitHub repository.

## License
This project is licensed under the MIT License - see the LICENSE file for details.
