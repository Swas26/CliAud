#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

static string findCliaudPath() {
    // 1) Allow override if you ever want it
    if (const char* env = getenv("CLIAUD_PATH")) {
        if (*env && access(env, X_OK) == 0) return string(env);
    }

    // 2) Common Homebrew locations
    const char* candidates[] = {
        "/opt/homebrew/bin/cliaud", // Apple Silicon
        "/usr/local/bin/cliaud",    // Intel Homebrew
        nullptr
    };

    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], X_OK) == 0) return string(candidates[i]);
    }

    // 3) Fallback: rely on PATH (may fail in LaunchAgents)
    return "cliaud";
}

static void runCycle() {
    string cliaudPath = findCliaudPath();

    const char* argv[] = { cliaudPath.c_str(), "cycle", nullptr };

    pid_t pid = 0;
    int status = 0;

    int err = posix_spawn(&pid, cliaudPath.c_str(), nullptr, nullptr, (char* const*)argv, environ);
    if (err != 0) {
        fprintf(stderr, "[cliaud-agent] spawn failed err=%d path='%s'\n", err, cliaudPath.c_str());
        fflush(stderr);
        return;
    }

    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "[cliaud-agent] waitpid failed\n");
        fflush(stderr);
        return;
    }

    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    fprintf(stderr, "[cliaud-agent] ran: %s cycle (exit=%d)\n", cliaudPath.c_str(), exitCode);
    fflush(stderr);
}

static OSStatus HotKeyHandler(EventHandlerCallRef, EventRef event, void*) {
    EventHotKeyID hkID{};
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID,
                      nullptr, sizeof(hkID), nullptr, &hkID);

    if (hkID.signature == 'CLAD' && hkID.id == 1) {
        runCycle();
    }
    return noErr;
}

int main() {
    EventTypeSpec eventType { kEventClassKeyboard, kEventHotKeyPressed };
    InstallApplicationEventHandler(HotKeyHandler, 1, &eventType, nullptr, nullptr);

    EventHotKeyID hkID { 'CLAD', 1 };
    EventHotKeyRef hkRef = nullptr;

    OSStatus st = RegisterEventHotKey(kVK_ANSI_9, optionKey | cmdKey,
                                      hkID, GetApplicationEventTarget(), 0, &hkRef);

    if (st != noErr) {
        fprintf(stderr, "[cliaud-agent] RegisterEventHotKey failed: %d\n", (int)st);
        return 1;
    }

    fprintf(stderr, "[cliaud-agent] Hotkey registered: Cmd+Option+9\n");
    fflush(stderr);

    for (;;) {
        EventRef eventRef = nullptr;
        OSStatus err = ReceiveNextEvent(0, nullptr, kEventDurationForever, true, &eventRef);
        if (err == noErr && eventRef) {
            SendEventToEventTarget(eventRef, GetApplicationEventTarget());
            ReleaseEvent(eventRef);
        }
    }
}
