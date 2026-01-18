# include<CoreFoundation/CoreFoundation.h>
# include<CoreAudio/CoreAudio.h>
# include<iostream>
# include<vector>
# include<string>
# include<fstream>
# include <filesystem>
# include<cstdlib>



using namespace std;
namespace fs = std::filesystem;

// holds cf string's chars in a buffer. 
static string cfStringToStd(CFStringRef s){
    if (!s) return {};
    char buff[1024]; // temp storage for apple strings 

    // copies the charaster contents of CFString (core foundation string) to a local c string after converting the chrs into given encoding 
    // kCFStringEncodingUTF8 same as UTF8 encoding just specific to apple CF 
    if (CFStringGetCString (s, buff, sizeof(buff), kCFStringEncodingUTF8) ) return buff;
    
    return {};
}

static bool parseIndex(const char* arg, size_t& outIdx){ // converst cli's string to literal unsinged int. 
    if (!arg || !*arg) return false;

    long v = strtol(arg, nullptr, 10);

    if( v < 0) return false;

    outIdx = static_cast<size_t>(v);
    return true;
}

static vector<size_t> parseIndexList(const char* arg) {
    vector<size_t> result;
    if (!arg || !*arg) return result;

    string s(arg);
    size_t start = 0;

    while (true) {
        size_t comma = s.find(',', start);
        string token = (comma == string::npos) ? s.substr(start) : s.substr(start, comma - start);

        size_t idx = 0;
        if (parseIndex(token.c_str(), idx)) {
            result.push_back(idx);
        } else {
            cerr << "Invalid index skipped: " << token << endl;
        }

        if (comma == string::npos) break;
        start = comma + 1;
    }

    return result;
}


static bool isOutputDevice(AudioDeviceID device){ // checks if the device has atleast 1 output stream 

    AudioObjectPropertyAddress addr{
        kAudioDevicePropertyStreamConfiguration, // the output stream's selection
        kAudioDevicePropertyScopeOutput, // at the scope of output
        kAudioObjectPropertyElementMain // of the main elemnet the default or primary one .
    };
    // audio object property address is a structure that has a selection, a scope and an element essentially what property do you want, for which direction and which elememt ?? 
    
    UInt32 size = 0; //required by core audio. 
    // a qualifier is a keyword that restricts how comething can be used.. we dont need that rn so we say 0, and nullptr

    // core audio's are desinged like this.. how big is the data ? alllocate that buffer now get that data.. hence is size is 0 thats a failed case    THIS IS A VERY COMMON C API PATTERN :) 
    if (AudioObjectGetPropertyDataSize(device, &addr, 0, nullptr, &size) != noErr || size == 0) return false;

    vector<uint8_t> storage(size); // we got the storage now we use it and get data 
    auto* bufList = reinterpret_cast<AudioBufferList*>(storage.data());
    // bufList points to AudioBufferList in memory; storage.data() returns a pointer to the 1st byte of vector stodage in memory 
    // Core Audio defines required memory layout and size, we allocate a buffer of exactly that size and reinterpret cast tells the cpp compiler to treat that memory as an audiobufferlist --- the cast only changes how the compiler interprets teh bytes 

    if ( AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, bufList) != noErr ){
        // here finally were getting data from core audio .  we created the memory allocated its size, now we let this fx to place property data in allocated buffer
        return false;
    }

    UInt32 channels = 0;
    for (UInt32 i = 0; i < bufList->mNumberBuffers; i++){
        // buff list is literally the buffer where core audio writes in audiobufferlist's format becaus its a pointer and not an actual struct like  vectors.. we use -> insted of .
        // bufList->mBuffers literally gets the array of audio buffer enteries
        channels += bufList->mBuffers[i].mNumberChannels;
    }

    return channels > 0;

}

string getDeviceName(AudioDeviceID device){
    AudioObjectPropertyAddress addr{
        kAudioObjectPropertyName, //address structure gets 3 elements.. the selection here name
        kAudioObjectPropertyScopeGlobal, //the scope of output as global
        kAudioObjectPropertyElementMain //the default or primary one.
    };
    CFStringRef name = nullptr;
    UInt32 size = sizeof(name);

    if(AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, &name) != noErr) return{}; 

    string out = cfStringToStd(name);
    if (name) CFRelease(name); // were done with cfstring's reference.. now remove it.. caus c++ is really messed up with garbage.
    return out;
}

string getDeviceUID(AudioDeviceID device){
    AudioObjectPropertyAddress addr{
        kAudioDevicePropertyDeviceUID, //address structure gets 3 elements.. the selection here name
        kAudioObjectPropertyScopeGlobal, //the scope of output as global
        kAudioObjectPropertyElementMain //the default or primary one.
    };
    CFStringRef name = nullptr;
    UInt32 size = sizeof(name);

    if(AudioObjectGetPropertyData(device, &addr, 0, nullptr, &size, &name) != noErr) return{}; 

    string out = cfStringToStd(name);
    if (name) CFRelease(name); // were done with cfstring's reference.. now remove it.. caus c++ is really messed up with garbage.
    return out;
}

static AudioDeviceID getDefaultOutput() {
    AudioDeviceID device = kAudioObjectUnknown; // essentially saying int i = 0... audio device id is uint32, device is i , kaudioobjectunknownn is a known garbade value like 0.

    // kAudioObjectSystemObject is a special constant id that represents global core audio system itself .. 

    AudioObjectPropertyAddress addr{
        kAudioHardwarePropertyDefaultOutputDevice, 
        kAudioObjectPropertyScopeGlobal, 
        kAudioObjectPropertyElementMain
    };
    
    UInt32 size = sizeof(device);
    if(AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, &device) != noErr) 
        return kAudioObjectUnknown;
    
    return device;
}


static OSStatus setDefaultOutput(AudioDeviceID device){
    // OSStatus is return data type used to undicate sucess or faliur of a fx
    AudioObjectPropertyAddress addr{
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    UInt32 size = sizeof(device);

    return AudioObjectSetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, size, &device);
}


static vector<AudioDeviceID> getALlDevices() {
    AudioObjectPropertyAddress addr{
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size) != noErr || size == 0) return {};
    UInt32 count = size / sizeof(AudioDeviceID);
    vector<AudioDeviceID> devices(count);

    if(AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, devices.data()) != noErr) return {};

    return devices;
}

static AudioDeviceID findDeviceByUID(const string& tergetUID){
    auto devices = getALlDevices();
    for (auto device : devices){
        auto uid = getDeviceUID(device);
        if( !uid.empty() && uid == tergetUID) return device;
    }

    return kAudioObjectUnknown;
}


// HELPERS for reading and writing output names. 

static string configPath() {
    const char* home = getenv("HOME"); // gets the home directry on any mac user. as a key value pair
    if (!home) return "config.txt";
    return string(home) + "/.config/cliaud/config.txt";
}

static void esnusreConfigDir(){
    const char* home = getenv("HOME");
    if (!home) return;
    fs::create_directories(string(home) + "/.config/cliaud");
}

static bool readUIDList(vector<string>& uids){
    uids.clear();

    ifstream inf(configPath());
    if(!inf) return false;

    string line;
    while( getline(inf, line)){
        if(line.rfind("UID=", 0) == 0) {uids.push_back(line.substr(4)) ;}
    }    
    return !uids.empty();
}

static bool writeUIDList(const vector<string>& uids){
    esnusreConfigDir();
    ofstream outf(configPath(), ios::trunc);
    if(!outf) return false;

    for (const auto& uid : uids){
        outf << "UID=" << uid << endl;
    }
    return true;
}

static bool addUIDToConfig(const string& uid) {
    if (uid.empty()) return false;

    vector<string> uids;
    readUIDList(uids);

    for(const auto& existing : uids){
        if (existing == uid) return true; // uid is already present
    }

    uids.push_back(uid);
    return writeUIDList(uids);
}


// custom structure for AudioOBjects. 
struct ListedDevice {
    AudioDeviceID id;
    string name;
    string uid;
    bool outputChanel;
};

static vector<ListedDevice> buildSelectableList(){
    vector<ListedDevice> list;
    auto devices = getALlDevices();

    for ( auto device : devices){
        string name = getDeviceName(device);
        string uid = getDeviceUID(device);
        if (name.empty() || uid.empty()) continue;
        if (!isOutputDevice(device)) continue;  // hard filtering only output chanels later can have more options 

        list.push_back(ListedDevice{
            device, name, uid, true
        });
    }
    return list;

}


// cmdlist as a helper.. 
static int cmdList() {
    auto list = buildSelectableList();
    if(list.empty()){
        cerr << "no devices found" << endl;
        return 1;
    }

    AudioDeviceID def = getDefaultOutput();
    string defUID = (def != kAudioObjectUnknown) ? getDeviceUID(def) : "";

    cout << "Selectable devices: " << endl;

    for (int i = 0; i < list.size(); i++){
        const auto& it = list[i];
        bool ifDef = (!defUID.empty() && it.uid == defUID);
        cout << "[ " << i << " ]" << (ifDef ? "-*" : "  ") << it.name << endl;
    }
    return 0;
}

static int cmdAdd(const char* idxArg) {
    auto indices = parseIndexList(idxArg);
    if (indices.empty()) {
        cerr << "No valid indices provided" << endl; return 1;
    }

    auto list = buildSelectableList();
    if (list.empty()) {
        cerr << "No selectable devices" << endl; return 1;
    }

    bool addedAny = false;

    for (size_t idx : indices) {
        if (idx >= list.size()) {
            cerr << "Index out of range skipped: " << idx << endl;
            continue;
        }

        const auto& chosen = list[idx];

        if (addUIDToConfig(chosen.uid)) {
            cout << "Added: " << chosen.name << endl;
            addedAny = true;
        } else {
            cerr << "Failed to add: " << chosen.name << endl;
        }
    }

    return addedAny ? 0 : 1;
}


static int cmdShow(){
    vector<string> uids;
    if(!readUIDList(uids)){
        cerr << "no sabed devices " << endl; return 1;
    }
    cout << "Saved devices: " << endl;
    for (size_t i = 0; i < uids.size(); i++){
        AudioDeviceID device = findDeviceByUID(uids[i]);
        if(device == kAudioObjectUnknown){
            cout << "[ " << i << " ] missing uid -> " << uids[i] << endl; 
        } else {
            cout << "[ " << i << " ]" << getDeviceName(device) << endl; 
        }
    }
    return 0;
}

static int cmdCycle() {
    vector<string> savedUIDs;
    if(!readUIDList(savedUIDs)){
        cerr << "Config is missing" << endl; return 1;
    }

    vector<AudioDeviceID> available;
    vector<string> availableUIDs;

    for (const auto& uid : savedUIDs) {
        AudioDeviceID device = findDeviceByUID(uid);
        if( device != kAudioObjectUnknown){
            available.push_back(device);
            availableUIDs.push_back(uid);
        }
    }

    if (available.empty()){
        cerr << "None of the saved devices are currently available" << endl; return 1;
    }

    AudioDeviceID cur = getDefaultOutput();
    string curUID = (cur != kAudioObjectUnknown) ? getDeviceUID(cur) : "";

    size_t targetIdx = 0;

    if(!curUID.empty()){
        size_t pos = 0;
        bool found = false;

        for ( ; pos < availableUIDs.size(); pos++){
            if (availableUIDs[pos] == curUID) { found = true; break; }
        }

        if (found){targetIdx = (pos + 1) % available.size();}
        else targetIdx = 0;
    } else {
        targetIdx = 0;
    }

    AudioDeviceID target = available[targetIdx];
    OSStatus st = setDefaultOutput(target);

    if(st != noErr){
        cerr << "Failed to set default output" << endl; return 1;
    }

    cout << "Switched output to " << getDeviceName(target) << endl;
    return 0;
}


static int cmdClear() {
    vector<string> empty;
    if (!writeUIDList(empty)) {
        cerr << "Failed to clear config\n";
        return 1;
    }
    cout << "Cleared saved devices.\n";
    return 0;
}



int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "Usage:" << endl;
        cout << "cliaud list" << endl;
        cout << "cliaud add <indexList>" << endl;
        cout << "cliaud show" << endl;
        cout << "cliaud cycle" << endl;
        cout << "cliaud clear" << endl; return 1;
    }

    string cmd = argv[1];

    if (cmd == "list")  return cmdList();
    if (cmd == "add") {
        if (argc < 3) { cout << "Missing index. use ./cliaud list" << endl; return 1; }
        return cmdAdd(argv[2]);
    }
    if (cmd == "show")  return cmdShow();
    if (cmd == "cycle") return cmdCycle();
    if (cmd == "clear") return cmdClear();

    cerr << "Unknown command: " << cmd << endl;
    return 1;
}

