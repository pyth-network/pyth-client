# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.9.2] - 2021-12-02
- [pyth] case-sensitive http headers
- [docker] move build logic to `scripts` for reuse by `pyth-serum`
- test init mapping and add product
- bisontrails example nginx config
- CLion project settings

## [2.9.1] - 2021-11-03
- [pyth] do not update twap with last price if agg status is unknown

## [2.9] - 2021-11-02
- [pyth] add minimum number of publishers for price to be valid (not unknown)
- [pyth] various minor code cleanup
- [pyth] testing improvements

## [2.8.1] - 2021-10-05
- [pyth] temporary code to reset twap calc on negative values

## [2.8] - 2021-10-05
- [pyth] use some bits for storing exponent to avoid overflow (twap)

## [2.7] - 2021-10-02
- [pyth] proper lprice/uprice sorting in confidence calculation

## [2.6] - 2021-09-28
- [pyth] filter out quoters far from median
- [pyth] add linker flags to detect missing defs

## [2.5] - 2021-09-22
- [pyth] fixed signed division in conf interval calculation
- [pyth] enabled stricter compiler flags and fixed associated warnings

## [2.4] - 2021-09-21
- [config] Replaced `prodbeta` with current `mainnet` keys in the `init_key_store.sh`
- Break out admin-only request/rpc classes into separate files.
- [pyth_admin] Separate out admin commands into new binary.
- [pyth] Consolidate CLI argument parsing.
- [pyth] Remove get_pub_key command and update docs/scripts - these should by run using the solana CLI tool.
- [pyth] Remove transfer and get_balance command line commands - these should by run using the solana CLI tool.
- [pyth] add Host header to RPC requests
- [pyth] add account filtering to get_program_accounts
- [pyth] add option to disable ws connection to RPC node
- [docker] add gcc-multilib package
- [pyth] add python tests
- [pyth] filter out zero prices in aggregation

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
