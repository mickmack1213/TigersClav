#include "TigersClav.hpp"
#include "LogViewer.hpp"
#include "git_version.h"

#include "util/easylogging++.h"

INITIALIZE_EASYLOGGINGPP

int main(int, char** argv)
{
    el::Configurations defaultConf;
    defaultConf.setToDefault();
    defaultConf.setGlobally(el::ConfigurationType::Format, "%datetime{%y-%M-%d %H:%m:%s.%g} [%levshort] %msg");
    el::Loggers::reconfigureAllLoggers(defaultConf);

    el::Helpers::installLogDispatchCallback<LogViewer>("LogViewer");

    LOG(INFO) << "Starting TIGERs Cut Lengthy Audio Video Editor v" << GIT_VERSION_STR << " from " << GIT_COMMIT_DATE_ISO8601;

    TigersClav clav;

    try
    {
        return clav.run();
    }
    catch(std::exception& ex)
    {
        LOG(ERROR) << "Exception: " << ex.what();
    }
    catch(...)
    {
        LOG(ERROR) << "Unknown exception.";
    }

    return -1;
}
