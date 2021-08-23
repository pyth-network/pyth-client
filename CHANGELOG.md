# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [upcoming] - TBD
- [pyth_admin] Separate out admin commands into new binary.
- [pyth] Consolidate CLI argument parsing.
- [pyth] Remove get_pub_key command and update docs/scripts - these should by run using the solana CLI tool.
- [pyth] Remove transfer and get_balance command line commands - these should by run using the solana CLI tool.

## [2.3.1] - 2021-08-20
- Fixed uninitialized variable in `manager.cpp`

## [2.3] - 2021-08-18
### Security update
- [pythd] add whitelist for dashboard static files to http server to prevent directory traversal

### Others
- Fix constructor parameter order and unnecessary declaration.
- Add ability to send price updates without pyth_tx.
- pyth.cpp: add upd_price_val for explicit price value updates (#32)
- [pythd] Update user::parse_get_product code.
- [pythd] Add get_product and get_all_products WS methods.
- fix uninit variable

## [2.2] - 2021-08-10
- [pyth] Add get_all_products command.
- [pyth] Print error and return 1 on mapping key failure.
- [manager] Set error state if program key is not found.
- [pyth] Add hidden -d debug logging flag to help text.
- [replace] slot_subscribe with get_slot
- [build] Add pyth_tx to the release target
- [docker] no access to secrets on pull requests
- [docker] grant sudo access to pyth user

## [2.1] - 2021-07-27
- [build] remove github packages
- [docker] docker action to create containers
- [oracle] weight twap contributions inversely with confidence interval; added testnet keys
- [config] add testnet keys

## [2.0] - 2021-07-14
Initial public release
- fix to proxy soolana node reconnect logic in reinitializing slot leaders (origin/v2)
- show tx errors in get_block
