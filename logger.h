#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QMutex>

class Logger : public QObject
{
    Q_OBJECT

public:
    enum LogLevel {
        Info,
        Warning,
        Error,
        Debug
    };
    Q_ENUM(LogLevel)

    explicit Logger(const QString &logDir, QObject *parent = nullptr);
    ~Logger();

    void log(const QString &message, LogLevel level = Info);

    // Optional: Check if logger is functional
    bool isValid() const { return m_isValid; }
    QString currentLogFile() const;

private:
    void rotateLogs();
    bool openNewLogFile();
    void closeCurrentLogFile();

    QString m_logDir;
    QFile m_logFile;
    QTextStream m_logStream;
    mutable QMutex m_mutex;
    bool m_isValid;

    const qint64 MAX_LOG_FILE_SIZE = 5 * 1024 * 1024; // 5 MB
    const int MAX_LOG_FILES = 5;
};

#endif // LOGGER_H
