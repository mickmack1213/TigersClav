#include "SSLGameLog.hpp"
#include <filesystem>
#include "util/gzstream.h"
#include "util/easylogging++.h"
#include <cstring>

const std::set<SSLMessageType> SSLGameLog::RECORDED_MESSAGES = {
                MESSAGE_SSL_VISION_2010,
                MESSAGE_SSL_REFBOX_2013,
                MESSAGE_SSL_VISION_2014,
                MESSAGE_SSL_VISION_TRACKER_2020 };

SSLGameLog::SSLGameLog(std::string filename, std::set<SSLMessageType> loadMsgTypes)
:shouldAbortLoading_(false),
 isLoaded_(false),
 activePoolUsage_(0),
 firstTimestamp_ns_(-1),
 lastTimestamp_ns_(-1)
{
    memoryPools_.emplace_back(std::vector<uint8_t>(MEM_POOL_CHUNK_SIZE));

    loaderThread_ = std::thread(SSLGameLog::loader, this, filename, loadMsgTypes);
}

SSLGameLog::~SSLGameLog()
{
    shouldAbortLoading_ = true;

    if(loaderThread_.joinable())
        loaderThread_.join();
}

bool SSLGameLog::isValid() const
{
    SSLGameLogStats stats = getStats();

    if(stats.type != std::string("SSL_LOG_FILE"))
        return false;

    if(stats.formatVersion != 1)
        return false;

    if(stats.numMessages == 0)
        return false;

    return true;
}

void SSLGameLog::loader(std::string filename, std::set<SSLMessageType> loadMsgTypes)
{
    // prepare statistics
    for(auto msgType : RECORDED_MESSAGES)
    {
        stats_.numMessagesPerType[msgType] = 0;
        messagesByType_[msgType] = MsgMap();
    }

    LOG(TRACE) << "Trying to load gamelog: " << filename;

    // Open logfile directly or through gzip stream
    std::filesystem::path filepath(filename);
    std::unique_ptr<std::istream> pFile;

    if(filepath.extension() == ".gz")
        pFile = std::make_unique<igzstream>(filepath.string().c_str(), std::ios::binary | std::ios::in);
    else
        pFile = std::make_unique<std::ifstream>(filepath.string(), std::ios::binary);

    filename_ = filepath.stem().string();

    // read header information (magic and version)
    char buf[12];
    pFile->read(buf, 12);

    stats_.type = std::string(buf, 12);

    if(stats_.type != std::string("SSL_LOG_FILE"))
    {
        isLoaded_ = true;
        return;
    }

    stats_.formatVersion = readInt32(*pFile);
    stats_.totalSize = 16;

    if(stats_.formatVersion != 1)
    {
        isLoaded_ = true;
        return;
    }

    LOG(TRACE) << "Valid header detected, loading messages...";

    // read full gamelog
    while(*pFile)
    {
        if(shouldAbortLoading_)
        {
            break;
        }

        // read message header
        SSLGameLogMsgHeader header;

        header.timestamp_ns = readInt64(*pFile);
        header.type = readInt32(*pFile);
        header.size = readInt32(*pFile);

        if(!*pFile)
            break;

        const SSLMessageType msgType = static_cast<SSLMessageType>(header.type);

        if(header.size < 0)
            break;

        if(firstTimestamp_ns_ < 0)
            firstTimestamp_ns_ = header.timestamp_ns;

        // Update live statistics
        {
            std::lock_guard<std::mutex> statsLock(statsMutex_);

            stats_.totalSize += header.size + sizeof(SSLGameLogMsgHeader);
            stats_.numMessages++;

            if(RECORDED_MESSAGES.find(msgType) != RECORDED_MESSAGES.end())
            {
                stats_.numMessagesPerType[msgType]++;
                stats_.duration_s = (header.timestamp_ns - firstTimestamp_ns_) * 1e-9;
                lastTimestamp_ns_ = header.timestamp_ns;
            }
        }

        // If this message is not blacklisted copy it to memory pool
        if(loadMsgTypes.find(msgType) != loadMsgTypes.end())
        {
            uint8_t* pBuf = alloc(header.size + sizeof(header));

            memcpy(pBuf, &header, sizeof(header));

            pFile->read((char*)(pBuf + sizeof(header)), header.size);

            if(!*pFile)
                break;

            messagesByType_[msgType][header.timestamp_ns] = reinterpret_cast<SSLGameLogMsgHeader*>(pBuf);
        }
        else
        {
            // otherwise just ignore it
            pFile->ignore(header.size);
        }
    }

    LOG(INFO) << "Loaded gamelog: " << filename;

    isLoaded_ = true;
}

SSLGameLogStats SSLGameLog::getStats() const
{
    SSLGameLogStats copy;

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        copy = stats_;
    }

    return copy;
}

SSLGameLog::MsgMapIter SSLGameLog::findFirstMsgAfterTimestamp(SSLMessageType type, int64_t timestamp) const
{
    return messagesByType_.at(type).upper_bound(timestamp);
}

SSLGameLog::MsgMapIter SSLGameLog::findLastMsgBeforeTimestamp(SSLMessageType type, int64_t timestamp) const
{
    // this may also return an exact iterator if there is one at exactly "timestamp"
    SSLGameLog::MsgMapIter iter = messagesByType_.at(type).upper_bound(timestamp);
    if(iter != messagesByType_.at(type).begin())
        iter--;

    return iter;
}


uint8_t* SSLGameLog::alloc(size_t size)
{
    if(activePoolUsage_ + size > MEM_POOL_CHUNK_SIZE)
    {
        memoryPools_.emplace_back(std::vector<uint8_t>(MEM_POOL_CHUNK_SIZE));
        activePoolUsage_ = 0;
    }

    uint8_t* pMem = memoryPools_.back().data() + activePoolUsage_;
    activePoolUsage_ += size;

    return pMem;
}

int32_t SSLGameLog::readInt32(std::istream& file)
{
    uint8_t buf[4];
    file.read((char*)buf, 4);

    return (((uint32_t)buf[0]) << 24) | (((uint32_t)buf[1]) << 16) | (((uint32_t)buf[2]) << 8) | ((uint32_t)buf[3]);
}

int64_t SSLGameLog::readInt64(std::istream& file)
{
    uint8_t buf[8];
    file.read((char*)buf, 8);

    return (uint64_t)buf[0] << 56 |
           (uint64_t)buf[1] << 48 |
           (uint64_t)buf[2] << 40 |
           (uint64_t)buf[3] << 32 |
           (uint64_t)buf[4] << 24 |
           (uint64_t)buf[5] << 16 |
           (uint64_t)buf[6] << 8 |
           (uint64_t)buf[7];
}
