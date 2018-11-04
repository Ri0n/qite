#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qite.h"
#include "iteaudio.h"

#include <algorithm>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QRandomGenerator>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    auto itc = new InteractiveText(ui->textEdit);
    auto atc = new ITEAudioController(itc);

    auto musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);

    auto nameFilters = QStringList() << "*.aac" << "*.flac" << "*.mp3" << "*.ogg" << "*.webm";
    QDirIterator it(musicDir, nameFilters, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    QStringList files;
    int count = 1000;
    while (it.hasNext() && count--)
        files << it.next();

    std::random_shuffle(files.begin(), files.end());

    while (files.size()) {
        QString fileToPlay = files.takeLast();
        QFileInfo fi(fileToPlay);
        auto url = QUrl::fromLocalFile(fileToPlay);
        ui->textEdit->append(fi.fileName() + "<br>");
        atc->insert(url);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
