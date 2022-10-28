#pragma once

#include <sys/stat.h>
#include <iostream>
#include <mutex>
#include <string>
#include <memory>
#include <filesystem>

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

//------------------------------------------------------------------------------

#ifdef av_err2str
#undef av_err2str
av_always_inline std::string av_err2string(int errnum) {
    char str[AV_ERROR_MAX_STRING_SIZE] = {0};
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
#define av_err2str(err) av_err2string(err).c_str()
#endif  // av_err2str

//------------------------------------------------------------------------------

#ifdef av_ts2timestr
#undef av_ts2timestr
av_always_inline std::string av_ts2timestring(int ts, AVRational* tb) {
    char av_ts[AV_TS_MAX_STRING_SIZE] = {0};
    return av_ts_make_time_string(av_ts, ts, tb);
}
#define av_ts2timestr(ts, tb) av_ts2timestring(ts, tb).c_str()
#endif  // av_ts2timestr

//------------------------------------------------------------------------------
