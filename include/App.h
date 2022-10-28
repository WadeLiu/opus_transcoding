#pragma pack(1)

#include <string>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <algorithm>
#include <vector>

class App
{
public:
    App(const char* domainName, int port);
    ~App();

    void exec();

private:
    std::string m_domainName;
    int m_port{80};
    char* m_buffer{nullptr};

    int m_readPos{0};
    int m_writePos{0};

    bool m_done{false};

    std::mutex m_mutex;

    void receiveFunc();
    void parseFunc();
};
