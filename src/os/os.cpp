#include "OS.h"
#include <QDebug>
#include <PowrProf.h>

#pragma comment(lib, "PowrProf.lib")

QString OS::s_lastError;

bool OS::enableShutdownPrivilege()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // Get a token for this process
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &hToken)) {
        s_lastError = QString("OpenProcessToken failed: %1").arg(GetLastError());
        return false;
    }

    // Get the LUID for the shutdown privilege
    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        s_lastError = QString("LookupPrivilegeValue failed: %1").arg(GetLastError());
        CloseHandle(hToken);
        return false;
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Enable the shutdown privilege
    if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, NULL)) {
        s_lastError = QString("AdjustTokenPrivileges failed: %1").arg(GetLastError());
        CloseHandle(hToken);
        return false;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        s_lastError = "The token does not have the shutdown privilege";
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}

bool OS::shutdown(int timeoutSeconds, bool force, const QString& reason)
{
    if (!enableShutdownPrivilege()) {
        qWarning() << "OS::shutdown - Failed to enable shutdown privilege:" << s_lastError;
        return false;
    }

    DWORD flags = SHUTDOWN_POWEROFF;
    if (force) {
        flags |= SHUTDOWN_FORCE_OTHERS | SHUTDOWN_FORCE_SELF;
    }

    // Use InitiateShutdown for more control (available on Vista+)
    DWORD result = InitiateShutdownW(
        NULL,                                           // Local computer
        reason.isEmpty() ? NULL : (LPWSTR)reason.utf16(), // Reason message
        timeoutSeconds,                                 // Timeout
        flags,                                          // Flags
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED
        );

    if (result != ERROR_SUCCESS) {
        s_lastError = QString("InitiateShutdown failed: %1").arg(result);
        qWarning() << "OS::shutdown -" << s_lastError;
        return false;
    }

    qDebug() << "OS::shutdown - Shutdown initiated, timeout:" << timeoutSeconds << "seconds";
    return true;
}

bool OS::restart(int timeoutSeconds, bool force, const QString& reason)
{
    if (!enableShutdownPrivilege()) {
        qWarning() << "OS::restart - Failed to enable shutdown privilege:" << s_lastError;
        return false;
    }

    DWORD flags = SHUTDOWN_RESTART;
    if (force) {
        flags |= SHUTDOWN_FORCE_OTHERS | SHUTDOWN_FORCE_SELF;
    }

    DWORD result = InitiateShutdownW(
        NULL,
        reason.isEmpty() ? NULL : (LPWSTR)reason.utf16(),
        timeoutSeconds,
        flags,
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED
        );

    if (result != ERROR_SUCCESS) {
        s_lastError = QString("InitiateShutdown failed: %1").arg(result);
        qWarning() << "OS::restart -" << s_lastError;
        return false;
    }

    qDebug() << "OS::restart - Restart initiated, timeout:" << timeoutSeconds << "seconds";
    return true;
}

bool OS::cancelShutdown()
{
    if (!enableShutdownPrivilege()) {
        qWarning() << "OS::cancelShutdown - Failed to enable shutdown privilege:" << s_lastError;
        return false;
    }

    if (!AbortSystemShutdownW(NULL)) {
        DWORD error = GetLastError();
        if (error == ERROR_NO_SHUTDOWN_IN_PROGRESS) {
            // Not really an error - just no shutdown to cancel
            qDebug() << "OS::cancelShutdown - No shutdown in progress";
            return true;
        }
        s_lastError = QString("AbortSystemShutdown failed: %1").arg(error);
        qWarning() << "OS::cancelShutdown -" << s_lastError;
        return false;
    }

    qDebug() << "OS::cancelShutdown - Shutdown cancelled";
    return true;
}

bool OS::sleep(bool disableWakeEvents)
{
    // SetSuspendState(hibernate, forceCritical, disableWakeEvents)
    // hibernate = FALSE means sleep (S3)
    // forceCritical = FALSE allows system to reject if busy
    if (!SetSuspendState(FALSE, FALSE, disableWakeEvents ? TRUE : FALSE)) {
        s_lastError = QString("SetSuspendState(sleep) failed: %1").arg(GetLastError());
        qWarning() << "OS::sleep -" << s_lastError;
        return false;
    }

    qDebug() << "OS::sleep - Sleep initiated";
    return true;
}

bool OS::hibernate(bool disableWakeEvents)
{
    // Check if hibernate is available first
    if (!isHibernateAvailable()) {
        s_lastError = "Hibernate is not available on this system";
        qWarning() << "OS::hibernate -" << s_lastError;
        return false;
    }

    // SetSuspendState(hibernate, forceCritical, disableWakeEvents)
    // hibernate = TRUE means hibernate (S4)
    if (!SetSuspendState(TRUE, FALSE, disableWakeEvents ? TRUE : FALSE)) {
        s_lastError = QString("SetSuspendState(hibernate) failed: %1").arg(GetLastError());
        qWarning() << "OS::hibernate -" << s_lastError;
        return false;
    }

    qDebug() << "OS::hibernate - Hibernate initiated";
    return true;
}

bool OS::lockWorkstation()
{
    if (!LockWorkStation()) {
        s_lastError = QString("LockWorkStation failed: %1").arg(GetLastError());
        qWarning() << "OS::lockWorkstation -" << s_lastError;
        return false;
    }

    qDebug() << "OS::lockWorkstation - Workstation locked";
    return true;
}

bool OS::logOff(bool force)
{
    UINT flags = EWX_LOGOFF;
    if (force) {
        flags |= EWX_FORCE;
    }

    if (!ExitWindowsEx(flags, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER)) {
        s_lastError = QString("ExitWindowsEx failed: %1").arg(GetLastError());
        qWarning() << "OS::logOff -" << s_lastError;
        return false;
    }

    qDebug() << "OS::logOff - Log off initiated";
    return true;
}

bool OS::isHibernateAvailable()
{
    SYSTEM_POWER_CAPABILITIES spc;

    if (!GetPwrCapabilities(&spc)) {
        return false;
    }

    // Check if hibernate is supported and enabled
    return spc.HiberFilePresent && spc.SystemS4;
}

QString OS::lastError()
{
    return s_lastError;
}
