#include "videoplayer.h"
#include <qDebug>

videoplayer::videoplayer(QObject *parent)
    : QObject{parent}
{
    m_timer = new QTimer(this);//创建定时器用于控制视频解码节奏
    connect(m_timer, &QTimer::timeout, this, &videoplayer::decodeFrame);//连接定时器超时信号到解码帧槽函数
}

videoplayer::~videoplayer()
{
    if (m_timer) m_timer->stop();//清理定时器
    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;//清理音频输出设备
    }
    //释放FFmpeg相关资源
    if (m_swrCtx) swr_free(&m_swrCtx);//音频重采样上下文
    if (m_swsCtx) sws_freeContext(m_swsCtx);//视频像素格式转换上下文
    if (m_audioCodecCtx) avcodec_free_context(&m_audioCodecCtx);//音频编解码上下文
    if (m_videoCodecCtx) avcodec_free_context(&m_videoCodecCtx);//视频编解码上下文
    if (m_formatCtx) avformat_close_input(&m_formatCtx);//格式上下文
}
//打开媒体文件
bool videoplayer::openFile(const QString &filename)
{
    //如果已经打开了文件，先清理之前的资源
    if (m_formatCtx) {
        if (m_timer) m_timer->stop();
        if (m_audioSink) {
            m_audioSink->stop();
            delete m_audioSink;
            m_audioSink = nullptr;
        }
        if (m_swrCtx) swr_free(&m_swrCtx);
        if (m_swsCtx) sws_freeContext(m_swsCtx);
        if (m_audioCodecCtx) avcodec_free_context(&m_audioCodecCtx);
        if (m_videoCodecCtx) avcodec_free_context(&m_videoCodecCtx);
        avformat_close_input(&m_formatCtx);
    }

    //打开媒体文件
    if(avformat_open_input(&m_formatCtx,filename.toUtf8().constData(),nullptr,nullptr)!=0) return false;
    //获取流信息
    if(avformat_find_stream_info(m_formatCtx,nullptr)<0) return false;

    for(int i=0;i<m_formatCtx->nb_streams;i++)//遍历所有流，查找视频流和音频流
    {
        AVCodecParameters *codecparams=m_formatCtx->streams[i]->codecpar;
        //处理视频流
        if(codecparams->codec_type==AVMEDIA_TYPE_VIDEO&&m_videoStreamIndex==-1)
        {
            m_videoStreamIndex=i;
            const AVCodec *codec=avcodec_find_decoder(codecparams->codec_id);
            if(codec)
            {
                m_videoCodecCtx=avcodec_alloc_context3(codec);//分配视频解码上下文
                avcodec_parameters_to_context(m_videoCodecCtx,codecparams);//将编解码参数复制到编解码上下文
                if(avcodec_open2(m_videoCodecCtx,codec,nullptr)<0) return false;//打开视频编解码器
            }
        }
        //处理音频流
        else if(codecparams->codec_type==AVMEDIA_TYPE_AUDIO&&m_audioStreamIndex==-1)
        {
            m_audioStreamIndex=i;
            const AVCodec *codec=avcodec_find_decoder(codecparams->codec_id);
            if(codec)
            {
                m_audioCodecCtx=avcodec_alloc_context3(codec);//分配音频编解码上下文
                avcodec_parameters_to_context(m_audioCodecCtx,codecparams);
                if(avcodec_open2(m_audioCodecCtx,codec,nullptr)<0)//打开音频编解码器
                {
                    avcodec_free_context(&m_audioCodecCtx);
                    m_audioCodecCtx = nullptr;
                    m_audioStreamIndex = -1;
                    continue;
                }
                //配置音频重采样器（将音频转换为统一的44.1kHz立体声格式）
                AVChannelLayout in_layout = {}, out_layout = {};
                av_channel_layout_default(&in_layout, m_audioCodecCtx->ch_layout.nb_channels);
                av_channel_layout_from_mask(&out_layout, AV_CH_LAYOUT_STEREO);
                //创建音频重采样上下文
                int ret = swr_alloc_set_opts2(&m_swrCtx,
                                              &out_layout, AV_SAMPLE_FMT_S16, 44100,
                                              &in_layout, m_audioCodecCtx->sample_fmt, m_audioCodecCtx->sample_rate,
                                              0, nullptr);

                av_channel_layout_uninit(&in_layout);
                av_channel_layout_uninit(&out_layout);

                if (ret < 0 || !m_swrCtx) {
                    // 处理错误
                    avcodec_free_context(&m_audioCodecCtx);
                    m_audioCodecCtx = nullptr;
                    m_audioStreamIndex = -1;
                    continue;
                }
                //初始化音频重采样器
                if(m_swrCtx && swr_init(m_swrCtx) >= 0)
                {
                    //配置音频输出格式
                    m_audioFormat.setSampleRate(44100);//采样率44.1kHz
                    m_audioFormat.setChannelCount(2);//立体声
                    m_audioFormat.setSampleFormat(QAudioFormat::Int16);

                    QAudioDevice device = QMediaDevices::defaultAudioOutput();//获取默认音频输出设备
                    if(!device.isNull())
                    {
                        if(!device.isFormatSupported(m_audioFormat))//检查格式支持，如果不支持则使用设备首选格式
                            m_audioFormat = device.preferredFormat();

                        m_audioSink = new QAudioSink(device, m_audioFormat, this);//创建音频输出设备
                        m_audioSink->setVolume(m_volume);
                        m_audioSink->setBufferSize(44100 * 2 * 2 / 10); // 设置0.1秒的缓冲
                        m_audioDevice = m_audioSink->start();
                        m_audioEnabled = (m_audioDevice != nullptr);
                    }
                }
            }
        }
    }
    return true;
}

void videoplayer::decodeFrame()//解码帧函数：由定时器定期调用
{
    if(!m_formatCtx || !m_isPlaying) return;

    AVPacket packet;
    AVFrame *frame = av_frame_alloc();

    int packet_count = 0;//每次解码最多处理10个数据包
    while (packet_count < 10 && av_read_frame(m_formatCtx, &packet) >= 0) {
        //处理音频包
        if (m_audioEnabled && packet.stream_index == m_audioStreamIndex) {
            if (avcodec_send_packet(m_audioCodecCtx, &packet) == 0) {
                while (avcodec_receive_frame(m_audioCodecCtx, frame) == 0) {
                    decodeAudioFrame(frame);
                }
            }
        }
        //处理视频包
        else if (packet.stream_index == m_videoStreamIndex) {
            if (avcodec_send_packet(m_videoCodecCtx, &packet) == 0) {
                if (avcodec_receive_frame(m_videoCodecCtx, frame) == 0) {
                    QImage image = convertFrameToImage(frame);
                    emit videoFrame(image);
                    av_packet_unref(&packet);
                    av_frame_free(&frame);
                    return;//每次只解码一帧视频
                }
            }
        }
        av_packet_unref(&packet);
        packet_count++;
    }
    av_frame_free(&frame);
}

QImage videoplayer::convertFrameToImage(AVFrame *frame)//将AVFrame转换为QImage
{
    if(!frame||!frame->data[0]) return QImage();
    //检查是否需要重新创建像素格式转换上下文
    if (!m_swsCtx || frame->width != m_videoCodecCtx->width ||
        frame->height != m_videoCodecCtx->height ||
        frame->format != m_videoCodecCtx->pix_fmt) {

        if(m_swsCtx) sws_freeContext(m_swsCtx);
        //创建像素格式转换上下文（转换为RGB32格式）
        m_swsCtx=sws_getContext(frame->width,frame->height,(AVPixelFormat)frame->format,
                                  frame->width,frame->height,AV_PIX_FMT_RGB32,SWS_BILINEAR,nullptr,nullptr,nullptr);
    }

    QImage image(frame->width, frame->height, QImage::Format_RGB32);//创建QImage用于存储转换后的图像
    uint8_t *destDate[1] = {image.bits()};
    int destLinesize[1] = {static_cast<int>(image.bytesPerLine())};
    //执行像素格式转换
    sws_scale(m_swsCtx, frame->data, frame->linesize, 0, frame->height, destDate, destLinesize);
    return image;
}

void videoplayer::play()
{
    if (!m_formatCtx) return;
    m_isPlaying = true;
    m_pause = false;
    //处理音频输出状态
    if (m_audioEnabled && m_audioSink) {
        if (m_audioSink->state() == QAudio::SuspendedState) {
            m_audioSink->resume();//从暂停状态恢复
        } else if (m_audioSink->state() == QAudio::StoppedState) {
            m_audioDevice = m_audioSink->start();//重新启动
        }
    }

    m_timer->start(frame_rate);//启动定时器开始解码帧
    emit playstatechange(true);//发射播放状态改变信号
}

void videoplayer::pause()
{
    m_pause = !m_pause;

    if(m_pause)
    {
        m_timer->stop();//停止定时器
        if (m_audioEnabled && m_audioSink) m_audioSink->suspend();//暂停音频
    }
    else
    {
        m_timer->start(frame_rate);//重新启动定时器
        if (m_audioEnabled && m_audioSink) m_audioSink->resume();//恢复音频
    }
    emit playstatechange(!m_pause);//发射播放状态改变信号
}

void videoplayer::decodeAudioFrame(AVFrame *frame)//解码音频帧
{
    if (!frame || !m_audioSink || !m_audioDevice || !m_swrCtx) return;
    //计算重采样后的样本数
    int out_samples = av_rescale_rnd(swr_get_delay(m_swrCtx, m_audioCodecCtx->sample_rate) + frame->nb_samples,
                                     44100, m_audioCodecCtx->sample_rate, AV_ROUND_UP);

    uint8_t **resampled_data = nullptr;
    int linesize;
    //分配重采样输出缓冲区
    if (av_samples_alloc_array_and_samples(&resampled_data, &linesize, 2, out_samples, AV_SAMPLE_FMT_S16, 0) < 0) return;
    //执行音频重采样
    int samples_converted = swr_convert(m_swrCtx, resampled_data, out_samples, (const uint8_t**)frame->data, frame->nb_samples);

    if (samples_converted > 0) {
        int data_size = samples_converted * 2 * 2;//计算数据大小
        //应用音量调节
        if (m_volume != 1.0f) {
            int16_t *samples = (int16_t*)resampled_data[0];
            for (int i = 0; i < samples_converted * 2; i++) samples[i] = (int16_t)(samples[i] * m_volume);
        }
        //将音频数据写入音频设备
        if (m_audioDevice && m_audioDevice->isOpen()) m_audioDevice->write((const char*)resampled_data[0], data_size);
    }
    //释放重采样数据缓冲区
    if (resampled_data) {
        if (resampled_data[0]) av_freep(&resampled_data[0]);
        av_freep(&resampled_data);
    }
}

void videoplayer::setVolume(float volume)
{
    m_volume = qBound(0.0f, volume, 1.0f);
    if (m_audioSink) m_audioSink->setVolume(m_volume);
}
