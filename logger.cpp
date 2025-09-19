#include "Logger.h"
#include <QDebug>
#include <QCoreApplication>

Logger::Logger(const QString &logDir, QObject *parent)
    : QObject(parent)
    , m_logDir(logDir)
    , m_isValid(false)
{
    // Ensure log directory exists
    QDir dir;
    if (!dir.exists(m_logDir)) {
        if (!dir.mkpath(m_logDir)) {
#ifdef QT_DEBUG
            qWarning() << "Failed to create log directory:" << m_logDir;
#endif
            return;
        }
    }

    // Open initial log file
    if (!openNewLogFile()) {
#ifdef QT_DEBUG
        qWarning() << "Failed to open initial log file";
#endif
        return;
    }

    m_isValid = true;

    // Cleanup older logs after successful initialization
    rotateLogs();

    // Log startup
    log("Logger initialized", Info);
}

Logger::~Logger()
{
    QMutexLocker locker(&m_mutex);

    if (m_isValid) {
        m_logStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
                    << " [INFO] Logger shutting down\n";
        m_logStream.flush();
    }

    closeCurrentLogFile();
}

void Logger::log(const QString &message, LogLevel level)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isValid || !m_logFile.isOpen()) {
        // Fallback to debug output if logging fails
#ifdef QT_DEBUG
        qDebug() << "Logger not available:" << message;
#endif
        return;
    }

    // Check file size and rotate if needed
    if (m_logFile.size() > MAX_LOG_FILE_SIZE) {
        closeCurrentLogFile();

        if (!openNewLogFile()) {
#ifdef QT_DEBUG
            qWarning() << "Failed to rotate log file";
#endif
            m_isValid = false;
            return;
        }

        rotateLogs();
    }

    // Determine level text
    QString levelText;
    switch (level) {
    case Info:    levelText = "[INFO]";  break;
    case Warning: levelText = "[WARN]";  break;
    case Error:   levelText = "[ERROR]"; break;
    case Debug:   levelText = "[DEBUG]"; break;
    default:      levelText = "[INFO]";  break;
    }

    // Create log line
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logLine = QString("%1 %2 %3").arg(timestamp, levelText, message);

    // Write to file
    m_logStream << logLine << "\n";
    m_logStream.flush();

// Also output to debug console in debug builds
#ifdef QT_DEBUG
    // Only show warnings and errors in release to reduce noise
    if (level >= Warning || level == Debug) {
        qDebug().noquote() << logLine;
    }
#endif
}

void Logger::rotateLogs()
{
    QDir dir(m_logDir);

    // Get all log files sorted by time (oldest first)
    QStringList logFiles = dir.entryList(
        QStringList() << "log_*.txt",
        QDir::Files,
        QDir::Time | QDir::Reversed);

    // Remove oldest files if we exceed the limit
    while (logFiles.size() > MAX_LOG_FILES) {
        QString fileToRemove = logFiles.takeFirst();
        QString fullPath = dir.filePath(fileToRemove);

        if (dir.remove(fileToRemove)) {
#ifdef QT_DEBUG
            qDebug() << "Removed old log file:" << fileToRemove;
#endif
        } else {
#ifdef QT_DEBUG
            qWarning() << "Failed to remove old log file:" << fileToRemove;
#endif
        }
    }
}

bool Logger::openNewLogFile()
{
    // Close any existing file first
    closeCurrentLogFile();

    // Generate new log file name with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString logFileName = QString("%1/log_%2.txt").arg(m_logDir, timestamp);

    m_logFile.setFileName(logFileName);

    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
#ifdef QT_DEBUG
        qWarning() << "Failed to open log file:" << logFileName
                   << "Error:" << m_logFile.errorString();
#endif
        return false;
    }

    // Set up the text stream
    m_logStream.setDevice(&m_logFile);

#ifdef QT_DEBUG
    qDebug() << "Opened new log file:" << logFileName;
#endif

    return true;
}

void Logger::closeCurrentLogFile()
{
    if (m_logStream.device()) {
        m_logStream.flush();
        m_logStream.setDevice(nullptr);
    }

    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

QString Logger::currentLogFile() const
{
    QMutexLocker locker(&m_mutex);
    return m_logFile.fileName();
}
