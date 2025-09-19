#include <windows.h>
#include <userenv.h>
#include <Wtsapi32.h>
#include "logger.h"  // Make sure this includes the Logger class
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "Userenv.lib")

bool LaunchProcessInUserSession(Logger* pLogger, const QString& appPath)
{
    DWORD sessionId = WTSGetActiveConsoleSessionId();

    HANDLE hUserToken = nullptr;
    if (!WTSQueryUserToken(sessionId, &hUserToken)) {
        pLogger->log("WTSQueryUserToken failed: " + QString::number(GetLastError()), Logger::Error);
        return false;
    }

    HANDLE hUserTokenDup = nullptr;
    if (!DuplicateTokenEx(hUserToken, MAXIMUM_ALLOWED, NULL,
                          SecurityIdentification, TokenPrimary, &hUserTokenDup)) {
        pLogger->log("DuplicateTokenEx failed: " + QString::number(GetLastError()), Logger::Error);
        CloseHandle(hUserToken);
        return false;
    }

    // Get environment block for the user
    LPVOID lpEnvironment = nullptr;
    if (!CreateEnvironmentBlock(&lpEnvironment, hUserTokenDup, FALSE)) {
        pLogger->log("CreateEnvironmentBlock failed: " + QString::number(GetLastError()), Logger::Error);
        CloseHandle(hUserTokenDup);
        CloseHandle(hUserToken);
        return false;
    }

    // Convert QString to mutable LPWSTR for CreateProcessAsUserW
    std::wstring cmdLineW = appPath.toStdWString();
    LPWSTR lpCmdLine = &cmdLineW[0]; // mutable for CreateProcess

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");

    PROCESS_INFORMATION pi = {};

    BOOL result = CreateProcessAsUserW(
        hUserTokenDup,
        nullptr,            // Application name
        lpCmdLine,          // Command line
        nullptr, nullptr,   // Process and thread security attributes
        FALSE,
        CREATE_UNICODE_ENVIRONMENT,
        lpEnvironment,
        nullptr,            // Current directory
        &si,
        &pi
        );

    if (!result) {
        pLogger->log("CreateProcessAsUserW failed: " + QString::number(GetLastError()), Logger::Error);
    } else {
        pLogger->log("Process launched successfully: " + appPath, Logger::Info);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    DestroyEnvironmentBlock(lpEnvironment);
    CloseHandle(hUserTokenDup);
    CloseHandle(hUserToken);

    return result == TRUE;
}
