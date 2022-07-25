#include "SSLGameLog.hpp"
#include <filesystem>
#include "util/gzstream.h"

SSLGameLog::SSLGameLog(std::vector<SSLGameLogEntry>&& msgs, SSLGameLogStats stats)
:messages_(msgs), stats_(stats)
{
}

SSLGameLogLoader::SSLGameLogLoader(std::string filename)
:shouldAbort_(false),
 isDone_(false)
{
    loaderThread_ = std::thread(SSLGameLogLoader::loader, this, filename);
}

SSLGameLogLoader::~SSLGameLogLoader()
{
    shouldAbort_ = true;

    if(loaderThread_.joinable())
        loaderThread_.join();
}

void SSLGameLogLoader::loader(std::string filename)
{
    std::unique_ptr<std::istream> pFile;
    size_t logDataSize;

    std::filesystem::path filepath(filename);

    if(filepath.extension() == ".gz")
    {
        pFile = std::make_unique<igzstream>(filepath.string().c_str(), std::ios::binary | std::ios::in);
        pFile->ignore(std::numeric_limits<std::streamsize>::max());
        logDataSize = pFile->gcount();

        pFile = std::make_unique<igzstream>(filepath.string().c_str(), std::ios::binary | std::ios::in);
    }
    else
    {
        pFile = std::make_unique<std::ifstream>(filepath.string(), std::ios::binary);
        pFile->seekg(0, std::ios::end);
        logDataSize = pFile->tellg();
        pFile->seekg(0, std::ios::beg);
    }

    std::cerr << "size: " << logDataSize << std::endl;
/*
    logData_.resize(logDataSize);

    pFile->read(logData_.data(), logDataSize);
*/
    char buf[12];
    pFile->read(buf, 12);

    std::cerr << "FileTypeString: " << buf << std::endl;

    stats_.type = std::string(buf, 12);

    if(stats_.type != std::string("SSL_LOG_FILE"))
    {
        isDone_ = true;
        return;
    }

    stats_.formatVersion = readInt32(*pFile);
    stats_.totalSize = 16;

    std::cerr << "Version: " << stats_.formatVersion << std::endl;

    stats_.numMessagesPerType[SSLMessageType::MESSAGE_SSL_VISION_2010] = 0;
    stats_.numMessagesPerType[SSLMessageType::MESSAGE_SSL_REFBOX_2013] = 0;
    stats_.numMessagesPerType[SSLMessageType::MESSAGE_SSL_VISION_2014] = 0;
    stats_.numMessagesPerType[SSLMessageType::MESSAGE_SSL_VISION_TRACKER_2020] = 0;
    stats_.numMessagesPerType[SSLMessageType::MESSAGE_SSL_INDEX_2021] = 0;
    stats_.numMessagesPerType[SSLMessageType::MESSAGE_BLANK] = 0;
    stats_.numMessagesPerType[SSLMessageType::MESSAGE_UNKNOWN] = 0;

    int64_t firstTimestamp_ns = -1;

    while(*pFile)
    {
        if(shouldAbort_)
        {
            break;
        }

        SSLGameLogEntry msg;

        msg.timestamp_ns = readInt64(*pFile);
        msg.type = readInt32(*pFile);
        msg.size = readInt32(*pFile);
        msg.pMsg = nullptr;

        if(msg.size < 0)
            break;

        if(parseBuffer_.size() < msg.size)
            parseBuffer_.resize(msg.size);

        pFile->read((char*)parseBuffer_.data(), msg.size);

        if(firstTimestamp_ns < 0)
            firstTimestamp_ns = msg.timestamp_ns;

        {
            std::lock_guard<std::mutex> statsLock(statsMutex_);

            stats_.totalSize += msg.size + 16;
            stats_.numMessages++;

            if(msg.type < SSLMessageType::MESSAGE_LAST)
            {
                if(msg.type != SSLMessageType::MESSAGE_SSL_INDEX_2021)
                    stats_.duration_s = (msg.timestamp_ns - firstTimestamp_ns) * 1e-9;

                stats_.numMessagesPerType[static_cast<SSLMessageType>(msg.type)]++;
            }
        }

        switch(msg.type)
        {
            case SSLMessageType::MESSAGE_SSL_VISION_2010:
                break;
            case SSLMessageType::MESSAGE_SSL_REFBOX_2013:
                msg.pMsg = std::make_shared<Referee>();
                break;
            case SSLMessageType::MESSAGE_SSL_VISION_2014:
                msg.pMsg = std::make_shared<SSL_WrapperPacket>();
                break;
            case SSLMessageType::MESSAGE_SSL_VISION_TRACKER_2020:
                msg.pMsg = std::make_shared<TrackerWrapperPacket>();
                break;
            case SSLMessageType::MESSAGE_SSL_INDEX_2021:
            case SSLMessageType::MESSAGE_BLANK:
            case SSLMessageType::MESSAGE_UNKNOWN:
                break;
            default:
                continue;
        }

        if(msg.pMsg)
        {
            if(msg.pMsg->ParseFromArray(parseBuffer_.data(), msg.size))
            {
                messages_.emplace_back(msg);
            }
            else
            {
                // parsing message failed
                std::lock_guard<std::mutex> statsLock(statsMutex_);
                stats_.unparsableMessages++;
            }
        }
    }

    pGameLog_ = std::make_shared<SSLGameLog>(std::move(messages_), stats_);
    pFile = nullptr;
    isDone_ = true;
}

SSLGameLogStats SSLGameLogLoader::getStats() const
{
    SSLGameLogStats copy;

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        copy = stats_;
    }

    return copy;
}

int32_t SSLGameLogLoader::readInt32(std::istream& file)
{
    uint8_t buf[4];
    file.read((char*)buf, 4);

    return (((uint32_t)buf[0]) << 24) | (((uint32_t)buf[1]) << 16) | (((uint32_t)buf[2]) << 8) | ((uint32_t)buf[3]);
}

int64_t SSLGameLogLoader::readInt64(std::istream& file)
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
