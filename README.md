BSV Fork
=========

What is this project?
-------------------

This is a fork of Bitcoin SV that creates a new blockchain from genesis. It maintains the core functionality of BSV while starting fresh with new genesis parameters and network configuration.

Key Features:
- Fresh genesis block created on April 1, 2025
- All protocol upgrades enabled from genesis
- 4GB block size from start
- Custom network magic bytes and port (8444)
- Clean checkpoint history

Building and Running
-------------------

1. Install dependencies (same as BSV)
2. Build using:
   ```
   ./autogen.sh
   ./configure
   make
   ```
3. Run the node:
   ```
   ./src/bitcoind
   ```

License
-------

This project is released under the terms of the Open BSV license. See [LICENSE](LICENSE) for more information.

Development Process
-------------------

This is an experimental fork of Bitcoin SV. Please use with caution.

Network Parameters
----------------

- Network Port: 8444
- RPC Port: 8445
- Genesis Time: April 1, 2025
- All upgrades active from genesis
- Max Block Size: 4GB
# boundless-sv
