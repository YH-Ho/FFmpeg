#ifndef FFMPEG_H
#define FFMPEG_H

#include <assert.h>
#include "BlockingQueue.h"

extern "C"{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#include "libavutil/pixfmt.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/imgutils.h"
}

class FFmpeg
{
public:
    FFmpeg();

    int init();

    int open_input_source(const char *url);

    void data_subcontracting(BlockingQueue<AVPacket*> &videoqueue,
                             BlockingQueue<AVPacket*> &audioqueue);

    void decode_video_frame(AVPacket* &pkt, AVFrame* &dst_frame);
    void decode_audio_frame(AVPacket* &pkt, AVFrame* &dst_frame);

    int init_converted_video_frame();
    int init_converted_audio_frame();

    int video_frame_swr_convert(const AVFrame* src_frame);
    int audio_frame_swr_convert(const AVFrame* src_frame);

    double get_video_frame_pts(const AVFrame* src_frame);
    double get_audio_frame_pts(const AVFrame* src_frame);

    double synchronize(AVFrame* src_frame, double pts);

    int release();


private:
    AVFormatContext *m_format_ctx;

    AVCodecContext *m_video_codec_ctx;
    AVCodecContext *m_audio_codec_ctx;

    const AVCodec *m_video_codec;
    const AVCodec *m_audio_codec;

    AVFrame	*m_video_frame;
    AVFrame	*m_audio_frame;
    AVFrame	*m_converted_video_frame;
    AVFrame	*m_converted_audio_frame;

    struct SwsContext *m_video_convert_ctx;
    struct SwrContext *m_audio_convert_ctx;

    int m_video_stream;
    int m_audio_stream;

    uint8_t *m_converted_video_buffer;
    uint8_t *m_converted_audio_buffer;

    int m_converted_video_buffer_size;
    int m_converted_audio_buffer_size;

    uint64_t m_original_ch_layout;
    uint64_t m_convert_ch_layout;

    enum AVSampleFormat m_original_sample_fmt;
    enum AVSampleFormat m_convert_sample_fmt;

    int m_original_sample_rate;
    int m_convert_sample_rate;

    int m_nb_channels;

    /* For test */
    double video_clock;
};

#endif // FFMPEG_H
