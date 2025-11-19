#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include<QFileDialog>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->play->setEnabled(false);
    m_player=new videoplayer(this);
    connect(m_player,&videoplayer::videoFrame, this, &MainWindow::onVideoFrame);
    connect(m_player,&videoplayer::playstatechange,this,&MainWindow::onStateChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_openfile_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
                                                    "打开视频文件", "","视频文件 (*.mp4 *.avi *.mkv *.mov *.flv *.wmv);;所有文件 (*.*)");
    if(!filename.isEmpty())
    {
        // 打开视频文件
        if(m_player->openFile(filename)) {
            // 视频文件打开成功，改变播放按钮状态
            ui->play->setEnabled(true);
        } else {
            // 文件打开失败处理
            ui->play->setEnabled(false);
            qDebug() << "打开文件失败:" << filename;
        }
    }
}
void MainWindow::onVideoFrame(const QImage &frame)//显示视频帧
{
    QPixmap pixmap = QPixmap::fromImage(frame);
    ui->video->setPixmap(pixmap.scaled(ui->video->size(), Qt::KeepAspectRatio));
}

void MainWindow::on_play_clicked()
{
    if(m_player->isPlaying())
    {
        m_player->pause();
    }
    else
    {
        m_player->play();
    }
}
void MainWindow::onStateChanged(bool playing)
{
    ui->play->setText(playing ? "暂停" : "播放");
}


void MainWindow::on_audio_actionTriggered(int action)
{
    float volume = action/ 100.0f; // 转换为0.0-1.0范围
    m_player->setVolume(volume);
}

