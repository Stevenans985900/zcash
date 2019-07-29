// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"

#include <boost/optional.hpp>

// Forward declaration.  Enum requires storage type.
namespace Consensus {
    struct Params;
    enum UpgradeIndex : uint32_t;
}
bool NetworkUpgradeActive(int nHeight, const Consensus::Params& params, Consensus::UpgradeIndex idx);


namespace Consensus {

/**
 * Index into Params.vUpgrades and NetworkUpgradeInfo
 *
 * Being array indices, these MUST be numbered consecutively.
 *
 * The order of these indices MUST match the order of the upgrades on-chain, as
 * several functions depend on the enum being sorted.
 */
enum UpgradeIndex : uint32_t {
    // Sprout must be first
    BASE_SPROUT,
    UPGRADE_TESTDUMMY,
    UPGRADE_OVERWINTER,
    UPGRADE_SAPLING,
    UPGRADE_BLOSSOM,
    // NOTE: Also add new upgrades to NetworkUpgradeInfo in upgrades.cpp
    MAX_NETWORK_UPGRADES
};

struct NetworkUpgrade {
    /**
     * The first protocol version which will understand the new consensus rules
     */
    int nProtocolVersion;

    /**
     * Height of the first block for which the new consensus rules will be active
     */
    int nActivationHeight;

    /**
     * Special value for nActivationHeight indicating that the upgrade is always active.
     * This is useful for testing, as it means tests don't need to deal with the activation
     * process (namely, faking a chain of somewhat-arbitrary length).
     *
     * New blockchains that want to enable upgrade rules from the beginning can also use
     * this value. However, additional care must be taken to ensure the genesis block
     * satisfies the enabled rules.
     */
    static constexpr int ALWAYS_ACTIVE = 0;

    /**
     * Special value for nActivationHeight indicating that the upgrade will never activate.
     * This is useful when adding upgrade code that has a testnet activation height, but
     * should remain disabled on mainnet.
     */
    static constexpr int NO_ACTIVATION_HEIGHT = -1;
};

/** ZIP208 block target interval in seconds. */
static const unsigned int PRE_BLOSSOM_POW_TARGET_SPACING = 150;
static const unsigned int POST_BLOSSOM_POW_TARGET_SPACING = 75;
BOOST_STATIC_ASSERT(POST_BLOSSOM_POW_TARGET_SPACING < PRE_BLOSSOM_POW_TARGET_SPACING);
static const unsigned int PRE_BLOSSOM_HALVING_INTERVAL = 840000;
static const unsigned int PRE_BLOSSOM_REGTEST_HALVING_INTERVAL = 150;
static const unsigned int BLOSSOM_POW_TARGET_SPACING_RATIO = PRE_BLOSSOM_POW_TARGET_SPACING / POST_BLOSSOM_POW_TARGET_SPACING;
static const unsigned int POST_BLOSSOM_HALVING_INTERVAL = PRE_BLOSSOM_HALVING_INTERVAL * BLOSSOM_POW_TARGET_SPACING_RATIO;
static const unsigned int POST_BLOSSOM_REGTEST_HALVING_INTERVAL = PRE_BLOSSOM_REGTEST_HALVING_INTERVAL * BLOSSOM_POW_TARGET_SPACING_RATIO;

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;

    bool fCoinbaseMustBeProtected;

    /** Needs to evenly divide MAX_SUBSIDY to avoid rounding errors. */
    int nSubsidySlowStartInterval;
    /**
     * Shift based on a linear ramp for slow start:
     *
     * MAX_SUBSIDY*(t_s/2 + t_r) = MAX_SUBSIDY*t_h  Coin balance
     *              t_s   + t_r  = t_h + t_c        Block balance
     *
     * t_s = nSubsidySlowStartInterval
     * t_r = number of blocks between end of slow start and first halving
     * t_h = nPreBlossomSubsidyHalvingInterval
     * t_c = SubsidySlowStartShift()
     */
    int SubsidySlowStartShift() const { return nSubsidySlowStartInterval / 2; }
    int nPreBlossomSubsidyHalvingInterval;
    int nPostBlossomSubsidyHalvingInterval;

    int GetLastFoundersRewardBlockHeight(int nHeight) const {
        // zip208
        // FoundersRewardLastBlockHeight := max({ height ⦂ N | Halving(height) < 1 })
        // H := blossom activation height; SS := SubsidySlowStartShift(); R := BLOSSOM_POW_TARGET_SPACING_RATIO
        // preBlossom:
        // 1 = (height - SS) / preInterval
        // height = preInterval + SS
        // postBlossom:
        // 1 = (H - SS) / preInterval + (height - H) / postInterval
        // height = H + postInterval + (SS - H) * (postInterval / preInterval)
        // height = H + postInterval + (SS - H) / R
        bool blossomActive = NetworkUpgradeActive(nHeight, *this, Consensus::UPGRADE_BLOSSOM);
        if (blossomActive) {
            int blossomActivationHeight = vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight;
            return blossomActivationHeight + nPostBlossomSubsidyHalvingInterval
                - (SubsidySlowStartShift() - blossomActivationHeight) / BLOSSOM_POW_TARGET_SPACING_RATIO - 1;
        } else {
            return nPreBlossomSubsidyHalvingInterval + SubsidySlowStartShift() - 1;
        }
    }

    /** Used to check majorities for block version upgrade */
    int nMajorityEnforceBlockUpgrade;
    int nMajorityRejectBlockOutdated;
    int nMajorityWindow;
    NetworkUpgrade vUpgrades[MAX_NETWORK_UPGRADES];
    /** Proof of work parameters */
    unsigned int nEquihashN = 0;
    unsigned int nEquihashK = 0;
    uint256 powLimit;
    boost::optional<uint32_t> nPowAllowMinDifficultyBlocksAfterHeight;
    int64_t nPowAveragingWindow;
    int64_t nPowMaxAdjustDown;
    int64_t nPowMaxAdjustUp;
    int64_t nPreBlossomPowTargetSpacing;
    int64_t nPostBlossomPowTargetSpacing;

    int64_t PoWTargetSpacing(int nHeight) const {
        // PoWTargetSpacing(height) :=
        // PreBlossomPoWTargetSpacing, if not IsBlossomActivated(height)
        // PostBlossomPoWTargetSpacing, otherwise.
        bool blossomActive = NetworkUpgradeActive(nHeight, *this, Consensus::UPGRADE_BLOSSOM);
        return blossomActive ? nPostBlossomPowTargetSpacing : nPreBlossomPowTargetSpacing;
    }

    int64_t AveragingWindowTimespan(int nHeight) const {
        return nPowAveragingWindow * PoWTargetSpacing(nHeight);
    }

    int64_t MinActualTimespan(int nHeight) const {
        return (AveragingWindowTimespan(nHeight) * (100 - nPowMaxAdjustUp)) / 100;
    }

    int64_t MaxActualTimespan(int nHeight) const {
        return (AveragingWindowTimespan(nHeight) * (100 + nPowMaxAdjustDown)) / 100;
    }

    uint256 nMinimumChainWork;
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
