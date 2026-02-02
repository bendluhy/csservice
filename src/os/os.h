#ifndef OS_H
#define OS_H

#include <QString>
#include <Windows.h>

/**
 * @brief OS - System power management utilities
 *
 * Provides functions for sleep, hibernate, shutdown, and restart.
 * Requires SE_SHUTDOWN_NAME privilege for shutdown/restart operations.
 *
 * Usage:
 *   OS::shutdown();           // Immediate shutdown
 *   OS::shutdown(30);         // Shutdown in 30 seconds
 *   OS::restart();            // Immediate restart
 *   OS::sleep();              // Enter sleep (S3)
 *   OS::hibernate();          // Enter hibernate (S4)
 *   OS::cancelShutdown();     // Cancel pending shutdown/restart
 */
class OS
{
public:
    /**
     * @brief Shut down the computer
     * @param timeoutSeconds Seconds before shutdown (0 = immediate)
     * @param force Force close applications without saving
     * @param reason Optional reason string for event log
     * @return true if successful
     */
    static bool shutdown(int timeoutSeconds = 0, bool force = false,
                         const QString& reason = QString());

    /**
     * @brief Restart the computer
     * @param timeoutSeconds Seconds before restart (0 = immediate)
     * @param force Force close applications without saving
     * @param reason Optional reason string for event log
     * @return true if successful
     */
    static bool restart(int timeoutSeconds = 0, bool force = false,
                        const QString& reason = QString());

    /**
     * @brief Cancel a pending shutdown or restart
     * @return true if successful
     */
    static bool cancelShutdown();

    /**
     * @brief Put the computer to sleep (S3 suspend)
     * @param disableWakeEvents If true, disable wake events
     * @return true if successful
     */
    static bool sleep(bool disableWakeEvents = false);

    /**
     * @brief Hibernate the computer (S4 suspend to disk)
     * @param disableWakeEvents If true, disable wake events
     * @return true if successful
     */
    static bool hibernate(bool disableWakeEvents = false);

    /**
     * @brief Lock the workstation
     * @return true if successful
     */
    static bool lockWorkstation();

    /**
     * @brief Log off the current user
     * @param force Force log off without prompting
     * @return true if successful
     */
    static bool logOff(bool force = false);

    /**
     * @brief Check if hibernate is available on this system
     * @return true if hibernate is supported and enabled
     */
    static bool isHibernateAvailable();

    /**
     * @brief Get the last error message
     * @return Error message from last failed operation
     */
    static QString lastError();

private:
    /**
     * @brief Enable shutdown privilege for this process
     * @return true if successful
     */
    static bool enableShutdownPrivilege();

    static QString s_lastError;
};

#endif // OS_H
