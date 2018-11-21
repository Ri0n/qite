/*
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qite.h"
#include "iteaudio.h"

#include <algorithm>
#include <limits>
#include <QStandardPaths>
#include <QDir>
#include <QDirIterator>
#include <QRandomGenerator>
#include <QAction>
#include <QIcon>
#include <QAudioRecorder>
#include <QStyle>
#include <QDateTime>
#include <QAudioProbe>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    recordAction = new QAction(QIcon(":/icon/recorder-microphone.png"), "Record", this);
    ui->mainToolBar->addAction(recordAction);
    connect(recordAction, &QAction::triggered, this, &MainWindow::recordMic);

    auto itc = new InteractiveText(ui->textEdit);
    auto atc = new ITEAudioController(itc);

    auto musicDir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);

    auto nameFilters = QStringList() << "*.aac" << "*.flac" << "*.mp3" << "*.ogg" << "*.webm";
    QDirIterator it(musicDir, nameFilters, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    QStringList files;
    int count = 2000;
    while (it.hasNext() && count--)
        files << it.next();

    std::srand(unsigned(std::time(nullptr)));
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

template <typename T> struct SoloFrameDefault { enum { Default = 0 }; };

template <typename T> struct SoloFrame {

    SoloFrame()
        : data(T(SoloFrameDefault<T>::Default))
    {
    }

    SoloFrame(T sample)
        : data(sample)
    {
    }

    SoloFrame& operator=(const SoloFrame &other)
    {
        data = other.data;
        return *this;
    }

    T data;

    T average() const {return data;}
    void clear() {data = T(SoloFrameDefault<T>::Default);}
};

template<class T> struct PeakValue { static const T value = std::numeric_limits<T>::max(); };

template<> struct PeakValue <float> { static constexpr float value = float(1.00003); };

template<class T>
void handle(const QAudioBuffer &buffer) {
    const T *data = buffer.constData<T>();
    auto peakvalue = qreal(PeakValue<decltype(data[0].average())>::value);

    int count10ms = buffer.format().framesForDuration(10000);
    int next = count10ms;
    //qDebug() << "frames: " << buffer.frameCount() << count10ms;
    qreal sum = 0.0;
    for (int i=0; i<buffer.frameCount(); i++) {
        if (i + 1 == next) {
            qDebug() << int((sum / qreal(count10ms)) * 255);
            sum = 0.0;
            next += count10ms;
        } else {
            sum += qreal(qAbs(data[i].average())) / peakvalue;
        }
    }
}

void MainWindow::recordMic()
{
    if (!recorder) {
        recorder = new QAudioRecorder(this);
        probe = new QAudioProbe(this);
        probe->setSource(recorder);

        QAudioEncoderSettings audioSettings;
        audioSettings.setCodec("audio/x-vorbis");
        audioSettings.setQuality(QMultimedia::HighQuality);

        recorder->setEncodingSettings(audioSettings);

        connect(recorder, &QAudioRecorder::stateChanged, this, [this](){
            if (recorder->state() == QAudioRecorder::StoppedState) {
                recordAction->setIcon(QIcon(":/icon/recorder-microphone.png"));
            }
            if (recorder->state() == QAudioRecorder::RecordingState) {
                recordAction->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
            }
        });

        connect(probe, &QAudioProbe::audioBufferProbed, this, [this](const QAudioBuffer &buffer){
            auto format = buffer.format();
            if (format.channelCount() > 2) {
                qWarning("unsupported amount of channels: %d", format.channelCount());
                return;
            }

            if (format.sampleType() == QAudioFormat::SignedInt) {
                switch (format.sampleSize()) {
                case 8:
                    if(format.channelCount() == 2)
                        handle<QAudioBuffer::S8S>(buffer);
                    else
                        handle<SoloFrame<signed char>>(buffer);
                    break;
                case 16:
                    if(format.channelCount() == 2)
                        handle<QAudioBuffer::S16S>(buffer);
                    else
                        handle<SoloFrame<signed short>>(buffer);
                    break;
                }
            } else if (format.sampleType() == QAudioFormat::UnSignedInt) {
                switch (format.sampleSize()) {
                case 8:
                    if(format.channelCount() == 2)
                        handle<QAudioBuffer::S8U>(buffer);
                    else
                        handle<SoloFrame<unsigned char>>(buffer);
                    break;
                case 16:
                    if(format.channelCount() == 2)
                        handle<QAudioBuffer::S16U>(buffer);
                    else
                        handle<SoloFrame<unsigned short>>(buffer);
                    break;
                }
            } else if(format.sampleType() == QAudioFormat::Float) {
                if(format.channelCount() == 2)
                    handle<QAudioBuffer::S32F>(buffer);
                else
                    handle<SoloFrame<float>>(buffer);
            } else {
                qWarning("unsupported audio sample type: %d", int(format.sampleType()));
            }
        });
    }

    if (recorder->state() == QAudioRecorder::StoppedState) {
        recorder->setOutputLocation(QUrl::fromLocalFile(QString("test-%1.ogg").arg(QDateTime::currentSecsSinceEpoch())));
        auto reserved = QLatin1String("AMPLDIAGSTART[000,") + QString(",000").repeated(19) + QLatin1String("]AMPLDIAGEND");
        recorder->setMetaData("ampldiag", reserved);
        recorder->record();
    } else {
        recorder->stop();
    }
}
