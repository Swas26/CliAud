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

static AudioDeviceID findOutputDeviceByName(const string& targetname){
    auto devices = getALlDevices();
    for (auto device : devices){
        if(!isOutputDevice(device)) continue; // skips devices with no output channels
        auto name = getDeviceName(device);
        if( !name.empty() && name == targetname) return device;
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

// static void rtrim(string& s){
//     while(!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')){
//         s.pop_back();
//     }
// }


static bool readConfig(string& A, string& B){ // gets a and b from config.txt and returns true if bothe are there 
    A.clear(); B.clear();
    ifstream in(configPath());
    if (!in) return false;

    string line;
    while(getline(in, line)){
        // if (line.rfind("A=", 0) == 0) { A = line.substr(2); rtrim(A); }
        // else if (line.rfind("B=", 0) == 0) { B = line.substr(2); rtrim(B); }
        
        if (line.rfind("A=", 0) == 0) { A = line.substr(2);  }
        else if (line.rfind("B=", 0) == 0) { B = line.substr(2); }

    }
    return !(A.empty() || B.empty());
}

static bool writeConfig(const string& A, const string& B){
    esnusreConfigDir();
    ofstream out(configPath(), ios::trunc);
    if(!out) return false;
    out << "A=" << A << endl;
    out << "B=" << B << endl;
    return true;
}



// cmdlist as a helper.. 
static int cmdList() {
    auto devices = getALlDevices();
    if(devices.empty()){
        cerr << "Failed to get devices list " << endl;
        return 1;
    }

    const AudioDeviceID def = getDefaultOutput();
    cout << "Output devices: " << endl;

    for (auto dev : devices ) {
        if (!isOutputDevice(dev)) continue;
        auto name = getDeviceName(dev);
        if(name.empty()) continue;
        cout << ( (dev == def) ? "-*" : " ") << dev << " " << name << endl;
    }
    return 0;
}

static int cmdSet(char which, const string& name){ // this updates the config 
    string A, B;
    readConfig(A, B); // will return false if file dosent exists yet.

    if (which == 'A') A = name;
    else B = name;

    if(!writeConfig(A, B)){
        cerr << "Failed to write config" << endl;
        return 1;
    }
    return 0;

}

static int cmdToggle() {
    string Aname, Bname;
    if(!readConfig(Aname, Bname)){
        cerr << "Config missing" << endl;
        return 1;
    }
    
    AudioDeviceID Aid = findOutputDeviceByName(Aname);
    AudioDeviceID Bid = findOutputDeviceByName(Bname);

    if(Aid == kAudioObjectUnknown && Bid == kAudioObjectUnknown) {
        cerr << "Neither configured device is available" << endl;
        return 1; 
    }

    AudioDeviceID cur = getDefaultOutput();
    AudioDeviceID target = kAudioObjectUnknown;

    if (Aid != kAudioObjectUnknown && Bid != kAudioObjectUnknown){
        target = (cur == Aid) ? Bid : Aid; // if current default is aid get bid.. else get aid and make it target. 
    } else if (Aid != kAudioObjectUnknown){
        target = Aid;
    } else {
        target = Bid;
    }

    OSStatus st = setDefaultOutput(target);

    if (st != noErr){
        cerr << "Failed to set default output OSStatus = " << st << endl;
        return 1;
    }

    cout << "Switched output to: " << getDeviceName(target) << endl;
    return 0;
}


int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage:\n"
             << "  cliaud list\n"
             << "  cliaud set-a \"Device Name\"\n"
             << "  cliaud set-b \"Device Name\"\n"
             << "  cliaud toggle\n";
        return 1;
    }

    string cmd = argv[1];
    if (cmd == "list") return cmdList();
    if (cmd == "set-a") {
        if (argc < 3) { 
            cerr << "Missing device name" << endl;
            return 1; }

        return cmdSet('A', argv[2]);
    }
    
    if (cmd == "set-b") {
        if (argc < 3) { 
            cerr << "Missing device name" << endl;
            return 1; }

        return cmdSet('B', argv[2]);
    }
    if (cmd == "toggle") return cmdToggle();

    cerr << "Unknown command: " << cmd << endl;
    return 1;
}

