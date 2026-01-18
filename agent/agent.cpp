# include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
# include <cstdio>
# include <cstdlib>
# include <string>
# include <fstream>
# include <filesystem>
# include <cstdlib>

using namespace std;


static string readCliaudPath(){
    const char* home = getenv("HOME");
    if(!home) return "";

    ifstream inf(string(home) + "/.config/cliaud/agent.conf");

    if (!inf) {
        fprintf(stderr, "Could not open agent.conf");
        return "";
    }


    string line;
    while(getline(inf, line)){
        if (line.rfind("CLIAUD_PATH=", 0) == 0)
            return line.substr(12);
    } 
    return "";
}

static void runCycle() {
   string path = readCliaudPath();
   if (path.empty()) return;

   fprintf(stderr, "CLIAUD_PATH='%s", path.c_str());
   fflush(stderr);     

   string cmd = "\"" + path + "\" cycle";
   system(cmd.c_str());
}

static OSStatus HotKeyHandler(EventHandlerCallRef, EventRef event, void*) {
    fprintf(stderr, "Handler fired");
    fflush(stderr);

    EventHotKeyID hkID{};
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(hkID), nullptr, &hkID);
    fprintf(stderr, "sig=%c%c%c%c id=%u",
        (hkID.signature >> 24) & 0xFF,
        (hkID.signature >> 16) & 0xFF,
        (hkID.signature >> 8) & 0xFF,
        (hkID.signature) & 0xFF,
        (unsigned)hkID.id);
    fflush(stderr);


    if(hkID.signature == 'CLAD' && hkID.id == 1){
        runCycle();
    }
    return noErr;
}

int main(){
    EventTypeSpec eventType{kEventClassKeyboard, kEventHotKeyPressed};
    InstallApplicationEventHandler(HotKeyHandler, 1, &eventType, nullptr, nullptr);

    EventHotKeyID hkID{ 'CLAD', 1};
    EventHotKeyRef hkRef = nullptr;

    OSStatus st = RegisterEventHotKey(kVK_ANSI_9, optionKey | cmdKey, hkID, GetApplicationEventTarget(), 0, &hkRef);

    if(st != noErr){
        fprintf(stderr, "redister event hotkey failed: %d ", (int)st); return 1;
    }

    fprintf(stderr, "Hotkey registered OK. Press it now...");
    fflush(stderr);


    for (;;) {
        EventRef event = nullptr;
        OSStatus err = ReceiveNextEvent(0, nullptr, kEventDurationForever, true, &event);
        if (err == noErr && event) {
            SendEventToEventTarget(event, GetApplicationEventTarget());
            ReleaseEvent(event);
        }
    }

    return 0;

}