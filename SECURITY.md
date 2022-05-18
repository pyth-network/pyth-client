# Security

1. [Bug Bounty Program](#bounty)
2. [3rd Party Security Audits](#audits)

<a name="bounty"></a>
## Bug Bounty Program

We operate a bug bounty program to financially incentivize independent researchers (with up to $TBD USDC) to find and responsibly disclose security issues in Pyth.
### Payout Structure

- **Critical** (Up to $TBD USDC)
    - Arbitrarily manipulate Pyth oracle prices or other published values.
    - Assume ownership of Pyth’s contracts in mainnet.
    - Locking, loss, or theft of funds staked on Pyth
- **High** (Up to $TBD USDC)
    - Software flaws in the on-chain program that cause Pyth to publish an inaccurate price when ≥ 3/4 of the contributing publishers are accurate.
    - Flaws enabling denial-of-service attacks for public-facing APIs
    - Remote code execution
    - Exposure of private keys for Pyth publishers or permissionless services.

_Note: We do encourage vulnerability reporters to submit issues outside of the above mentioned payout structure, though we want to be clear that we will exercise discretion on a case by case basis whether an issue warrants a payout and what that payout would be._

### In Scope Assets
- **[Pyth-Client](https://github.com/pyth-network/pyth-client)**

_Note: We will be adding additional scope as we complete due diligence efforts for those scopes._
### How to report a security vulnerability?

Please do not file a GitHub Issue for security vulnerabilities.

Please email security@pyth-network.com and provide your GitHub username and we will setup a draft security advisory for further discussion.

In your email, you can explain the potential impact of the security vulnerability, but please do not include detailed exploitation details until we can establish secure communications with the draft security advisory.

You should expect a response from our team within 72 hours.

<a name="audits"></a>
## 3rd Party Security Audits

We engage 3rd party firms to conduct independent security audits of Pyth.  At any given time, we likely have multiple audit streams in progress.

As these 3rd party audits are completed and issues are sufficiently addressed, we make those audit reports public.

- **[April, 8, 2022 - Zellic](https://github.com/pyth-network/audit-reports/blob/main/2022_04_08/pyth_oracle_client_zellic.pdf)**
    - **Scope**: *Pyth Client*