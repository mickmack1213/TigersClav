#pragma once

#include <memory>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <optional>
#include <set>
#include <deque>

#include "ssl_gc_referee_message.pb.h"
#include "ssl_vision_wrapper.pb.h"
#include "ssl_vision_wrapper_tracked.pb.h"

enum SSLMessageType
{
    MESSAGE_BLANK = 0,
    MESSAGE_UNKNOWN = 1,
    MESSAGE_SSL_VISION_2010 = 2,
    MESSAGE_SSL_REFBOX_2013 = 3,
    MESSAGE_SSL_VISION_2014 = 4,
    MESSAGE_SSL_VISION_TRACKER_2020 = 5,
    MESSAGE_SSL_INDEX_2021 = 6,
    MESSAGE_LAST = 7,
};

struct SSLGameLogStats
{
    std::string type;
    int32_t formatVersion{0};

    uint32_t totalSize{0};
    uint32_t numMessages{0};
    double duration_s{0.0};

    std::map<SSLMessageType, uint32_t> numMessagesPerType;
};

struct __attribute__((packed)) SSLGameLogMsgHeader
{
    int64_t timestamp_ns;
    int32_t type;
    int32_t size;
};

class SSLGameLog
{
public:
    typedef std::map<int64_t, SSLGameLogMsgHeader*> MsgMap;
    typedef MsgMap::const_iterator MsgMapIter;

    SSLGameLog(std::string filename, std::set<SSLMessageType> loadMsgTypes = RECORDED_MESSAGES);
    ~SSLGameLog();

    SSLGameLogStats getStats() const;
    bool isLoaded() const { return isLoaded_; }
    void abortLoading() { shouldAbortLoading_ = true; }

    int64_t getFirstTimestamp_ns() const { return firstTimestamp_ns_; }
    int64_t getLastTimestamp_ns() const { return lastTimestamp_ns_; }

    MsgMapIter begin(SSLMessageType type) const { return messagesByType_.at(type).begin(); }
    MsgMapIter end(SSLMessageType type) const { return messagesByType_.at(type).end(); }

    MsgMapIter findFirstMsgAfterTimestamp(SSLMessageType type, int64_t timestamp) const;
    MsgMapIter findLastMsgBeforeTimestamp(SSLMessageType type, int64_t timestamp) const;

    template<typename ProtoType>
    std::optional<ProtoType> convertTo(MsgMapIter& iter);

private:
    void loader(std::string filename, std::set<SSLMessageType> loadMsgTypes);
    uint8_t* alloc(size_t size);

    int32_t readInt32(std::istream& file);
    int64_t readInt64(std::istream& file);

    std::thread loaderThread_;

    std::atomic<bool> shouldAbortLoading_;
    std::atomic<bool> isLoaded_;

    SSLGameLogStats stats_;
    mutable std::mutex statsMutex_;

    int64_t firstTimestamp_ns_;
    int64_t lastTimestamp_ns_;

    static const std::set<SSLMessageType> RECORDED_MESSAGES;
    static constexpr size_t MEM_POOL_CHUNK_SIZE = 16*1024*1024;

    std::deque<std::vector<uint8_t>> memoryPools_;
    size_t activePoolUsage_;

    std::map<SSLMessageType, MsgMap> messagesByType_;
};

template<typename ProtoType>
std::optional<ProtoType> SSLGameLog::convertTo(SSLGameLog::MsgMapIter& iter)
{
    ProtoType protoMsg;

    const uint8_t* pProtoStart = reinterpret_cast<const uint8_t*>(iter->second) + sizeof(SSLGameLogMsgHeader);

    if(protoMsg.ParseFromArray(pProtoStart, iter->second->size))
    {
        return std::optional<ProtoType>(std::move(protoMsg));
    }

    return std::optional<ProtoType>();
}
