#pragma once
#include "declare.h"

//------------------------------------------------------------------------------

class CDemuxer
{
public:
    CDemuxer();
    virtual ~CDemuxer();

private:
    AVCodecContext* m_video_dec_ctx{nullptr};
    AVCodecContext* m_audio_dec_ctx{nullptr};
    AVFrame* m_frame{nullptr};
    int m_video_frame_count{0};
    int m_audio_frame_count{0};

    FILE *m_audio_dst_file{nullptr};

    int open_codec_context(int* stream_idx,
                           AVCodecContext** dec_ctx,
                           AVFormatContext* fmt_ctx,
                           enum AVMediaType type);

    int decode_packet(AVCodecContext* dec, const AVPacket* pkt);

    int output_video_frame(AVFrame* frame);
    int output_audio_frame(AVFrame* frame);
};

//------------------------------------------------------------------------------
