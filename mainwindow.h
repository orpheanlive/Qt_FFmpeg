#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include"videoplayer.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();


private slots:
    void on_openfile_clicked();
    void on_play_clicked();
    void onStateChanged(bool playing);
    void on_audio_actionTriggered(int action);

private:
    Ui::MainWindow *ui;

    void onVideoFrame(const QImage &frame);
    videoplayer *m_player;
};
#endif // MAINWINDOW_H
