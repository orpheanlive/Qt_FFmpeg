#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QObject>
#include <QImage>
#include<QTimer>

#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioFormat>
extern "C" {
#include <libavformat/avformat.h>//媒体容器格式处理
#include <libavcodec/avcodec.h>//音视频编解码
#include <libswscale/swscale.h>//图像缩放和格式转换
#include <libswresample/swresample.h>  // 添加音频重采样
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>  // 声道布局相关
#include <libavutil/error.h>  // 错误处理

}
class videoplayer : public QObject
{
    Q_OBJECT
public:
    explicit videoplayer(QObject *parent = nullptr);
    ~videoplayer();

    bool openFile(const QString &filename);//打开媒体文件
    void play();//播放，启动解码定时器，开始播放媒体文件
    void pause();//暂停
    bool isPlaying() const { return m_isPlaying; }//检查是否播放
    void setVolume(float volume); // 设置音量


private slots:
    void decodeFrame();//解码音视频帧
signals:
    void playstatechange(bool playing);//播放状态改变
    void videoFrame(const QImage &frame);//将解码出的视频帧传递到界面显示

private:
    //================================= 处理视频模块==========================================================
    QImage convertFrameToImage(AVFrame *frame);// 将FFmpeg帧转换为Qt图像,使用libswscale进行YUV到RGB格式转换
    AVFormatContext *m_formatCtx = nullptr;//媒体格式上下文，管理文件容器
    AVCodecContext *m_videoCodecCtx = nullptr;//视频编解码器上下文，管理视频解码；存储视频流的解码参数
    SwsContext *m_swsCtx = nullptr;//图像缩放转换上下文，YUV转RGB

    int m_videoStreamIndex = -1;  //视频流在文件中的索引，-1表示未找到

    bool m_isPlaying = false;//播放状态标志
    bool m_pause=false;
    int frame_rate=33;
    QTimer *m_timer;  // 解码定时器，控制解码帧率;这个是控制策略，播放、暂停、停止、重新播放都是通过这个控制的
    //原理：每隔固定时间触发timeout()信号，调用decodeFrame()而decodeFrame()函数会解码后生成一帧（就是一张图片）



    //================================= 处理声音模块==========================================================
    // 音频相关成员变量
    AVCodecContext *m_audioCodecCtx = nullptr;
    int m_audioStreamIndex = -1;
    SwrContext *m_swrCtx = nullptr;
    float m_volume = 1.0f;
    bool m_audioEnabled = false;
    // Qt6 音频输出
    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_audioDevice = nullptr;
    QAudioFormat m_audioFormat;
    // 音频同步
    int64_t m_audioPts = 0;
    AVRational m_audioTimeBase;
    // 音频缓冲区
    QByteArray m_audioBuffer;

    // 音频相关函数
    bool initAudio(); // 初始化音频
    void decodeAudioFrame(AVFrame *frame);





};

#endif // VIDEOPLAYER_H
