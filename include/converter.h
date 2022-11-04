#pragma once
#include "declare.h"

//------------------------------------------------------------------------------

class CConverter
{
public:
    CConverter();
    virtual ~CConverter();

    bool convert(const char* input, const char* output);

private:
    int writeFrame(AVFormatContext* ctx, AVPacket* pkt);
};

//------------------------------------------------------------------------------
