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

#ifndef QITEAUDIORECORDER_H
#define QITEAUDIORECORDER_H

#include <QObject>

class QAudioRecorder;
class QAudioProbe;
class AudioRecorder : public QObject
{
    Q_OBJECT
public:
    static const qint64 HistogramQuantumSize = 10000; // 10ms. 100 values per second
    static const int HistogramMemSize = int(1e6) / HistogramQuantumSize * 20; // for 20 secs. ~ 2Kb

    struct Quantum {
        qint64 timeLeft = HistogramQuantumSize; // to generate next value for aplitude histogram
        qreal sum = 0.0;
        int count = 0;
    };

    explicit AudioRecorder(QObject *parent = nullptr);

    void record(const QString &fileName);
    void stop();

    inline auto recorder() const { return _recorder; }
    inline auto maxVolume() const { return _maxVolume; } // peak value of vlume over all the recording.

signals:
    void stateChanged();
public slots:

private:
    QAudioRecorder *_recorder = nullptr;
    QAudioProbe *probe;
    Quantum quantum;
    QByteArray histogram;
    quint8 _maxVolume;
};

#endif // QITEAUDIORECORDER_H
