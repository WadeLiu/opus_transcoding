#include "App.h"

constexpr int MAX_BUFFER_SIZE = 10 * 1024 * 1024;

struct HEADER
{
    int number;
    short len;
};

struct DATA_IDX
{
    int number;
    uintptr_t address;
    short len;
};

App::App(const char* domainName, int port)
{
    m_domainName = std::string(domainName);
    m_port = port;

    m_buffer = new char[MAX_BUFFER_SIZE];
    memset(m_buffer, 0x00, MAX_BUFFER_SIZE);
}

App::~App()
{
    if (m_buffer != nullptr)
    {
        delete[] m_buffer;
    }
}

void App::exec()
{
    std::thread receiveThread(&App::receiveFunc, this);
    std::thread parseThread(&App::parseFunc, this);
    receiveThread.join();
    parseThread.join();
}

void App::receiveFunc()
{
    struct sockaddr_in destAddress = {0};

    struct hostent* hostInfo = gethostbyname(m_domainName.c_str());
    if (hostInfo == nullptr)
    {
        return;
    }

    char *ip = inet_ntoa(*(struct in_addr*)*hostInfo->h_addr_list);

    destAddress.sin_family = AF_INET;
    destAddress.sin_port = htons(49152);
    memcpy((char *) &destAddress.sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);

    int sockfd = 0;
    if ((sockfd = socket(destAddress.sin_family, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Error : Could not create socket \n");
        return;
    }

    if (connect(sockfd, (struct sockaddr *)&destAddress, sizeof(destAddress)) < 0)
    {
       printf("\n Error : Connect Failed \n");
       return;
    }

    int len = 0;
    while ((len = recv(sockfd, m_buffer + m_writePos, MAX_BUFFER_SIZE - m_writePos, 0)) > 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_writePos += len;

        printf("recv len(%d)\n", m_writePos);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    close(sockfd);

    m_done = true;
}

void App::parseFunc()
{
    std::vector<DATA_IDX> vecData;

    while (true)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if ((m_writePos == m_readPos) && m_done)
        {
            break;
        }

        if ((m_writePos - m_readPos) < sizeof(HEADER))
        {
            continue;
        }

        HEADER* header = (HEADER*)(m_buffer + m_readPos);
        int packageNumber   = __bswap_32(header->number);   // be32toh(header->number)
        short packageLen    = __bswap_16(header->len);      // be16toh(header->len)

        printf("packageNumber(%d,%d), packageLen(%d,%d)\n", packageNumber, htonl(header->number), packageLen, htons(header->len));

        if ((m_writePos - m_readPos) < (sizeof(HEADER) + packageLen))
        {
            continue;
        }

        // get payload
        char* payload = m_buffer + (m_readPos + sizeof(HEADER));
        vecData.push_back({packageNumber, (uintptr_t)payload, packageLen});

        m_readPos += (sizeof(HEADER) + packageLen);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::sort(vecData.begin(), vecData.end(), [](const DATA_IDX lhs, const DATA_IDX rhs)
    {
        return lhs.number < rhs.number;
    });

    FILE* pFile = fopen("ffd8_ffd9", "wb");

    for (auto& data : vecData)
    {
        printf("seq = %d\n", data.number);
        fwrite((const void*)data.address, 1, data.len, pFile);
    }

    fclose(pFile);

    printf("Total count = %ld, writePos(%d), readPos(%d)\n", vecData.size(), m_writePos, m_readPos);
}
