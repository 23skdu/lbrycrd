// Copyright (c) 2015 The LBRY Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://opensource.org/licenses/mit-license.php

#include "main.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"
#include "miner.h"
#include "txmempool.h"
#include "claimtrie.h"
#include "nameclaim.h"
#include "coins.h"
#include "streams.h"
#include "chainparams.h"
#include <boost/test/unit_test.hpp>
#include <iostream>
#include "test/test_bitcoin.h"

using namespace std;

CScript scriptPubKey = CScript() << OP_TRUE;

BOOST_FIXTURE_TEST_SUITE(claimtrie_tests, RegTestingSetup)

CMutableTransaction BuildTransaction(const uint256& prevhash)
{
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vin[0].prevout.hash = prevhash;
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    tx.vout[0].scriptPubKey = CScript();
    tx.vout[0].nValue = 0;

    return tx;
}

CMutableTransaction BuildTransaction(const CMutableTransaction& prev, uint32_t prevout=0, unsigned int numOutputs=1)
{
    CMutableTransaction tx;
    tx.nVersion = 1;
    tx.nLockTime = 0;
    tx.vin.resize(1);
    tx.vout.resize(numOutputs);
    tx.vin[0].prevout.hash = prev.GetHash();
    tx.vin[0].prevout.n = prevout;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = std::numeric_limits<unsigned int>::max();
    CAmount valuePerOutput = prev.vout[prevout].nValue;
    unsigned int numOutputsCopy = numOutputs;
    while ((numOutputsCopy = numOutputsCopy >> 1) > 0)
    {
        valuePerOutput = valuePerOutput >> 1;
    }
    for (unsigned int i = 0; i < numOutputs; ++i)
    {
        tx.vout[i].scriptPubKey = CScript();
        tx.vout[i].nValue = valuePerOutput;
    }

    return tx;
}

CMutableTransaction BuildTransaction(const CTransaction& prev, uint32_t prevout=0, unsigned int numOutputs=1)
{
    return BuildTransaction(CMutableTransaction(prev), prevout, numOutputs);
}

void AddToMempool(CMutableTransaction& tx)
{
    mempool.addUnchecked(tx.GetHash(), CTxMemPoolEntry(tx, 0, GetTime(), 111.0, chainActive.Height()));
}

bool CreateBlock(CBlockTemplate* pblocktemplate)
{
    static int unique_block_counter = 0;
    CBlock* pblock = &pblocktemplate->block;
    pblock->nVersion = 1;
    pblock->nTime = chainActive.Tip()->GetMedianTimePast()+1;
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = CScript() << CScriptNum(unique_block_counter++) << CScriptNum(chainActive.Height());
    txCoinbase.vout[0].nValue = GetBlockSubsidy(chainActive.Height() + 1, Params().GetConsensus());
    pblock->vtx[0] = CTransaction(txCoinbase);
    pblock->hashMerkleRoot = pblock->ComputeMerkleRoot();
    for (int i = 0; ; ++i)
    {
        pblock->nNonce = i;
        if (CheckProofOfWork(pblock->GetHash(), pblock->nBits, Params().GetConsensus()))
        {
            break;
        }
    }
    CValidationState state;
    bool success = (ProcessNewBlock(state, NULL, pblock, true, NULL) && state.IsValid() && pblock->GetHash() == chainActive.Tip()->GetBlockHash());
    pblock->hashPrevBlock = pblock->GetHash();
    return success;
}

bool RemoveBlock(uint256& blockhash)
{
    CValidationState state;
    if (mapBlockIndex.count(blockhash) == 0)
        return false;
    CBlockIndex* pblockindex = mapBlockIndex[blockhash];
    InvalidateBlock(state, pblockindex);
    if (state.IsValid())
    {
        ActivateBestChain(state);
    }
    return state.IsValid();

}

bool CreateCoinbases(unsigned int num_coinbases, std::vector<CTransaction>& coinbases)
{
    CBlockTemplate *pblocktemplate;
    coinbases.clear();
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == 1);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < 100 + num_coinbases; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
        if (coinbases.size() < num_coinbases)
            coinbases.push_back(CTransaction(pblocktemplate->block.vtx[0]));
    }
    delete pblocktemplate;
    return true;
}

bool CreateBlocks(unsigned int num_blocks, unsigned int num_txs)
{
    CBlockTemplate *pblocktemplate;
    BOOST_CHECK(pblocktemplate = CreateNewBlock(scriptPubKey));
    BOOST_CHECK(pblocktemplate->block.vtx.size() == num_txs);
    pblocktemplate->block.hashPrevBlock = chainActive.Tip()->GetBlockHash();
    for (unsigned int i = 0; i < num_blocks; ++i)
    {
        BOOST_CHECK(CreateBlock(pblocktemplate));
    }
    delete pblocktemplate;
    return true;
}

BOOST_AUTO_TEST_CASE(claimtrie_merkle_hash)
{
    int unused;
    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    CMutableTransaction tx1 = BuildTransaction(hash0);
    CMutableTransaction tx2 = BuildTransaction(tx1.GetHash());
    CMutableTransaction tx3 = BuildTransaction(tx2.GetHash());
    CMutableTransaction tx4 = BuildTransaction(tx3.GetHash());
    CMutableTransaction tx5 = BuildTransaction(tx4.GetHash());
    CMutableTransaction tx6 = BuildTransaction(tx5.GetHash());

    uint256 hash1;
    hash1.SetHex("4bbf61ec5669c721bf007c71c59f85e6658f1de7b4562078e22f69f8f7ebcafd");
    
    uint256 hash2;
    hash2.SetHex("33d00d8f4707eb0abef7297a7ff4975e7354fe1ed81455f28301dbceb939494d");
    
    uint256 hash3;
    hash3.SetHex("eb19bbaeecfd6dee77a8cb69286ac01226caee3c3883f55b86d0ca5a2f4252d7");
    
    uint256 hash4;
    hash4.SetHex("a889778ba28603294c1e5c7cec469a9019332ec93838b2f1331ebba547c5fd22");

    BOOST_CHECK(pclaimTrie->empty());

    CClaimTrieCache ntState(pclaimTrie, false);
    ntState.insertClaimIntoTrie(std::string("test"), CClaimValue(tx1.GetHash(), 0, 50, 100, 200));
    ntState.insertClaimIntoTrie(std::string("test2"), CClaimValue(tx2.GetHash(), 0, 50, 100, 200));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!ntState.empty());
    BOOST_CHECK(ntState.getMerkleHash() == hash1);

    ntState.insertClaimIntoTrie(std::string("test"), CClaimValue(tx3.GetHash(), 0, 50, 101, 201));
    BOOST_CHECK(ntState.getMerkleHash() == hash1);
    ntState.insertClaimIntoTrie(std::string("tes"), CClaimValue(tx4.GetHash(), 0, 50, 100, 200));
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.insertClaimIntoTrie(std::string("testtesttesttest"), CClaimValue(tx5.GetHash(), 0, 50, 100, 200));
    ntState.removeClaimFromTrie(std::string("testtesttesttest"), tx5.GetHash(), 0, unused);
    BOOST_CHECK(ntState.getMerkleHash() == hash2);
    ntState.flush();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState1(pclaimTrie, false);
    ntState1.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("test2"), tx2.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);
    ntState1.removeClaimFromTrie(std::string("tes"), tx4.GetHash(), 0, unused);

    BOOST_CHECK(ntState1.getMerkleHash() == hash0);

    CClaimTrieCache ntState2(pclaimTrie, false);
    ntState2.insertClaimIntoTrie(std::string("abab"), CClaimValue(tx6.GetHash(), 0, 50, 100, 200));
    ntState2.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);

    BOOST_CHECK(ntState2.getMerkleHash() == hash3);

    ntState2.flush();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash3);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState3(pclaimTrie, false);
    ntState3.insertClaimIntoTrie(std::string("test"), CClaimValue(tx1.GetHash(), 0, 50, 100, 200));
    BOOST_CHECK(ntState3.getMerkleHash() == hash4);
    ntState3.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash4);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState4(pclaimTrie, false);
    ntState4.removeClaimFromTrie(std::string("abab"), tx6.GetHash(), 0, unused);
    BOOST_CHECK(ntState4.getMerkleHash() == hash2);
    ntState4.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState5(pclaimTrie, false);
    ntState5.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);

    BOOST_CHECK(ntState5.getMerkleHash() == hash2);
    ntState5.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState6(pclaimTrie, false);
    ntState6.insertClaimIntoTrie(std::string("test"), CClaimValue(tx3.GetHash(), 0, 50, 101, 201));

    BOOST_CHECK(ntState6.getMerkleHash() == hash2);
    ntState6.flush();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash2);
    BOOST_CHECK(pclaimTrie->checkConsistency());

    CClaimTrieCache ntState7(pclaimTrie, false);
    ntState7.removeClaimFromTrie(std::string("test"), tx3.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("test"), tx1.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("tes"), tx4.GetHash(), 0, unused);
    ntState7.removeClaimFromTrie(std::string("test2"), tx2.GetHash(), 0, unused);
    
    BOOST_CHECK(ntState7.getMerkleHash() == hash0);
    ntState7.flush();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pclaimTrie->checkConsistency());
}

BOOST_AUTO_TEST_CASE(claimtrie_insert_update_claim)
{
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);
    
    LOCK(cs_main);
    //Checkpoints::fEnabled = false;

    std::string sName1("atest");
    std::string sName2("btest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName1(sName1.begin(), sName1.end());
    std::vector<unsigned char> vchName2(sName2.begin(), sName2.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(4, coinbases));

    uint256 hash0(uint256S("0000000000000000000000000000000000000000000000000000000000000001"));
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    
    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx3 = BuildTransaction(tx1);
    tx3.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx4 = BuildTransaction(tx2);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;
    CMutableTransaction tx5 = BuildTransaction(coinbases[2]);
    tx5.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName2 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;
    CMutableTransaction tx7 = BuildTransaction(coinbases[3]);
    tx7.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx7.vout[0].nValue = tx1.vout[0].nValue - 1;
    CMutableTransaction tx8 = BuildTransaction(tx3, 0, 2);
    tx8.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx8.vout[0].nValue = tx8.vout[0].nValue - 1;
    tx8.vout[1].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName1 << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    CClaimValue val;

    std::vector<uint256> blocks_to_invalidate;
    
    // Verify updates to the best claim get inserted immediately, and others don't.
    
    // Put tx1, tx2, and tx7 into the blockchain, and then advance 100 blocks to put them in the trie
    
    AddToMempool(tx1);
    AddToMempool(tx2);
    AddToMempool(tx7);

    BOOST_CHECK(CreateBlocks(1, 4));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pcoinsTip->HaveCoins(tx7.GetHash()));

    BOOST_CHECK(CreateBlocks(99, 1));
    
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName2, val));
    
    BOOST_CHECK(CreateBlocks(1, 1));
    
    // Verify tx1 and tx2 are in the trie

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Verify tx7 is also in the trie

    BOOST_CHECK(pclaimTrie->haveClaim(sName1, tx7.GetHash(), 0));

    // Spend tx1 with tx3, tx2 with tx4, and put in tx5.
    
    AddToMempool(tx3);
    AddToMempool(tx4);
    AddToMempool(tx5);

    BOOST_CHECK(CreateBlocks(1, 4));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    // Verify tx1, tx2, and tx5 are not in the trie, but tx3 is in the trie.

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName2, val));

    // Roll back the last block, make sure tx1 and tx2 are put back in the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Roll all the way back, make sure all txs are out of the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(!pclaimTrie->getInfoForName(sName2, val));
    mempool.clear();
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->getMerkleHash() == hash0);
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Test undoing a claim before the claim gets into the trie

    // Put tx1 in the chain, and then undo that block.
    
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // Make sure it's not in the queue

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing a claim that has gotten into the trie

    // Put tx1 in the chain, and then advance until it's in the trie

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(99, 1));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // Remove it from the trie

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spend which involves a claim just inserted into the queue

    // Immediately spend tx2 with tx4, verify nothing gets put in the trie

    AddToMempool(tx2);
    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 3));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spend which involves a claim inserted into the queue by a previous block
    
    // Put tx2 into the chain, and then advance a few blocks but not far enough for it to get into the trie

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(49, 1));
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // Spend tx2 with tx4, and then advance to where tx2 would be inserted into the trie and verify it hasn't happened

    AddToMempool(tx4);
    
    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(50, 1));
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // Undo spending tx2 with tx4, and then advance and verify tx2 is inserted into the trie when it should be

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    mempool.clear();

    BOOST_CHECK(CreateBlocks(50, 1));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName2, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // Test undoing a spend which involves a claim in the trie

    // spend tx2, which is in the trie, with tx4
    
    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // undo spending tx2 with tx4, and verify tx2 is back in the trie
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // roll back to the beginning
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing a spent update which updated a claim still in the queue

    // Create the original claim (tx1)

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // move forward some, but not far enough for the claim to get into the trie

    BOOST_CHECK(CreateBlocks(49, 1));
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // update the original claim (tx3 spends tx1)

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // spend the update (tx6 spends tx3)

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // undo spending the update (undo tx6 spending tx3)

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    mempool.clear();

    // make sure the update (tx3) still goes into effect in 100 blocks

    BOOST_CHECK(CreateBlocks(99, 1));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    // undo updating the original claim (tx3 spends tx1)

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    mempool.clear();

    // Test undoing an spent update which updated the best claim to a name

    // move forward until the original claim is inserted into the trie

    BOOST_CHECK(CreateBlocks(50, 1));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // update the original claim (tx3 spends tx1)
    
    AddToMempool(tx3);
    
    BOOST_CHECK(CreateBlocks(1, 2));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());
    
    // spend the update (tx6 spends tx3)
    
    AddToMempool(tx6);
    
    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    
    // undo spending the update (undo tx6 spending tx3)
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx3.GetHash());

    // Test having two updates to a claim in the same transaction

    // Add both txouts (tx8 spends tx3)
    
    AddToMempool(tx8);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    // ensure txout 0 made it into the trie and txout 1 did not
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx8.GetHash() && val.nOut == 0);

    // roll forward until tx8 output 1 gets into the trie

    BOOST_CHECK(CreateBlocks(99, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    // ensure txout 1 made it into the trie and is now in control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName1, val));
    BOOST_CHECK(val.txhash == tx8.GetHash() && val.nOut == 1);

    // roll back to before tx8

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
}

BOOST_AUTO_TEST_CASE(claimtrie_claim_expiration)
{
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue("testa");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue(sValue.begin(), sValue.end());

    std::vector<CTransaction> coinbases;
   
    BOOST_CHECK(CreateCoinbases(2, coinbases));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue << OP_2DROP << OP_DROP << OP_TRUE;
    CMutableTransaction tx2 = BuildTransaction(tx1);
    tx2.vout[0].scriptPubKey = CScript() << OP_TRUE;

    std::vector<uint256> blocks_to_invalidate;
    // set expiration time to 100 blocks after the block becomes valid. (more correctly, 200 blocks after the block is created)

    pclaimTrie->setExpirationTime(200);

    // create a claim. verify no expiration event has been scheduled.

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the claim is valid. verify the expiration event is scheduled.

    BOOST_CHECK(CreateBlocks(99, 1));

    BOOST_CHECK(pclaimTrie->empty());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.
    
    BOOST_CHECK(CreateBlocks(99, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    
    // roll forward a bit and then roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.

    BOOST_CHECK(CreateBlocks(100, 1));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    
    // advance until the expiration event occurs. verify the expiration event occurs on time.
    
    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
    
    // roll back to before the claim is valid. verify the claim is removed but the expiration event still exists.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the claim is valid again. verify the expiration event is scheduled.

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // advance until the expiration event occurs. verify the expiration event occurs on time.

    BOOST_CHECK(CreateBlocks(50, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    BOOST_CHECK(CreateBlocks(49, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify the expiration event is scheduled.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // roll back some.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // spend the claim. verify the expiration event is removed.
    
    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    
    // roll back the spend. verify the expiration event is returned.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());
   
    mempool.clear();
    
    // advance until the expiration event occurs. verify the event occurs on time.
    
    BOOST_CHECK(CreateBlocks(50, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // spend the expired claim

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // undo the spend. verify everything remains empty.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    mempool.clear();
    
    // roll back to before the expiration event. verify the claim is reinserted. verify the expiration event is scheduled again.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // verify the expiration event happens at the right time again

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());

    // roll back to before the expiration event. verify it gets reinserted and expiration gets scheduled.

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // roll back to before the claim is valid. verify the claim is removed but the expiration event is not.
    
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->expirationQueueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->expirationQueueEmpty());
    BOOST_CHECK(blocks_to_invalidate.empty());
}

BOOST_AUTO_TEST_CASE(claimtrie_supporting_claims)
{
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(3, coinbases));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 100000000;

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 500000000;

    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    uint256 tx1Hash = tx1.GetHash();
    std::vector<unsigned char> vchTx1Hash(tx1Hash.begin(), tx1Hash.end());
    std::vector<unsigned char> vchSupportnOut = uint32_t_to_vch(0);
    tx3.vout[0].scriptPubKey = CScript() << OP_SUPPORT_CLAIM << vchName << vchTx1Hash << vchSupportnOut << OP_2DROP << OP_2DROP << OP_TRUE;
    tx3.vout[0].nValue = 500000000;

    CMutableTransaction tx4 = BuildTransaction(tx1);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx5 = BuildTransaction(tx2);
    tx5.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;

    // Test 1: create 1 LBC claim (tx1), create 5 LBC support (tx3), create 5 LBC claim (tx2)
    // Verify that tx1 retains control throughout
    // spend tx3, verify that tx2 gains control
    // roll back to before tx3 is valid
    // advance until tx3 and tx2 are valid, verify tx1 retains control
    // spend tx3, verify tx2 gains control
    // roll back to before tx3 is spent, verify tx1 gains control
    // roll back to before tx2 is valid, spend tx3
    // advance to tx2 valid, verify tx2 gains control
    // roll back to before tx3 is valid, spend tx3
    // advance to tx2 valid, verify tx2 gains control
    // roll back to insertion of tx3, and don't insert it
    // insert tx2, advance until it is valid, verify tx2 gains control

    // Put tx1 in the blockchain

    AddToMempool(tx1);
    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(19, 1));

    // Put tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(19, 1));

    // Put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(59, 1));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    
    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance until tx3 is valid
    
    BOOST_CHECK(CreateBlocks(19, 1));

    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // spend tx3

    AddToMempool(tx6);
    
    BOOST_CHECK(CreateBlocks(1, 2));

    // verify tx2 gains control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());
    
    // roll back to before tx3 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    // advance until tx3 is valid
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // spend tx3
    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    // verify tx2 gains control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // undo spending tx3

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // verify tx1 has control again

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // roll back to before tx2 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // spend tx3

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));

    // Verify tx2 gains control

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // roll back to before tx3 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // spend tx3

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // roll back to before tx3 is inserted
    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(20, 1));

    // Put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(59, 1));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    
    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(39, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
}

BOOST_AUTO_TEST_CASE(claimtrie_supporting_claims2)
{
    BOOST_CHECK(pclaimTrie->nCurrentHeight == chainActive.Height() + 1);

    LOCK(cs_main);

    std::string sName("atest");
    std::string sValue1("testa");
    std::string sValue2("testb");

    std::vector<unsigned char> vchName(sName.begin(), sName.end());
    std::vector<unsigned char> vchValue1(sValue1.begin(), sValue1.end());
    std::vector<unsigned char> vchValue2(sValue2.begin(), sValue2.end());

    std::vector<CTransaction> coinbases;

    BOOST_CHECK(CreateCoinbases(3, coinbases));

    CMutableTransaction tx1 = BuildTransaction(coinbases[0]);
    tx1.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue1 << OP_2DROP << OP_DROP << OP_TRUE;
    tx1.vout[0].nValue = 100000000;

    CMutableTransaction tx2 = BuildTransaction(coinbases[1]);
    tx2.vout[0].scriptPubKey = CScript() << OP_CLAIM_NAME << vchName << vchValue2 << OP_2DROP << OP_DROP << OP_TRUE;
    tx2.vout[0].nValue = 500000000;

    CMutableTransaction tx3 = BuildTransaction(coinbases[2]);
    uint256 tx1Hash = tx1.GetHash();
    std::vector<unsigned char> vchTx1Hash(tx1Hash.begin(), tx1Hash.end());
    std::vector<unsigned char> vchSupportnOut = uint32_t_to_vch(0);
    tx3.vout[0].scriptPubKey = CScript() << OP_SUPPORT_CLAIM << vchName << vchTx1Hash << vchSupportnOut << OP_2DROP << OP_2DROP << OP_TRUE;
    tx3.vout[0].nValue = 500000000;

    CMutableTransaction tx4 = BuildTransaction(tx1);
    tx4.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx5 = BuildTransaction(tx2);
    tx5.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CMutableTransaction tx6 = BuildTransaction(tx3);
    tx6.vout[0].scriptPubKey = CScript() << OP_TRUE;

    CClaimValue val;
    std::vector<uint256> blocks_to_invalidate;
    
    // Test 2: create 1 LBC claim (tx1), create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 loses control when tx2 becomes valid, and then tx1 gains control when tx3 becomes valid
    // Then, verify that tx2 regains control when A) tx3 is spent and B) tx3 is undone
    
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // advance 20 blocks
    
    BOOST_CHECK(CreateBlocks(19, 1));

    // put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx2.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance 20 blocks

    BOOST_CHECK(CreateBlocks(19, 1));

    // put tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pcoinsTip->HaveCoins(tx3.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(59, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(19, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // advance until tx3 is valid

    BOOST_CHECK(CreateBlocks(19, 1));

    BOOST_CHECK(CreateBlocks(1, 1));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // spend tx3

    AddToMempool(tx6);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // undo spend

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // roll back to before tx3 is valid

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Test 4: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3)
    // Verify that tx1 retains control throughout

    // put tx1 into the blockchain

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pcoinsTip->HaveCoins(tx1.GetHash()));
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(99, 1));

    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // put tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // advance some, insert tx3, should be immediately valid

    BOOST_CHECK(CreateBlocks(49, 1));

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // advance until tx2 is valid, verify tx1 retains control

    BOOST_CHECK(CreateBlocks(49, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // Test 5: create 5 LBC claim (tx2), wait till valid, create 1 LBC claim (tx1), create 5 LBC support (tx3)
    // Verify that tx2 retains control until support becomes valid

    // insert tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(99, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // insert tx1 into the block chain

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // advance some
    
    BOOST_CHECK(CreateBlocks(49, 1));

    // insert tx3 into the block chain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(49, 1));
    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());

    // advance until tx3 is valid

    BOOST_CHECK(CreateBlocks(49, 1));
    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());


    // Test 6: create 1 LBC claim (tx1), wait till valid, create 5 LBC claim (tx2), create 5 LBC support (tx3), spend tx1
    // Verify that tx1 retains control until it is spent

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(99, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // insert tx2 into the blockchain

    AddToMempool(tx2);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance some, insert tx3

    BOOST_CHECK(CreateBlocks(49, 1));

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // advance until tx2 is valid

    BOOST_CHECK(CreateBlocks(49, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // spend tx1

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx2.GetHash());
    
    // undo spend of tx1

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    BOOST_CHECK(pclaimTrie->getInfoForName(sName, val));
    BOOST_CHECK(val.txhash == tx1.GetHash());
    
    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // Test 7: create 1 LBC claim (tx1), wait till valid, create 5 LBC support (tx3), spend tx1
    // Verify name trie is empty

    // insert tx1 into blockchain
    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(99, 1));
    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // insert tx3 into blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // spent tx1

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // Test 8: create 1 LBC claim (tx1), create 5 LBC support (tx3), wait till tx1 valid, spend tx1, wait till tx3 valid
    // Verify name trie is empty

    // insert tx1 into the blockchain

    AddToMempool(tx1);

    BOOST_CHECK(CreateBlocks(1, 2));
    blocks_to_invalidate.push_back(chainActive.Tip()->GetBlockHash());

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());
    
    // move forward a bit

    BOOST_CHECK(CreateBlocks(49, 1));

    // insert tx3 into the blockchain

    AddToMempool(tx3);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(!pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    
    // advance until tx1 is valid

    BOOST_CHECK(CreateBlocks(49, 1));

    BOOST_CHECK(CreateBlocks(1, 1));

    BOOST_CHECK(!pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    
    // spend tx1

    AddToMempool(tx4);

    BOOST_CHECK(CreateBlocks(1, 2));

    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(pclaimTrie->supportEmpty());
    BOOST_CHECK(!pclaimTrie->supportQueueEmpty());
    
    // advance until tx3 is valid

    BOOST_CHECK(CreateBlocks(48, 1));

    BOOST_CHECK(CreateBlocks(1, 1));
    
    BOOST_CHECK(pclaimTrie->empty());
    BOOST_CHECK(pclaimTrie->queueEmpty());
    BOOST_CHECK(!pclaimTrie->supportEmpty());
    BOOST_CHECK(pclaimTrie->supportQueueEmpty());

    // roll all the way back

    BOOST_CHECK(RemoveBlock(blocks_to_invalidate.back()));
    blocks_to_invalidate.pop_back();
}

BOOST_AUTO_TEST_CASE(claimtrienode_serialize_unserialize)
{
    CDataStream ss(SER_DISK, 0);

    CClaimTrieNode n1;
    CClaimTrieNode n2;
    CClaimValue throwaway;
    
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.hash.SetHex("0000000000000000000000000000000000000000000000000000000000000001");
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.hash.SetHex("a79e8a5b28f7fa5e8836a4b48da9988bdf56ce749f81f413cb754f963a516200");
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    CClaimValue v1(uint256S("0000000000000000000000000000000000000000000000000000000000000001"), 0, 50, 0, 100);
    CClaimValue v2(uint256S("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), 1, 100, 1, 101);

    n1.insertClaim(v1);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.insertClaim(v2);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeClaim(v1.txhash, v1.nOut, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);

    n1.removeClaim(v2.txhash, v2.nOut, throwaway);
    BOOST_CHECK(n1 != n2);
    ss << n1;
    ss >> n2;
    BOOST_CHECK(n1 == n2);
}

BOOST_AUTO_TEST_SUITE_END()
