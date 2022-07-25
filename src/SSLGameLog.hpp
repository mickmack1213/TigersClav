#pragma once

#include <memory>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <optional>
#include <set>

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
    uint32_t unparsableMessages{0};
    double duration_s{0.0};

    std::map<SSLMessageType, uint32_t> numMessagesPerType;
};

struct SSLGameLogEntry
{
    int64_t timestamp_ns;
    int32_t type;
    int32_t size;

    std::shared_ptr<google::protobuf::Message> pMsg;
};

class SSLGameLog2
{
public:
    typedef std::map<int64_t, int64_t> MsgMap;
    typedef MsgMap::const_iterator MsgMapIter;

    SSLGameLog2(std::string filename, std::set<SSLMessageType> blacklistedMsgs = {MESSAGE_BLANK, MESSAGE_UNKNOWN, MESSAGE_SSL_INDEX_2021});
    ~SSLGameLog2();

    bool good() const;

    MsgMapIter begin() const { return msgOffsets_.begin(); }
    MsgMapIter end() const { return msgOffsets_.end(); }

    MsgMapIter findNext(MsgMapIter& iter, SSLMessageType type);
    MsgMapIter findPrevious(MsgMapIter& iter, SSLMessageType type);

    MsgMapIter findFirstMsgAfterTimestamp(int64_t timestamp, SSLMessageType type);
    MsgMapIter findLastMsgBeforeTimestamp(int64_t timestamp, SSLMessageType type);

    template<typename ProtoType>
    std::optional<ProtoType> convertTo(MsgMapIter& iter);

private:
    int removeMsgTypes_;

    std::istream dataStream_;

    std::vector<uint8_t> logData_;

    MsgMap msgOffsets_;
};

class SSLGameLog
{
public:
    SSLGameLog(std::vector<SSLGameLogEntry>&& msgs, SSLGameLogStats stats);

private:
    std::vector<SSLGameLogEntry> messages_;
    SSLGameLogStats stats_;
};

class SSLGameLogLoader
{
public:
    SSLGameLogLoader(std::string filename);
    ~SSLGameLogLoader();

    SSLGameLogStats getStats() const;
    bool isDone() const { return isDone_; }
    void abort() { shouldAbort_ = true; }

    std::shared_ptr<SSLGameLog> getGameLog() const { return pGameLog_; }

private:
    void loader(std::string filename);

    int32_t readInt32(std::istream& file);
    int64_t readInt64(std::istream& file);

    std::thread loaderThread_;

    std::atomic<bool> shouldAbort_;
    std::atomic<bool> isDone_;

    std::vector<uint8_t> parseBuffer_;

    SSLGameLogStats stats_;
    mutable std::mutex statsMutex_;

    std::vector<SSLGameLogEntry> messages_;

    std::shared_ptr<SSLGameLog> pGameLog_;


    // ########
    std::vector<char> logData_;
};
