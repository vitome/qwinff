#include "mediaconverter.h"
#include "ffmpeginterface.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>

#define TIMEOUT 1000

MediaConverter::MediaConverter(QObject *parent) :
    QObject(parent)
{
    m_pConv = new FFmpegInterface(this);

    connect(&m_proc, SIGNAL(readyRead())
            , this, SLOT(readProcessOutput()));
    connect(&m_proc, SIGNAL(finished(int,QProcess::ExitStatus))
            , this, SLOT(convertProgressFinished(int,QProcess::ExitStatus)));
    connect(m_pConv, SIGNAL(progressRefreshed(double))
            , this, SLOT(convertProgressRefreshed(double)));

}

MediaConverter::~MediaConverter()
{
    this->stop();
}

// public methods

bool MediaConverter::start(ConversionParameters param)
{
    if (m_proc.state() == QProcess::NotRunning) {
        m_stopped = false;

        emit progressRefreshed(0);

        // Save output filename.
        m_outputFileName = param.destination;

        // Generate temporary file name.
        m_tmpFileName = m_outputFileName + "." + QString::number(qrand()) +
                ".temp." + QFileInfo(m_outputFileName).suffix();

        // Output to temporary file.
        param.destination = m_tmpFileName;

        m_pConv->reset();

        QStringList list;
        m_pConv->setReadChannel(m_proc);
        m_pConv->fillParameterList(param, list);

        qDebug() << list;

        m_proc.start(m_pConv->executableName(), list);
        return m_proc.waitForStarted(TIMEOUT);
    }
    return false;
}

void MediaConverter::stop()
{
    if (m_proc.state() == QProcess::Running) {
        m_stopped = true;
        m_proc.kill();
        m_proc.waitForFinished();
    }
}

double MediaConverter::progress()
{
    return m_pConv->progress();
}

// private slots

void MediaConverter::readProcessOutput()
{
       m_pConv->parseProcessOutput(QString(m_proc.readAll()));
}

void MediaConverter::convertProgressFinished(int exitcode, QProcess::ExitStatus)
{
    QFile output_file(m_outputFileName);
    QFile tmp_file(m_tmpFileName);

    if (exitcode == 0 && tmp_file.exists() && !m_stopped) { // succeed
        output_file.remove();              // Remove original file if it exists.
        tmp_file.rename(m_outputFileName); // Rename tmpfile to outputfile.
    } else { // failed
        tmp_file.remove(); // Remove tmpfile if conversion failed.
    }

    if (exitcode == 0 && !m_stopped)
        emit progressRefreshed(100); // 100% finished
    else
        emit progressRefreshed(0);

    emit finished(exitcode);
}

void MediaConverter::convertProgressRefreshed(double percentage)
{
    emit progressRefreshed((int)percentage);
}

// private methods
