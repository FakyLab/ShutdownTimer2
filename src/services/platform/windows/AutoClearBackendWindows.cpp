#include "AutoClearBackendWindows.h"

#include <windows.h>
#include <comdef.h>
#include <taskschd.h>
#include <QDir>
#include <QCoreApplication>

// -- Constructor / Destructor --
// COM is NOT initialized here. It is deferred to the first actual use via
// ensureCOM(). This means app startup has zero COM overhead - the cost is
// paid only when the user first interacts with the Startup Message tab.

AutoClearBackendWindows::AutoClearBackendWindows(QObject* parent)
    : IAutoClearBackend(parent)
    // m_comInitialized = false - COM intentionally not started yet
{}

AutoClearBackendWindows::~AutoClearBackendWindows()
{
    uninitCOM();
}

// -- ensureCOM --
// Initializes COM the first time any operation is needed.
// All subsequent calls are a cheap boolean check and return immediately.
// Thread-safety note: this is called from the main Qt thread only -
// all three public methods are triggered by UI interactions.

bool AutoClearBackendWindows::ensureCOM()
{
    if (m_comInitialized) return true;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // RPC_E_CHANGED_MODE means COM was already initialized by someone else
    // with a different apartment model - we can still use it safely.
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        m_comInitialized = true;
        return true;
    }

    m_lastError = QString("CoInitializeEx failed: 0x%1")
                  .arg(static_cast<DWORD>(hr), 8, 16, QChar('0'));
    return false;
}

void AutoClearBackendWindows::uninitCOM()
{
    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
    }
}

// -- Internal helper: connect to Task Scheduler service --

static HRESULT connectTaskService(ITaskService** ppService)
{
    HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_ITaskService,
                                  reinterpret_cast<void**>(ppService));
    if (FAILED(hr)) return hr;

    hr = (*ppService)->Connect(_variant_t(), _variant_t(),
                               _variant_t(), _variant_t());
    if (FAILED(hr)) {
        (*ppService)->Release();
        *ppService = nullptr;
    }
    return hr;
}

// -- schedule --
// Called when user saves a message with "auto-clear after next login" checked.
// First use of the message tab - this is where COM initializes for the first
// time, not at app startup.

bool AutoClearBackendWindows::schedule()
{
    if (!ensureCOM()) return false;

    QString exePath = QDir::toNativeSeparators(
        QCoreApplication::applicationFilePath());

    // Security: refuse to create a privileged task if the exe lives outside
    // Program Files. Prevents user-writable path + high-privilege task
    // from being used as a privilege escalation vector.
    {
        wchar_t progFiles[MAX_PATH]    = {};
        wchar_t progFilesX86[MAX_PATH] = {};
        GetEnvironmentVariableW(L"ProgramFiles",      progFiles,    MAX_PATH);
        GetEnvironmentVariableW(L"ProgramFiles(x86)", progFilesX86, MAX_PATH);

        QString pf     = QString::fromWCharArray(progFiles).toLower();
        QString pfx86  = QString::fromWCharArray(progFilesX86).toLower();
        QString exeLow = exePath.toLower();

        if (!exeLow.startsWith(pf) && !exeLow.startsWith(pfx86)) {
            m_lastError = tr(
                "Auto-clear task refused:\n"
                "The application must be installed in Program Files\n"
                "to create a privileged scheduled task.\n\n"
                "Current path:\n%1").arg(exePath);
            return false;
        }
    }

    std::wstring wsExe  = exePath.toStdWString();
    std::wstring wsArgs = L"--auto-clear";

    ITaskService* pService = nullptr;
    HRESULT hr = connectTaskService(&pService);
    if (FAILED(hr)) {
        m_lastError = QString("ITaskService::Connect failed: 0x%1")
                      .arg(static_cast<DWORD>(hr), 8, 16, QChar('0'));
        return false;
    }

    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(kTaskFolder), &pRootFolder);
    if (FAILED(hr)) {
        m_lastError = "GetFolder failed";
        pService->Release();
        return false;
    }

    ITaskDefinition* pTask = nullptr;
    hr = pService->NewTask(0, &pTask);
    if (FAILED(hr)) {
        m_lastError = "NewTask failed";
        pRootFolder->Release();
        pService->Release();
        return false;
    }

    // Registration info
    IRegistrationInfo* pRegInfo = nullptr;
    pTask->get_RegistrationInfo(&pRegInfo);
    if (pRegInfo) {
        pRegInfo->put_Author(_bstr_t(L"Shutdown Timer"));
        pRegInfo->put_Description(
            _bstr_t(L"Clears the startup message after next login."));
        pRegInfo->Release();
    }

    // Principal: run as logged-in user with highest available privilege.
    // --auto-clear deletes values from HKLM\...\Winlogon which requires
    // elevation — TASK_RUNLEVEL_HIGHEST is necessary for this to work.
    IPrincipal* pPrincipal = nullptr;
    pTask->get_Principal(&pPrincipal);
    if (pPrincipal) {
        pPrincipal->put_UserId(_bstr_t(L""));
        pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
        pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
        pPrincipal->Release();
    }

    // Settings
    ITaskSettings* pSettings = nullptr;
    pTask->get_Settings(&pSettings);
    if (pSettings) {
        pSettings->put_StartWhenAvailable(VARIANT_TRUE);
        pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
        pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
        pSettings->put_ExecutionTimeLimit(_bstr_t(L"PT5M"));
        pSettings->Release();
    }

    // Trigger: ONLOGON - fires for any user login
    ITriggerCollection* pTriggers = nullptr;
    pTask->get_Triggers(&pTriggers);
    if (pTriggers) {
        ITrigger* pTrigger = nullptr;
        pTriggers->Create(TASK_TRIGGER_LOGON, &pTrigger);
        if (pTrigger) {
            pTrigger->put_Id(_bstr_t(L"LogonTrigger"));
            pTrigger->Release();
        }
        pTriggers->Release();
    }

    // Action: run ShutdownTimer.exe --auto-clear
    IActionCollection* pActions = nullptr;
    pTask->get_Actions(&pActions);
    if (pActions) {
        IAction* pAction = nullptr;
        pActions->Create(TASK_ACTION_EXEC, &pAction);
        if (pAction) {
            IExecAction* pExecAction = nullptr;
            pAction->QueryInterface(IID_IExecAction,
                                    reinterpret_cast<void**>(&pExecAction));
            if (pExecAction) {
                pExecAction->put_Path(_bstr_t(wsExe.c_str()));
                pExecAction->put_Arguments(_bstr_t(wsArgs.c_str()));
                pExecAction->Release();
            }
            pAction->Release();
        }
        pActions->Release();
    }

    // Register the task
    IRegisteredTask* pRegisteredTask = nullptr;
    hr = pRootFolder->RegisterTaskDefinition(
        _bstr_t(kTaskName),
        pTask,
        TASK_CREATE_OR_UPDATE,
        _variant_t(), _variant_t(),
        TASK_LOGON_INTERACTIVE_TOKEN,
        _variant_t(L""),
        &pRegisteredTask
    );

    bool ok = SUCCEEDED(hr);
    if (!ok)
        m_lastError = QString("RegisterTaskDefinition failed: 0x%1")
                      .arg(static_cast<DWORD>(hr), 8, 16, QChar('0'));

    if (pRegisteredTask) pRegisteredTask->Release();
    pTask->Release();
    pRootFolder->Release();
    pService->Release();
    return ok;
}

// -- cancel --
// Called when user clears the message or unchecks auto-clear.

bool AutoClearBackendWindows::cancel()
{
    if (!ensureCOM()) return false;

    ITaskService* pService = nullptr;
    HRESULT hr = connectTaskService(&pService);
    if (FAILED(hr)) return true; // Nothing to cancel

    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(kTaskFolder), &pRootFolder);
    if (FAILED(hr)) {
        pService->Release();
        return true;
    }

    hr = pRootFolder->DeleteTask(_bstr_t(kTaskName), 0);
    bool ok = SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    if (!ok)
        m_lastError = QString("DeleteTask failed: 0x%1")
                      .arg(static_cast<DWORD>(hr), 8, 16, QChar('0'));

    pRootFolder->Release();
    pService->Release();
    return ok;
}

// -- exists --
// Called when user clicks "Load Current" to check the auto-clear checkbox state.

bool AutoClearBackendWindows::exists()
{
    if (!ensureCOM()) return false;

    ITaskService* pService = nullptr;
    HRESULT hr = connectTaskService(&pService);
    if (FAILED(hr)) return false;

    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(kTaskFolder), &pRootFolder);
    if (FAILED(hr)) {
        pService->Release();
        return false;
    }

    IRegisteredTask* pTask = nullptr;
    hr = pRootFolder->GetTask(_bstr_t(kTaskName), &pTask);
    bool found = SUCCEEDED(hr);

    if (pTask) pTask->Release();
    pRootFolder->Release();
    pService->Release();
    return found;
}
