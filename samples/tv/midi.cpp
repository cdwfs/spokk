#include "midi.h"

#include <windows.h>

#include <mmsystem.h>

#include <stdint.h>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <stack>
#include <string>

namespace {
// Basic type aliases
using DeviceHandle = HMIDIIN;
using DeviceID = uint32_t;

// Utility functions for Win32/64 compatibility
#ifdef _WIN64
DeviceID DeviceHandleToID(DeviceHandle handle) { return static_cast<DeviceID>(reinterpret_cast<uint64_t>(handle)); }
DeviceHandle DeviceIDToHandle(DeviceID id) { return reinterpret_cast<DeviceHandle>(static_cast<uint64_t>(id)); }
#else
DeviceID DeviceHandleToID(DeviceHandle handle) { return reinterpret_cast<DeviceID>(handle); }
DeviceHandle DeviceIDToHandle(DeviceID id) { return reinterpret_cast<DeviceHandle>(id); }
#endif

// MIDI message storage class
class MidiMessage {
  DeviceID source_;
  uint8_t status_;
  uint8_t data1_;
  uint8_t data2_;

public:
  MidiMessage(DeviceID source, uint32_t rawData)
    : source_(source), status_((uint8_t)rawData), data1_((uint8_t)(rawData >> 8)), data2_((uint8_t)(rawData >> 16)) {}

  uint64_t Encode64Bit() {
    uint64_t ul = source_;
    ul |= (uint64_t)status_ << 32;
    ul |= (uint64_t)data1_ << 40;
    ul |= (uint64_t)data2_ << 48;
    return ul;
  }

  std::string ToString() {
    char temp[256];
    std::snprintf(temp, sizeof(temp), "(%X) %02X %02X %02X", source_, status_, data1_, data2_);
    return temp;
  }
};

// Incoming MIDI message queue
std::queue<MidiMessage> message_queue;

// Device handler lists
std::list<DeviceHandle> active_handles;
std::stack<DeviceHandle> handles_to_close;

// Mutex for resources
std::mutex resource_lock;

// MIDI input callback
static void CALLBACK MidiInProc(
    HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR /*dwInstance*/, DWORD_PTR dwParam1, DWORD_PTR /*dwParam2*/) {
  if (wMsg == MIM_DATA) {
    std::lock_guard<std::mutex> guard(resource_lock);
    DeviceID id = DeviceHandleToID(hMidiIn);
    uint32_t raw = static_cast<uint32_t>(dwParam1);
    message_queue.push(MidiMessage(id, raw));
  } else if (wMsg == MIM_CLOSE) {
    std::lock_guard<std::mutex> guard(resource_lock);
    handles_to_close.push(hMidiIn);
  }
}

// Retrieve a name of a given device.
std::string GetDeviceName(DeviceHandle handle) {
  auto casted_id = reinterpret_cast<UINT_PTR>(handle);
  MIDIINCAPS caps;
  if (midiInGetDevCaps(casted_id, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
    std::wstring name(caps.szPname);
    return std::string(name.begin(), name.end());
  }
  return "unknown";
}

// Open a MIDI device with a given index.
void OpenDevice(unsigned int index) {
  static const DWORD_PTR callback = reinterpret_cast<DWORD_PTR>(MidiInProc);
  DeviceHandle handle;
  if (midiInOpen(&handle, index, callback, NULL, CALLBACK_FUNCTION) == MMSYSERR_NOERROR) {
    if (midiInStart(handle) == MMSYSERR_NOERROR) {
      std::lock_guard<std::mutex> guard(resource_lock);
      active_handles.push_back(handle);
    } else {
      midiInClose(handle);
    }
  }
}

// Close a given handler.
void CloseDevice(DeviceHandle handle) {
  midiInStop(handle);
  midiInReset(handle);
  midiInClose(handle);

  {
    std::lock_guard<std::mutex> guard(resource_lock);
    active_handles.remove(handle);
  }
}

// Open the all devices.
void OpenAllDevices() {
  int device_count = midiInGetNumDevs();
  for (int i = 0; i < device_count; i++) {
    OpenDevice(i);
  }
}

// Refresh device handlers
void RefreshDevices() {
  // Close disconnected handlers.
  while (!handles_to_close.empty()) {
    CloseDevice(handles_to_close.top());
    handles_to_close.pop();
  }

  // Try open all devices to detect newly connected ones.
  OpenAllDevices();
}

// Close the all devices.
void CloseAllDevices() {
  while (!active_handles.empty()) CloseDevice(active_handles.front());
}
}  // namespace

// Counts the number of endpoints.
int MidiJackCountEndpoints() { return static_cast<int>(active_handles.size()); }

// Get the unique ID of an endpoint.
uint32_t MidiJackGetEndpointIDAtIndex(int index) {
  auto itr = active_handles.begin();
  std::advance(itr, index);
  return DeviceHandleToID(*itr);
}

// Get the name of an endpoint.
const char* MidiJackGetEndpointName(uint32_t id) {
  auto handle = DeviceIDToHandle(id);
  static std::string buffer;
  buffer = GetDeviceName(handle);
  return buffer.c_str();
}

void MidiJackRefreshEndpoints() { RefreshDevices(); }

// Retrieve and erase an MIDI message data from the message queue.
uint64_t MidiJackDequeueIncomingData() {
  if (active_handles.empty()) {
    RefreshDevices();
  }
  if (message_queue.empty()) {
    return 0;
  }

  {
    std::lock_guard<std::mutex> guard(resource_lock);
    auto msg = message_queue.front();
    message_queue.pop();
    return msg.Encode64Bit();
  }
}

void MidiJackShutdown() { CloseAllDevices(); }
