#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "interactivetextoverlord.h"
#include "iteaudio.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto itc = new InteractiveTextController(ui->textEdit);
    auto atc = new ITEAudioController(itc);
    atc->add(QUrl("file:///home/rion/Музыка/Kl.Lo.Werk.7/209. Ibiphonic Feat. Dennis Le Gree - Sunny.mp3"));
}

MainWindow::~MainWindow()
{
    delete ui;
}
