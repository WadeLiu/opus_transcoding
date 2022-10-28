#include "converter.h"

//------------------------------------------------------------------------------

CConverter::CConverter()
{
    m_demuxer = std::make_unique<CDemuxer>();
    m_muxer   = std::make_unique<CMuxer>();
}

//------------------------------------------------------------------------------

CConverter::~CConverter()
{

}

//------------------------------------------------------------------------------

bool CConverter::convert(const char* input, const char* output)
{
    AVFormatContext* ifmt_ctx = nullptr;
    AVFormatContext* ofmt_ctx = nullptr;

    AVStream* video_stream = nullptr;
    AVStream* audio_stream = nullptr;

    const AVOutputFormat* ofmt = nullptr;

    AVPacket* pkt = nullptr;

    int video_stream_idx = -1, audio_stream_idx = -1;
    int stream_index = 0;

    AVDictionary *opt = nullptr;

    const AVCodec *audio_codec;

    /* open input file, and allocate format context */
    if (avformat_open_input(&ifmt_ctx, input, nullptr, nullptr) < 0)
    {
        fprintf(stderr, "ERROR: Could not open source file %s\n", input);
        return false;
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0)
    {
        fprintf(stderr, "ERROR: Could not find stream information\n");
        avformat_close_input(&ifmt_ctx);
        return false;
    }

    /* dump input information to stderr */
    av_dump_format(ifmt_ctx, 0, input, 0);

    /* allocate the output media context */
    avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, output);
    if (!ofmt_ctx)
    {
        return false;
    }

    ofmt = ofmt_ctx->oformat;

    int stream_mapping_size = ifmt_ctx->nb_streams;
    int* stream_mapping = (int*)av_calloc(stream_mapping_size, sizeof(int));
    if (!stream_mapping)
    {
        return false;
    }

    for (int i = 0; i < stream_mapping_size; ++i)
    {
        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;

        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE)
        {
            stream_mapping[i] = -1;
            continue;
        }

        stream_mapping[i] = stream_index++;

        out_stream = avformat_new_stream(ofmt_ctx, nullptr);
        if (!out_stream)
        {
            fprintf(stderr, "Failed allocating output stream\n");
            exit(1);
        }

        if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
            if (!audio_codec)
            {
                fprintf(stderr, "Could not find encoder for '%s'\n",
                        avcodec_get_name(AV_CODEC_ID_OPUS));
                exit(1);
            }

            AVCodecContext *ctx = avcodec_alloc_context3(audio_codec);
            if (!ctx)
            {
                fprintf(stderr, "Could not alloc an encoding context\n");
                exit(1);
            }


        }
        else
        {
            if (avcodec_parameters_copy(out_stream->codecpar, in_codecpar) < 0)
            {
                fprintf(stderr, "Failed to copy codec parameters\n");
                exit(1);
            }
            out_stream->codecpar->codec_tag = 0;
        }
    }

    av_dump_format(ofmt_ctx, 0, output, 1);

    if (!(ofmt->flags & AVFMT_NOFILE))
    {
        if (avio_open(&ofmt_ctx->pb, output, AVIO_FLAG_WRITE) < 0)
        {
            fprintf(stderr, "Could not open output file '%s'", output);
            exit(1);
        }
    }

    if (avformat_write_header(ofmt_ctx, &opt) < 0)
    {
        fprintf(stderr, "Error occurred when opening output file\n");
        exit(1);
    }

    pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "ERROR: Could not allocate packet\n");
        return false;
    }

    while (av_read_frame(ifmt_ctx, pkt) >= 0)
    {
        AVStream *in_stream, *out_stream;

        in_stream  = ifmt_ctx->streams[pkt->stream_index];
        if (pkt->stream_index >= stream_mapping_size ||
            stream_mapping[pkt->stream_index] < 0)
        {
            av_packet_unref(pkt);
            continue;
        }

        pkt->stream_index = stream_mapping[pkt->stream_index];
        out_stream = ofmt_ctx->streams[pkt->stream_index];
        // log_packet(ifmt_ctx, pkt, "in");

        /* copy packet */
        av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;
        // log_packet(ofmt_ctx, pkt, "out");


        if (av_interleaved_write_frame(ofmt_ctx, pkt) < 0)
        {
            fprintf(stderr, "Error muxing packet\n");
            break;
        }
    }

    av_write_trailer(ofmt_ctx);

    // if (open_codec_context(&video_stream_idx, &m_video_dec_ctx, ifmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0)
    // {
    //     video_stream = ifmt_ctx->streams[video_stream_idx];
    // }
    //
    // if (open_codec_context(&audio_stream_idx, &m_audio_dec_ctx, ifmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0)
    // {
    //     audio_stream = ifmt_ctx->streams[audio_stream_idx];
    // }
    //
    // if (!audio_stream && !video_stream)
    // {
    //     fprintf(stderr, "ERROR: Could not find audio or video stream in the input, aborting\n");
    //     return false;
    // }
    //
    // m_frame = av_frame_alloc();
    // if (!m_frame)
    // {
    //     fprintf(stderr, "ERROR: Could not allocate frame\n");
    //     return false;
    // }
    //
    // pkt = av_packet_alloc();
    // if (!pkt)
    // {
    //     fprintf(stderr, "ERROR: Could not allocate packet\n");
    //     return false;
    // }
    //
    // /* read frames from the file */
    // int ret = 0;
    // while (av_read_frame(ifmt_ctx, pkt) >= 0)
    // {
    //     // check if the packet belongs to a stream we are interested in, otherwise
    //     // skip it
    //     if (pkt->stream_index == video_stream_idx)
    //     {
    //         ret = decode_packet(m_video_dec_ctx, pkt);
    //     }
    //     else if (pkt->stream_index == audio_stream_idx)
    //     {
    //         ret = decode_packet(m_audio_dec_ctx, pkt);
    //     }
    //
    //     av_packet_unref(pkt);
    //
    //     if (ret < 0)
    //     {
    //         break;
    //     }
    // }
    //
    // /* flush the decoders */
    // if (m_video_dec_ctx)
    // {
    //     decode_packet(m_video_dec_ctx, nullptr);
    // }
    //
    // if (m_audio_dec_ctx)
    // {
    //     decode_packet(m_audio_dec_ctx, nullptr);
    // }

    printf("Demuxing succeeded.\n");

    // if (video_stream)
    // {
    //     printf("Play the output video file with the command:\n"
    //            "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n",
    //            av_get_pix_fmt_name(pix_fmt), width, height,
    //            video_dst_filename);
    // }
    //
    // if (audio_stream)
    // {
    //     enum AVSampleFormat sfmt = m_audio_dec_ctx->sample_fmt;
    //     int n_channels = m_audio_dec_ctx->ch_layout.nb_channels;
    //     const char *fmt;
    //
    //     if (av_sample_fmt_is_planar(sfmt)) {
    //         const char *packed = av_get_sample_fmt_name(sfmt);
    //         printf("Warning: the sample format the decoder produced is planar "
    //                "(%s). This example will output the first channel only.\n",
    //                packed ? packed : "?");
    //         sfmt = av_get_packed_sample_fmt(sfmt);
    //         n_channels = 1;
    //     }
    //
    //     if ((ret = get_format_from_sample_fmt(&fmt, sfmt)) < 0)
    //         goto end;
    //
    //     printf("Play the output audio file with the command:\n"
    //            "ffplay -f %s -ac %d -ar %d %s\n",
    //            fmt, n_channels, m_audio_dec_ctx->sample_rate,
    //            audio_dst_filename);
    // }

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
    {
        avio_closep(&ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);

    av_freep(&stream_mapping);

    avcodec_free_context(&m_video_dec_ctx);
    avcodec_free_context(&m_audio_dec_ctx);
    avformat_close_input(&ifmt_ctx);

    av_packet_free(&pkt);
    av_frame_free(&m_frame);

    return true;
}

//------------------------------------------------------------------------------

int CConverter::open_codec_context(int* stream_idx,
                                   AVCodecContext** dec_ctx,
                                   AVFormatContext* fmt_ctx,
                                   enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = nullptr;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file\n",
                av_get_media_type_string(type));
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

//------------------------------------------------------------------------------

int CConverter::decode_packet(AVCodecContext* dec, const AVPacket* pkt)
{
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0)
    {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }

    // get all the available frames from the decoder
    while (ret >= 0)
    {
        ret = avcodec_receive_frame(dec, m_frame);
        if (ret < 0)
        {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
            {
                return 0;
            }

            fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
            return ret;
        }

        // write the frame data to output file
        if (dec->codec->type == AVMEDIA_TYPE_VIDEO)
        {
            ret = output_video_frame(m_frame);
        }
        else
        {
            ret = output_audio_frame(m_frame);
        }

        av_frame_unref(m_frame);

        if (ret < 0)
        {
            return ret;
        }
    }

    return 0;
}

//------------------------------------------------------------------------------

int CConverter::output_video_frame(AVFrame* frame)
{
    // if (frame->width != width || frame->height != height ||
    //     frame->format != pix_fmt) {
    //     /* To handle this change, one could call av_image_alloc again and
    //      * decode the following frames into another rawvideo file. */
    //     fprintf(stderr, "Error: Width, height and pixel format have to be "
    //             "constant in a rawvideo file, but the width, height or "
    //             "pixel format of the input video changed:\n"
    //             "old: width = %d, height = %d, format = %s\n"
    //             "new: width = %d, height = %d, format = %s\n",
    //             width, height, av_get_pix_fmt_name(pix_fmt),
    //             frame->width, frame->height,
    //             av_get_pix_fmt_name(frame->format));
    //     return -1;
    // }

    printf("video_frame n:%d coded_picture_number:%d\n",
           m_video_frame_count++, frame->coded_picture_number);

    /* copy decoded frame to destination buffer:
     * this is required since rawvideo expects non aligned data */
    // av_image_copy(video_dst_data, video_dst_linesize,
    //               (const uint8_t **)(frame->data), frame->linesize,
    //               pix_fmt, width, height);
    //
    // /* write to rawvideo file */
    // fwrite(video_dst_data[0], 1, video_dst_bufsize, video_dst_file);
    return 0;
}

//------------------------------------------------------------------------------

int CConverter::output_audio_frame(AVFrame* frame)
{
    size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
    printf("audio_frame n:%d nb_samples:%d format:%d pts:%s\n",
           m_audio_frame_count++, frame->nb_samples, frame->format,
           av_ts2timestr(frame->pts, &m_audio_dec_ctx->time_base));

    /* Write the raw audio data samples of the first plane. This works
     * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
     * most audio decoders output planar audio, which uses a separate
     * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
     * In other words, this code will write only the first audio channel
     * in these cases.
     * You should use libswresample or libavfilter to convert the frame
     * to packed data. */

    // in_sample_fmt = AV_SAMPLE_FMT_FLTP;
    // out_sample_fmt = AV_SAMPLE_FMT_16;
    // swr = swr_alloc_set_opts(swr, out_ch_layout, out_sample_fmt,  out_sample_rate,
    //                      in_ch_layout,  in_sample_fmt,  in_sample_rate,
    //                                  0, 0);
    // swr_init(swr)
    // swr_convert(swr, pOutBuffer, out_count, pInBuffer, frame->nb_samples);

    return 0;
}

//------------------------------------------------------------------------------
