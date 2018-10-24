#include "ffmpeg.h"

FFmpeg::FFmpeg(){
    m_format_ctx = nullptr;
    m_video_codec_ctx = nullptr;
    m_audio_codec_ctx = nullptr;
    m_video_stream = -1;
    m_audio_stream = -1;
}

int FFmpeg::init(){
    /* Initialize all components */
    av_register_all();

    /* Initializing network support */
    avformat_network_init();

    /* Allocate an AVFormatContext */
    m_format_ctx = avformat_alloc_context();

    m_video_frame = av_frame_alloc();
    m_audio_frame = av_frame_alloc();

    return 0;
}

int FFmpeg::open_input_source(const char *url){
    int ret = -1;

    ret = avformat_open_input(&m_format_ctx, url, NULL, NULL);
    if(ret != 0){
        perror("avformat_open_input");
        return ret;
    }

    ret = avformat_find_stream_info(m_format_ctx, NULL);
    if(ret < 0){
        perror("avformat_find_stream_info");
        return ret;
    }
    av_dump_format(m_format_ctx, 0, url, 0);

    /* get duration */
    int duration = m_format_ctx->duration / AV_TIME_BASE;

    printf("duration %d\n", duration);

    /* The number of streams that resource contains */
    unsigned int stream_nums = m_format_ctx->nb_streams;

    for(int i = 0; i < (int)stream_nums; i++){
        AVCodecContext *codec_ctx = m_format_ctx->streams[i]->codec;

        /* Find video codec*/
        if(m_format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            m_video_stream = i;
            m_video_codec_ctx = codec_ctx;
            m_video_codec = avcodec_find_decoder(codec_ctx->codec_id);
            if(!m_video_codec){
                printf("No matching video decoder\n");
            }
            else{
                ret = avcodec_open2(codec_ctx, m_video_codec, NULL);
                if(ret != 0){
                    char buf[1024] = { 0 };
                    av_strerror(ret, buf, sizeof(buf));
                    printf("error: %s\n",buf);
                }
            }
            init_converted_video_frame();
        }

        /* Find audio codec */
        if(m_format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
            m_audio_stream = i;
            m_audio_codec_ctx = codec_ctx;
            m_audio_codec = avcodec_find_decoder(codec_ctx->codec_id);
            if(!m_audio_codec){
                printf("No matching audio decoder\n");
            }
            else{
                ret = avcodec_open2(codec_ctx, m_audio_codec, NULL);
                if(ret != 0){
                    char buf[1024] = { 0 };
                    av_strerror(ret, buf, sizeof(buf));
                    printf("error: %s\n",buf);
                }
            }
            init_converted_audio_frame();
        }

    }

    return 0;
}

int FFmpeg::init_converted_video_frame(){
    m_converted_video_buffer_size = av_image_get_buffer_size(
                AV_PIX_FMT_RGB32,
                m_video_codec_ctx->width,
                m_video_codec_ctx->height,1);

    m_converted_video_buffer = (uint8_t *) av_malloc(m_converted_video_buffer_size * sizeof(uint8_t));

    m_converted_video_frame = av_frame_alloc();

    av_image_fill_arrays(
                m_converted_video_frame->data,
                m_converted_video_frame->linesize,
                m_converted_video_buffer,
                AV_PIX_FMT_RGB32,
                m_video_codec_ctx->width,
                m_video_codec_ctx->height,
                1
                );

    m_video_convert_ctx = sws_getContext(
                m_video_codec_ctx->width,
                m_video_codec_ctx->height,
                m_video_codec_ctx->pix_fmt,
                m_video_codec_ctx->width,
                m_video_codec_ctx->height,
                AV_PIX_FMT_RGB32,
                SWS_BICUBIC,
                NULL, NULL, NULL);
    return 0;
}

int FFmpeg::init_converted_audio_frame(){
    m_original_ch_layout = m_audio_codec_ctx->channel_layout;
    m_convert_ch_layout = AV_CH_LAYOUT_STEREO;

    m_original_sample_fmt = m_audio_codec_ctx->sample_fmt;
    m_convert_sample_fmt = AV_SAMPLE_FMT_S16;

    m_original_sample_rate = m_audio_codec_ctx->sample_rate;
    m_convert_sample_rate = m_original_sample_rate;

    m_audio_convert_ctx = swr_alloc();
    swr_alloc_set_opts(m_audio_convert_ctx,
                       m_convert_ch_layout,
                       m_convert_sample_fmt,
                       m_convert_sample_rate,
                       m_original_ch_layout,
                       m_original_sample_fmt,
                       m_original_sample_rate, 0, nullptr);
    swr_init(m_audio_convert_ctx);

    m_nb_channels = av_get_channel_layout_nb_channels(m_convert_ch_layout);
    m_converted_audio_buffer = (uint8_t *)av_malloc(m_convert_sample_rate * 2);
    return 0;
}

/* get datas from source stream and decode */
void FFmpeg::data_subcontracting(BlockingQueue<AVPacket *> &videoqueue, BlockingQueue<AVPacket *> &audioqueue){
    /* Allocate a packet */
    AVPacket *packet = (AVPacket *)malloc(sizeof(AVPacket));
    av_init_packet(packet);

    while(av_read_frame(m_format_ctx, packet) == 0){
        /* Convert to output video frame  */
        if(packet->stream_index == m_video_stream){
            /* Push the decoded video frame into the queue, must be free after used */
            videoqueue.push(packet);
        }

        /* Convert to output audio frame  */
        else if(packet->stream_index == m_audio_stream){
            /* Push the decoded audio frame into the queue, must be free after used */
            audioqueue.push(packet);
        }

        av_packet_unref(packet);
    }
    av_packet_free(&packet);
}

void FFmpeg::decode_video_frame(AVPacket* &pkt){
    int ret = -1;
    ret = avcodec_send_packet(m_video_codec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(m_video_codec_ctx, m_video_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            return;
        }
        else
            break;
    }
    cout<<m_video_frame->pts<<endl;
    //av_frame_unref(m_video_frame);
}

void FFmpeg::decode_audio_frame(AVPacket* &pkt){
    int ret = -1;
    ret = avcodec_send_packet(m_audio_codec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(m_audio_codec_ctx, m_audio_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            return;
        }
        else
            break;
    }
    cout<<m_audio_frame->pts<<endl;
    //av_frame_unref(m_audio_frame);
}

/* video frame swr convert*/
int FFmpeg::video_frame_swr_convert(const AVFrame *srcFrame){
    int ret = -1;
    ret = sws_scale(m_video_convert_ctx,
                    (const uint8_t* const*)srcFrame->data,
                    srcFrame->linesize,
                    0,
                    m_video_codec_ctx->height,
                    m_converted_video_frame->data,
                    m_converted_video_frame->linesize);
    return ret;
}

/* audio frame swr convert*/
int FFmpeg::audio_frame_swr_convert(const AVFrame *srcFrame){
    int ret = -1;
    ret = swr_convert(m_audio_convert_ctx,
                &m_converted_audio_buffer,
                m_convert_sample_rate * 2,
                (const uint8_t **)srcFrame->data,
                srcFrame->nb_samples);

    m_converted_audio_buffer_size = av_samples_get_buffer_size(NULL,
                                                               m_nb_channels,
                                                               srcFrame->nb_samples,
                                                               m_convert_sample_fmt,1
                                                               );
    return ret;
}

double FFmpeg::get_video_frame_pts(const AVFrame *srcFrame){
    double pts;
    if ((pts = av_frame_get_best_effort_timestamp(srcFrame)) == AV_NOPTS_VALUE)
        pts = 0;
    pts *= av_q2d(m_format_ctx->streams[m_video_stream]->time_base);
    pts = synchronize(m_converted_video_frame, pts);
    return pts;
}

double FFmpeg::get_audio_frame_pts(const AVFrame *srcFrame){
    double pts= srcFrame->pkt_pts * av_q2d(m_format_ctx->streams[m_audio_stream]->time_base);
    return pts;
}

/* for synchronize the video and audio */
double FFmpeg::synchronize(AVFrame *srcFrame, double pts)
{
    double frame_delay;

    if (pts != 0)
        video_clock = pts; // Get pts,then set video clock to it
    else
        pts = video_clock; // Don't get pts,set it to video clock

    frame_delay = av_q2d(m_format_ctx->streams[m_audio_stream]->codec->time_base);
    frame_delay += srcFrame->repeat_pict * (frame_delay * 0.5);

    video_clock += frame_delay;

    return pts;
}

/* release all */
int FFmpeg::release() {
    av_frame_free(&m_video_frame);
    av_frame_free(&m_audio_frame);
    sws_freeContext(m_video_convert_ctx);
    swr_free(&m_audio_convert_ctx);

    av_frame_unref(m_converted_video_frame);
    av_free(m_converted_video_buffer);
    av_free(m_converted_audio_buffer);

    avcodec_close(m_video_codec_ctx);
    avcodec_close(m_audio_codec_ctx);

    avformat_close_input(&m_format_ctx);
    return 0;
}

