#include "converter.h"

//------------------------------------------------------------------------------

CConverter::CConverter()
{
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

    const AVOutputFormat* ofmt = nullptr;

    int stream_index = 0;

    AVDictionary *opt = nullptr;

    AVCodecContext *audioDecoderContext{nullptr};
    AVCodecContext *audioEncoderContext{nullptr};

    const AVCodec *audioEncoder, *audioDecoder;

    SwrContext *swr_ctx{nullptr};

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
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
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
            // deocder ---------------------------------------------------------
            audioDecoder = avcodec_find_decoder(in_stream->codecpar->codec_id);
            if (!audioDecoder)
            {
                fprintf(stderr, "Could not find decoder for '%s'\n",
                        avcodec_get_name(in_stream->codecpar->codec_id));
                exit(1);
            }

            audioDecoderContext = avcodec_alloc_context3(audioDecoder);
            if (!audioDecoderContext)
            {
                fprintf(stderr, "Could not alloc an decoder context\n");
                exit(1);
            }

            /* Copy codec parameters from input stream to output codec context */
            if (avcodec_parameters_to_context(audioDecoderContext, in_stream->codecpar) < 0)
            {
                fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                        av_get_media_type_string(AVMEDIA_TYPE_AUDIO));
                exit(1);
            }

            /* Init the decoder */
            if (avcodec_open2(audioDecoderContext, audioDecoder, nullptr) < 0)
            {
                fprintf(stderr, "Failed to open %s codec\n",
                        av_get_media_type_string(AVMEDIA_TYPE_AUDIO));
                exit(1);
            }

            // encoder ---------------------------------------------------------
            audioEncoder = avcodec_find_encoder(AV_CODEC_ID_OPUS);
            if (!audioEncoder)
            {
                fprintf(stderr, "Could not find encoder for '%s'\n",
                        avcodec_get_name(AV_CODEC_ID_OPUS));
                exit(1);
            }

            audioEncoderContext = avcodec_alloc_context3(audioEncoder);
            if (!audioEncoderContext)
            {
                fprintf(stderr, "Could not alloc an encoding context\n");
                exit(1);
            }

            audioEncoderContext->sample_rate = audioEncoder->supported_samplerates[0];
            audioEncoderContext->time_base = (AVRational){1, audioEncoderContext->sample_rate};
            audioEncoderContext->channels = 1;
            audioEncoderContext->channel_layout = AV_CH_LAYOUT_MONO;
            audioEncoderContext->sample_fmt = audioEncoderContext->codec->sample_fmts[0];
            audioEncoderContext->bit_rate = 16000;

            /* Init the encoder */
            if (avcodec_open2(audioEncoderContext, audioEncoder, nullptr) < 0)
            {
                fprintf(stderr, "Failed to open %s codec\n",
                        av_get_media_type_string(AVMEDIA_TYPE_AUDIO));
                exit(1);
            }

            if (avcodec_parameters_from_context(out_stream->codecpar, audioEncoderContext) < 0)
            {
                exit(1);
            }

            // -----------------------------------------------------------------
            swr_ctx = swr_alloc();
            if (!swr_ctx)
            {
                fprintf(stderr, "Could not allocate resampler context\n");
                exit(1);
            }

            av_opt_set_int(swr_ctx, "in_channel_count", audioDecoderContext->channels, 0);
            av_opt_set_int(swr_ctx, "in_sample_rate", audioDecoderContext->sample_rate, 0);
            av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", audioDecoderContext->sample_fmt, 0);

            av_opt_set_int(swr_ctx, "out_channel_count", audioEncoderContext->channels, 0);
            av_opt_set_int(swr_ctx, "out_sample_rate", audioEncoderContext->sample_rate, 0);
            av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", audioEncoderContext->sample_fmt, 0);

            if (swr_init(swr_ctx) < 0)
            {
                fprintf(stderr, "Failed to initialize the resampling context\n");
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

    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "ERROR: Could not allocate frame\n");
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "ERROR: Could not allocate packet\n");
        return false;
    }

    //-----------------------------------
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
    //-----------------------------------

    /* read frames from the file */
    int64_t resamplePos = 0;
    int streamIndex = -1;
    while (av_read_frame(ifmt_ctx, pkt) >= 0)
    {
        AVStream *in_stream = nullptr, *out_stream = nullptr;
        streamIndex = pkt->stream_index;

        if (pkt->stream_index >= stream_mapping_size ||
            stream_mapping[pkt->stream_index] < 0)
        {
            av_packet_unref(pkt);
            continue;
        }

        in_stream  = ifmt_ctx->streams[pkt->stream_index];
        out_stream = ofmt_ctx->streams[pkt->stream_index];

        if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
            pkt->pos = -1;

            printf("video_frame pts:%s(%d) pts_time:%s dts:%s(%d) dts_time:%s duration:%s duration_time:%s num/den:%d/%d\n",
                   av_ts2str(pkt->pts), pkt->pts, av_ts2timestr(pkt->pts, &ofmt_ctx->streams[pkt->stream_index]->time_base),
                   av_ts2str(pkt->dts), pkt->dts, av_ts2timestr(pkt->dts, &ofmt_ctx->streams[pkt->stream_index]->time_base),
                   av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, &ofmt_ctx->streams[pkt->stream_index]->time_base),
                   ofmt_ctx->streams[pkt->stream_index]->time_base.num, ofmt_ctx->streams[pkt->stream_index]->time_base.den);

            if (av_interleaved_write_frame(ofmt_ctx, pkt) < 0)
            {
                fprintf(stderr, "Error muxing packet\n");
                break;
            }
        }
        else if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            // Audio decode ----------------------------------------------------
            int ret = avcodec_send_packet(audioDecoderContext, pkt);
            if (ret < 0)
            {
                fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
                exit(1);
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(audioDecoderContext, frame);
                if (ret < 0)
                {
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                    {
                        av_packet_unref(pkt);
                        continue;
                    }

                    fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
                    break;
                }

                // Encode audio in opus ----------------------------------------

                // resampling
                if (frame->nb_samples != audioEncoderContext->frame_size)
                {
                    AVFrame *reasamplingFrame = av_frame_alloc();
                    if (!reasamplingFrame)
                    {
                        fprintf(stderr, "Could not allocate oframe_a\n");
                        exit(1);
                    }

                    reasamplingFrame->format            = audioEncoderContext->sample_fmt;
                    reasamplingFrame->channel_layout    = audioEncoderContext->channel_layout;
                    reasamplingFrame->channels          = audioEncoderContext->channels;
                    reasamplingFrame->sample_rate       = audioEncoderContext->sample_rate;
                    reasamplingFrame->nb_samples        = audioEncoderContext->frame_size;

                    if (av_frame_get_buffer(reasamplingFrame, 0) < 0)
                    {
                        fprintf(stderr, "Error allocating an oframe_a buffer\n");
                        exit(1);
                    }

                    if (frame->nb_samples != audioEncoderContext->frame_size)
                    {
                        if (swr_get_out_samples(swr_ctx, 0) >= audioEncoderContext->frame_size)
                        {
                            if (swr_convert(swr_ctx, reasamplingFrame->data,
                                            audioEncoderContext->frame_size, nullptr, 0) < 0)
                            {
                                fprintf(stderr, "resampling cache failed\n");
                                exit(1);
                            }
                        }
                        else
                        {
                            if (swr_convert(swr_ctx, reasamplingFrame->data,
                                            audioEncoderContext->frame_size,
                                            (const uint8_t **)frame->data,
                                            frame->nb_samples) < 0)
                            {
                                fprintf(stderr, "resampling data failed\n");
                                exit(1);
                            }
                        }
                    }

                    if (frame->nb_samples != audioEncoderContext->frame_size)
                    {
                        resamplePos += audioEncoderContext->frame_size;

                        reasamplingFrame->pts = resamplePos;
                        reasamplingFrame->pkt_dts = resamplePos;
                    }

                    ret = avcodec_send_frame(audioEncoderContext, reasamplingFrame);
                }
                else
                {
                    ret = avcodec_send_frame(audioEncoderContext, frame);
                }

                if (ret < 0)
                {
                    exit(1);
                }

                while (ret >= 0)
                {
                    ret = avcodec_receive_packet(audioEncoderContext, pkt);

                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    {
                        continue;
                    }
                    pkt->stream_index = streamIndex;

                    printf("audio_frame(%d) pts:%s(%d) pts_time:%s dts:%s(%d) dts_time:%s duration:%s duration_time:%s num/den:%d/%d \n",
                           pkt->stream_index,
                           av_ts2str(pkt->pts), pkt->pts, av_ts2timestr(pkt->pts, &ofmt_ctx->streams[pkt->stream_index]->time_base),
                           av_ts2str(pkt->dts), pkt->dts, av_ts2timestr(pkt->dts, &ofmt_ctx->streams[pkt->stream_index]->time_base),
                           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, &ofmt_ctx->streams[pkt->stream_index]->time_base),
                           ofmt_ctx->streams[pkt->stream_index]->time_base.num, ofmt_ctx->streams[pkt->stream_index]->time_base.den);

                    // pktAudio = av_packet_clone(pkt);
                    if (av_interleaved_write_frame(ofmt_ctx, pkt) < 0)
                    {
                        fprintf(stderr, "Error muxing packet\n");
                        break;
                    }
                }
                // -------------------------------------------------------------

                av_frame_unref(frame);

                if (ret < 0)
                {
                    break;
                }
            }
        }

        av_packet_unref(pkt);
    }

    av_write_trailer(ofmt_ctx);

    fprintf(stderr, "Completed.\n");

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
    {
        avio_closep(&ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);

    av_freep(&stream_mapping);

    avformat_close_input(&ifmt_ctx);

    av_packet_free(&pkt);
    av_frame_free(&frame);


    return true;
}

//------------------------------------------------------------------------------

int CConverter::writeFrame(AVFormatContext* ctx, AVPacket* pkt)
{
    int ret = av_interleaved_write_frame(ctx, pkt);
    if (ret < 0)
    {
        fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
        exit(1);
    }
    return ret;
}

//------------------------------------------------------------------------------
