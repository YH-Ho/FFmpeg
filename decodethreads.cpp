#include "decodethreads.h"

DecodeThreads::DecodeThreads(char *url, size_t queue_max_size)
{
    BlockingQueue<AVPacket *> videoqueue(queue_max_size);
    BlockingQueue<AVPacket *> audioqueue(queue_max_size);

    m_ffmpeg = new FFmpeg;
    m_ffmpeg->init();
    m_ffmpeg->open_input_source(url);

    thread thrad_subcontracting(&FFmpeg::data_subcontracting, m_ffmpeg, ref(videoqueue), ref(audioqueue));
    thread thread_video_decode(&DecodeThreads::decode_video, this, ref(m_ffmpeg), ref(videoqueue));
    thread thread_audio_decode(&DecodeThreads::decode_audio, this, ref(m_ffmpeg), ref(audioqueue));

    thrad_subcontracting.join();
    thread_video_decode.join();
    thread_audio_decode.join();
}

void DecodeThreads::decode_video(FFmpeg* &ffmpeg, BlockingQueue<AVPacket *> &queue) {
    AVFrame *dst_frame = av_frame_alloc();
    while (!m_quit) {
        AVPacket *pkt = *queue.wait_and_pop().get();
        ffmpeg->decode_video_frame(pkt, dst_frame);
        double pts = ffmpeg->get_video_frame_pts(dst_frame);
        cout<<"video"<<pts<<endl;
        av_frame_unref(dst_frame);
        av_packet_free(&pkt);
    }
    av_frame_free(&dst_frame);
    return;
}

void DecodeThreads::decode_audio(FFmpeg* &ffmpeg, BlockingQueue<AVPacket *> &queue) {
    AVFrame *dst_frame = av_frame_alloc();
    while (!m_quit) {
        AVPacket *pkt = *queue.wait_and_pop().get();
        ffmpeg->decode_audio_frame(pkt, dst_frame);
        double pts = ffmpeg->get_audio_frame_pts(dst_frame);
        cout<<"audio"<<pts<<endl;
        av_frame_unref(dst_frame);
        av_packet_free(&pkt);
    }
    av_frame_free(&dst_frame);
    return;
}
