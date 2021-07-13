// Copyright (c) 2021 Bitcoin Association
// Distributed under the Open BSV software license, see the accompanying file
// LICENSE.

#include "miner_id.h"

#include <array>
#include <string_view>

#include "hash.h"
#include "logging.h"
#include "pubkey.h"

using namespace std;

namespace
{
    // Parse dataRefs field from coinbase document.
    // If signature of current coinbase document is valid, we expect valid
    // transaction references in datarefs field. But it can happen that
    // referenced transactions are not found due to various reasons. Here, we
    // only store transactions and not check their existence. This happens later
    // in the process.
    bool parseDataRefs(const UniValue& coinbaseDocument,
                       std::vector<CoinbaseDocument::DataRef>& dataRefs)
    {
        if(!coinbaseDocument.exists("dataRefs"))
            return true;

        // If dataRefs are present, they have to have the correct structure.
        const auto& data_refs{coinbaseDocument["dataRefs"]};

        if(!data_refs.isObject() || !data_refs.exists("refs") ||
           !data_refs["refs"].isArray())
        {
            return false;
        }

        const UniValue refs = data_refs["refs"].get_array();

        for(size_t i = 0; i < refs.size(); i++)
        {
            if(refs[i].exists("brfcIds") && refs[i]["brfcIds"].isArray() &&
               refs[i].exists("txid") && refs[i]["txid"].isStr() &&
               refs[i].exists("vout") && refs[i]["vout"].isNum())
            {
                std::vector<std::string> brfcIds;
                for(size_t brfcIdx = 0; brfcIdx < refs[i]["brfcIds"].size();
                    brfcIdx++)
                {
                    if(!refs[i]["brfcIds"][brfcIdx].isStr())
                    {
                        // Incorrect structure of member in dataRefs list.
                        return false;
                    }
                    brfcIds.push_back(refs[i]["brfcIds"][brfcIdx].get_str());
                }
                dataRefs.push_back(CoinbaseDocument::DataRef{
                    brfcIds,
                    uint256S(refs[i]["txid"].get_str()),
                    refs[i]["vout"].get_int()});
            }
            else
            {
                // Incorrect structure of member in dataRefs list.
                return false;
            }
        }

        return true;
    }

    template <typename O>
    void hash_sha256(const string_view msg, O o)
    {
        CSHA256()
            .Write(reinterpret_cast<const uint8_t*>(msg.data()), msg.size())
            .Finalize(o);
    }

    bool verify(const string_view msg,
                const vector<uint8_t>& pub_key,
                const vector<uint8_t>& sig)
    {
        std::vector<uint8_t> hash(CSHA256::OUTPUT_SIZE);
        hash_sha256(msg, hash.data());

        const CPubKey pubKey{pub_key.begin(), pub_key.end()};
        return pubKey.Verify(uint256{hash}, sig);
    }
} // namespace

bool MinerId::SetStaticCoinbaseDocument(
    const UniValue& document,
    const std::vector<uint8_t>& signatureBytes,
    const COutPoint& tx_out,
    int32_t blockHeight)
{
    auto LogInvalidDoc = [&] {
        LogPrint(
            BCLog::TXNVAL,
            "One or more required parameters from coinbase document missing or "
            "incorrect. Coinbase transaction txid %s and output number %d. \n",
            tx_out.GetTxId().ToString(),
            tx_out.GetN());
    };

    // Check existence and validity of required fields of static coinbase
    // document.
    const auto& version = document["version"];
    if(!version.isStr() || !SUPPORTED_VERSIONS.count(version.get_str()))
    {
        LogInvalidDoc();
        return false;
    }

    auto& height = document["height"];
    if(!height.isStr())
    {
        LogInvalidDoc();
        return false;
    }
    const auto block_height{std::stoi(height.get_str())};
    if(block_height != blockHeight)
    {
        LogPrint(BCLog::TXNVAL,
                 "Block height in coinbase document is incorrect in coinbase "
                 "transaction with txid %s and output number %d. \n",
                 tx_out.GetTxId().ToString(),
                 tx_out.GetN());
        return false;
    }

    auto& prevMinerId = document["prevMinerId"];
    if(!prevMinerId.isStr())
    {
        LogInvalidDoc();
        return false;
    }

    auto& prevMinerIdSig = document["prevMinerIdSig"];
    if(!prevMinerIdSig.isStr())
    {
        LogInvalidDoc();
        return false;
    }

    auto& minerId = document["minerId"];
    if(!minerId.isStr())
    {
        LogInvalidDoc();
        return false;
    }

    auto& vctx = document["vctx"];
    if(!vctx.isObject())
    {
        LogInvalidDoc();
        return false;
    }

    auto& vctxTxid = vctx["txId"];
    if(!vctxTxid.isStr())
    {
        LogInvalidDoc();
        return false;
    }

    auto& vctxVout = vctx["vout"];
    if(!vctxVout.isNum())
    {
        LogInvalidDoc();
        return false;
    }

    // Verify signature of static document miner id.
    const std::string cd_json{document.write()};
    const std::vector<uint8_t> minerIdBytes = ParseHex(minerId.get_str());
    if(!verify(cd_json, minerIdBytes, signatureBytes))
    {
        LogPrint(BCLog::TXNVAL,
                 "Signature of static coinbase document is invalid in coinbase "
                 "transaction with txid %s and output number %d. \n",
                 tx_out.GetTxId().ToString(),
                 tx_out.GetN());
        return false;
    }

    // Verify signature of previous miner id.
    std::string dataToSign{prevMinerId.get_str() + minerId.get_str() +
                           vctxTxid.get_str()};
    if(version.get_str() == "0.1")
    {
    }
    else if(version.get_str() == "0.2")
    {
        const std::string dataToSignHex = HexStr(dataToSign);
        dataToSign = dataToSignHex;
    }
    else
    {
        LogPrint(BCLog::TXNVAL,
                 "Unsupported version in miner id in txid %s and output number "
                 "%d. \n",
                 tx_out.GetTxId().ToString(),
                 tx_out.GetN());
        return false;
    }

    const vector<uint8_t> signaturePrevMinerId{
        ParseHex(prevMinerIdSig.get_str())};
    const std::vector<uint8_t> prevMinerIdBytes{
        ParseHex(prevMinerId.get_str())};
    if(!verify(dataToSign, prevMinerIdBytes, signaturePrevMinerId))
    {
        LogPrint(
            BCLog::TXNVAL,
            "Signature of previous miner id in coinbase document is invalid in "
            "coinbase transaction with txid %s and output number %d. \n",
            tx_out.GetTxId().ToString(),
            tx_out.GetN());
        return false;
    }

    CoinbaseDocument coinbaseDocument(
        version.get_str(),
        block_height,
        prevMinerId.get_str(),
        prevMinerIdSig.get_str(),
        minerId.get_str(),
        COutPoint(uint256S(vctxTxid.get_str()), vctxVout.get_int()));

    std::vector<CoinbaseDocument::DataRef> dataRefs;
    if(!parseDataRefs(document, dataRefs))
    {
        LogInvalidDoc();
        return false;
    }
    if(dataRefs.size() != 0)
    {
        coinbaseDocument.SetDataRefs(dataRefs);
    }

    // Set static coinbase document.
    coinbaseDocument_ = coinbaseDocument;
    // Set fields needed for verifying dynamic miner id.
    staticDocumentJson_ = document.write();
    signatureStaticDocument_ =
        std::string(signatureBytes.begin(), signatureBytes.end());

    return true;
}

bool MinerId::SetDynamicCoinbaseDocument(
    const UniValue& document,
    const std::vector<uint8_t>& signatureBytes,
    const COutPoint& tx_out,
    int32_t blockHeight)
{
    auto LogInvalidDoc = [&] {
        LogPrint(BCLog::TXNVAL,
                 "Structure in coinbase document is incorrect (incorrect field "
                 "type) in coinbase transaction with txid %s and output number "
                 "%d. \n",
                 tx_out.GetTxId().ToString(),
                 tx_out.GetN());
    };

    // Dynamic document has no required fields (except for dynamic miner id).
    // Check field types if they exist.
    auto& version = document["version"];
    if(!version.isNull() &&
       (!version.isStr() || !SUPPORTED_VERSIONS.count(version.get_str())))
    {
        LogInvalidDoc();
        return false;
    }

    auto& height = document["height"];
    if(!height.isNull())
    {
        if(!height.isNum())
        {
            LogInvalidDoc();
            return false;
        }
        if(height.get_int() != blockHeight)
        {
            LogPrint(
                BCLog::TXNVAL,
                "Block height in coinbase document is incorrect in coinbase "
                "transaction with txid %s and output number %d. \n",
                tx_out.GetTxId().ToString(),
                tx_out.GetN());
            return false;
        }
    }

    auto& prevMinerId = document["prevMinerId"];
    if(!prevMinerId.isNull() && !prevMinerId.isStr())
    {
        LogInvalidDoc();
        return false;
    }

    auto& prevMinerIdSig = document["prevMinerIdSig"];
    if(!prevMinerIdSig.isNull() && !prevMinerIdSig.isStr())
    {
        LogInvalidDoc();
        return false;
    }

    auto& minerId = document["minerId"];
    if(!minerId.isNull() && !minerId.isStr())
    {
        LogInvalidDoc();
        return false;
    }

    auto& dynamicMinerId = document["dynamicMinerId"];
    if(!dynamicMinerId.isStr())
    {
        LogInvalidDoc();
        return false;
    }

    auto& vctx = document["vctx"];
    if(!vctx.isNull())
    {
        if(!vctx.isObject())
        {
            LogInvalidDoc();
            return false;
        }

        auto& vctxTxid = vctx["txId"];
        if(!vctxTxid.isStr())
        {
            LogInvalidDoc();
            return false;
        }

        auto& vctxVout = vctx["vout"];
        if(!vctxVout.isNum())
        {
            LogInvalidDoc();
            return false;
        }
    }

    // Verify signature of dynamic document miner id.
    std::vector<uint8_t> dynamicMinerIdBytes =
        ParseHex(dynamicMinerId.get_str());
    CPubKey dynamicMinerIdPubKey(dynamicMinerIdBytes.begin(),
                                 dynamicMinerIdBytes.end());
    std::string dataToSign =
        staticDocumentJson_ + signatureStaticDocument_ + document.write();

    std::vector<uint8_t> dataToSignBytes =
        std::vector<uint8_t>(dataToSign.begin(), dataToSign.end());
    uint8_t hashSignature[CSHA256::OUTPUT_SIZE];
    CSHA256()
        .Write(dataToSignBytes.data(), dataToSignBytes.size())
        .Finalize(hashSignature);

    if(!dynamicMinerIdPubKey.Verify(
           uint256(std::vector<uint8_t>{std::begin(hashSignature),
                                        std::end(hashSignature)}),
           signatureBytes))
    {
        LogPrint(
            BCLog::TXNVAL,
            "Signature of dynamic miner id in coinbase document is invalid in "
            "coinbase transaction with txid %s and output number %d. \n",
            tx_out.GetTxId().ToString(),
            tx_out.GetN());
        return false;
    }

    // set data refs only if they do not exist already
    if(!coinbaseDocument_.GetDataRefs())
    {
        std::vector<CoinbaseDocument::DataRef> dataRefs;
        if(!parseDataRefs(document, dataRefs))
        {
            LogInvalidDoc();
            return false;
        }
        if(dataRefs.size() != 0)
        {
            coinbaseDocument_.SetDataRefs(dataRefs);
        }
    }

    return true;
}

bool parseCoinbaseDocument(MinerId& minerId,
                           const std::string& coinbaseDocumentDataJson,
                           const std::vector<uint8_t>& signatureBytes,
                           const COutPoint& tx_out,
                           int32_t blockHeight,
                           bool dynamic)
{

    UniValue coinbaseDocumentData;
    if(!coinbaseDocumentData.read(coinbaseDocumentDataJson))
    {
        LogPrint(BCLog::TXNVAL,
                 "Cannot parse coinbase document in coinbase transaction with "
                 "txid %s and output number %d.\n",
                 tx_out.GetTxId().ToString(),
                 tx_out.GetN());
        return false;
    }

    if(!dynamic &&
       !minerId.SetStaticCoinbaseDocument(
           coinbaseDocumentData, signatureBytes, tx_out, blockHeight))
    {
        return false;
    }

    if(dynamic &&
       !minerId.SetDynamicCoinbaseDocument(
           coinbaseDocumentData, signatureBytes, tx_out, blockHeight))
    {
        return false;
    }

    return true;
}

std::optional<MinerId> FindMinerId(const CTransaction& tx, int32_t blockHeight)
{
    MinerId minerId;

    // Scan coinbase transaction outputs for minerId; stop on first valid
    // minerId
    for(size_t i = 0; i < tx.vout.size(); i++)
    {
        // OP_FALSE OP_RETURN 0x04 0xAC1EED88 OP_PUSHDATA Coinbase Document
        if(IsMinerId(tx.vout[i].scriptPubKey))
        {
            const CScript& pubKey = tx.vout[i].scriptPubKey;

            std::vector<uint8_t> msgBytes{};
            opcodetype opcodeRet{};
            // MinerId coinbase documents starts at 7th byte of the output
            // message
            CScript::const_iterator pc{pubKey.begin() + 7};
            if(!pubKey.GetOp(pc, opcodeRet, msgBytes))
            {
                LogPrint(
                    BCLog::TXNVAL,
                    "Failed to extract data for static document of minerId "
                    "from script with txid %s and output number %d.\n",
                    tx.GetId().ToString(),
                    i);
                continue;
            }

            if(msgBytes.empty())
            {
                LogPrint(BCLog::TXNVAL,
                         "Invalid data for MinerId protocol from script with "
                         "txid %s and output number %d.\n",
                         tx.GetId().ToString(),
                         i);
                continue;
            }

            std::vector<uint8_t> signature{};

            if(!pubKey.GetOp(pc, opcodeRet, signature))
            {
                LogPrint(
                    BCLog::TXNVAL,
                    "Failed to extract signature of static document of minerId "
                    "from script with txid %s and output number %d.\n",
                    tx.GetId().ToString(),
                    i);
                continue;
            }

            if(signature.empty())
            {
                LogPrint(BCLog::TXNVAL,
                         "Invalid data for MinerId signature from script with "
                         "txid %s and output number %d.\n",
                         tx.GetId().ToString(),
                         i);
                continue;
            }

            std::string staticCoinbaseDocumentJson =
                std::string(msgBytes.begin(), msgBytes.end());

            if(parseCoinbaseDocument(minerId,
                                     staticCoinbaseDocumentJson,
                                     signature,
                                     COutPoint(tx.GetId(), i),
                                     blockHeight,
                                     false))
            {
                // Static document of MinerId is successful. Check dynamic
                // MinerId.
                if(pc >= tx.vout[i].scriptPubKey.end())
                {
                    // Dynamic miner id is empty. We found first successful
                    // miner id - we can stop looking.
                    return minerId;
                }

                if(!pubKey.GetOp(pc, opcodeRet, msgBytes))
                {
                    LogPrint(BCLog::TXNVAL,
                             "Failed to extract data for dynamic document of "
                             "minerId from script with txid %s and output "
                             "number %d.\n",
                             tx.GetId().ToString(),
                             i);
                    continue;
                }

                if(!pubKey.GetOp(pc, opcodeRet, signature))
                {
                    LogPrint(BCLog::TXNVAL,
                             "Failed to extract signature of dynamic document "
                             "of minerId from script with txid %s and output "
                             "number %d.\n",
                             tx.GetId().ToString(),
                             i);
                    continue;
                }

                std::string dynamicCoinbaseDocumentJson =
                    std::string(msgBytes.begin(), msgBytes.end());
                if(parseCoinbaseDocument(minerId,
                                         dynamicCoinbaseDocumentJson,
                                         signature,
                                         COutPoint(tx.GetId(), i),
                                         blockHeight,
                                         true))
                {
                    return minerId;
                }

                // Successful static coinbase doc, but failed dynamic coinbase
                // doc: let's reset miner id.
                minerId = MinerId();
            }
        }
    }

    return {};
}
