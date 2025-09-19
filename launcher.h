#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <QString>
#include "logger.h"

bool LaunchProcessInUserSession(Logger* pLogger, const QString& appPath);

#endif // LAUNCHER_H
