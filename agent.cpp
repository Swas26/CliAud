#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

extern char **environ;

static void runCycle() {
   const char* argv[] = {"cliaud", "cycle", nullptr};

   pid_t pid = 0; // pid_t is a datatype that representa process ids. and process ids are nums assinged by OS to each running task.
   int status = 0; 

   int err = posix_spawn(&pid, "cliaud", nullptr, nullptr, (char * const*)argv, environ); // calls a new child process that executes a specific file 
   if (err != 0) {
    fprintf(stderr, "cliaud agent posix_spawn failed err = %d", err);
    fflush(stderr);
    return;
   }
   if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "cliaud agent waitpid failed" );
        fflush(stderr);
        return;
    }
}

static OSStatus HotKeyHandler(EventHandlerCallRef, EventRef event, void*) {
    EventHotKeyID hkID{};
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID,nullptr, sizeof(hkID), nullptr, &hkID);

    if (hkID.signature == 'CLAD' && hkID.id == 1) {
        runCycle();
    }
    return noErr;
}

int main() {
    // Register handler for hotkey pressed
    EventTypeSpec eventType { kEventClassKeyboard, kEventHotKeyPressed };
    InstallApplicationEventHandler(HotKeyHandler, 1, &eventType, nullptr, nullptr);

    // Default hotkey: Cmd + Option + 9
    EventHotKeyID hkID { 'CLAD', 1 };
    EventHotKeyRef hkRef = nullptr;

    OSStatus st = RegisterEventHotKey(kVK_ANSI_9, optionKey | cmdKey,hkID, GetApplicationEventTarget(), 0, &hkRef);

    if (st != noErr) {
        fprintf(stderr, "cliaud agent RegisterEventHotKey failed: %d", (int)st);
        return 1;
    }

    fprintf(stderr, "cliaud agent Hotkey registered: Cmd+Option+9");
    fflush(stderr);

    // Event loop
    for (;;) {
        EventRef eventRef = nullptr;
        OSStatus err = ReceiveNextEvent(0, nullptr, kEventDurationForever, true, &eventRef);
        if (err == noErr && eventRef) {
            SendEventToEventTarget(eventRef, GetApplicationEventTarget());
            ReleaseEvent(eventRef);
        }
    }
    return 0;
}