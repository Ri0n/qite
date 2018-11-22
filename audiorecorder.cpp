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

#include "audiorecorder.h"

#include <cmath>

#include <QAudioProbe>
#include <QAudioRecorder>
#include <QDateTime>
#include <QUrl>


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
void handle(const QAudioBuffer &buffer, AudioRecorder::Quantum &quantum, QByteArray &collector) {
    const T *data = buffer.constData<T>();
    auto peakvalue = qreal(PeakValue<decltype(data[0].average())>::value);

    auto format = buffer.format();
    int countLeft = format.framesForDuration(quantum.timeLeft);
    Q_ASSERT(countLeft > 0);
    for (int i=0; i<buffer.frameCount(); i++) {
        quantum.sum += qreal(qAbs(data[i].average())) / peakvalue;
        quantum.count++;
        countLeft--;
        if (!countLeft) {
            //qDebug() << int((quantum.sum / qreal(quantum.count)) * 255.0);
            collector.append(char((quantum.sum / qreal(quantum.count)) * 255.0));
            if (collector.size() == collector.capacity()) {
                collector.reserve(collector.capacity() + AudioRecorder::HistogramMemSize);
            }
            quantum = AudioRecorder::Quantum();
            countLeft = format.framesForDuration(quantum.timeLeft);
        }
    }
    if (countLeft) {
        quantum.timeLeft = format.durationForFrames(countLeft);
    }
}


AudioRecorder::AudioRecorder(QObject *parent) : QObject(parent)
{
    _recorder = new QAudioRecorder(this);
    probe = new QAudioProbe(this);
    probe->setSource(_recorder);

    QAudioEncoderSettings audioSettings;
    audioSettings.setCodec("audio/x-vorbis");
    audioSettings.setQuality(QMultimedia::HighQuality);

    _recorder->setEncodingSettings(audioSettings);

    connect(_recorder, &QAudioRecorder::stateChanged, this, [this](){
        if (_recorder->state() == QAudioRecorder::StoppedState) {

            // compress histogram..
            // divide duration into 100 columns and divide into quantum size in ms.
            int samplesPerColumn = int(std::ceil(_recorder->duration() / 100.0 / (HistogramQuantumSize / 1000.0)));
            Q_ASSERT(samplesPerColumn);

            QStringList columns;

            qDebug() << "duration" << _recorder->duration() << "ms." <<
                        "samples per column" << samplesPerColumn <<
                        "columns" << _recorder->duration() / (samplesPerColumn * HistogramQuantumSize / 1000.0);
            int sum = 0;
            int count = 0;
            for (int i = 0; i < histogram.size(); i++) {
                sum += quint8(histogram[i]);
                count++;
                if (count == samplesPerColumn) {
                    columns.append(QString::number(int(sum / double(count))));
                    sum = 0;
                    count = 0;
                }
            }
            if (count) {
                columns.append(QString::number(int(sum / double(count))));
            }
            qDebug() << columns;
        }
        emit stateChanged();
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
                    handle<QAudioBuffer::S8S>(buffer, quantum, histogram);
                else
                    handle<SoloFrame<signed char>>(buffer, quantum, histogram);
                break;
            case 16:
                if(format.channelCount() == 2)
                    handle<QAudioBuffer::S16S>(buffer, quantum, histogram);
                else
                    handle<SoloFrame<signed short>>(buffer, quantum, histogram);
                break;
            }
        } else if (format.sampleType() == QAudioFormat::UnSignedInt) {
            switch (format.sampleSize()) {
            case 8:
                if(format.channelCount() == 2)
                    handle<QAudioBuffer::S8U>(buffer, quantum, histogram);
                else
                    handle<SoloFrame<unsigned char>>(buffer, quantum, histogram);
                break;
            case 16:
                if(format.channelCount() == 2)
                    handle<QAudioBuffer::S16U>(buffer, quantum, histogram);
                else
                    handle<SoloFrame<unsigned short>>(buffer, quantum, histogram);
                break;
            }
        } else if(format.sampleType() == QAudioFormat::Float) {
            if(format.channelCount() == 2)
                handle<QAudioBuffer::S32F>(buffer, quantum, histogram);
            else
                handle<SoloFrame<float>>(buffer, quantum, histogram);
        } else {
            qWarning("unsupported audio sample type: %d", int(format.sampleType()));
        }
    });
}

void AudioRecorder::record(const QString &fileName)
{
    quantum = Quantum();
    histogram.clear();
    histogram.reserve(HistogramMemSize);
    _recorder->setOutputLocation(QUrl::fromLocalFile(fileName));
    auto reserved = QLatin1String("AMPLDIAGSTART[000,") + QString(",000").repeated(200) + QLatin1String("]AMPLDIAGEND");
    _recorder->setMetaData("ampldiag", reserved);
    _recorder->record();
}

void AudioRecorder::stop()
{
    _recorder->stop();
}
