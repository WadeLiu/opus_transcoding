#include "demuxer.h"
#include "muxer.h"

//------------------------------------------------------------------------------

class CConverter
{
public:
    CConverter();
    virtual ~CConverter();

    bool convert(const char* input, const char* output);

private:
    std::unique_ptr<CDemuxer> m_demuxer;
    std::unique_ptr<CMuxer>   m_muxer;

    AVCodecContext* m_video_dec_ctx{nullptr};
    AVCodecContext* m_audio_dec_ctx{nullptr};
    AVFrame* m_frame{nullptr};
    int m_video_frame_count{0};
    int m_audio_frame_count{0};

    int open_codec_context(int* stream_idx,
                           AVCodecContext** dec_ctx,
                           AVFormatContext* fmt_ctx,
                           enum AVMediaType type);

    int decode_packet(AVCodecContext* dec, const AVPacket* pkt);

    int output_video_frame(AVFrame* frame);
    int output_audio_frame(AVFrame* frame);
};

//------------------------------------------------------------------------------
