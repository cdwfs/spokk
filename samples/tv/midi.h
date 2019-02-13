#include <stdint.h>

// Counts the number of endpoints.
int MidiJackCountEndpoints();
// Get the unique ID of an endpoint.
uint32_t MidiJackGetEndpointIDAtIndex(int index);
// Get the name of an endpoint.
const char* MidiJackGetEndpointName(uint32_t id);
// Retrieve and erase an MIDI message data from the message queue.
uint64_t MidiJackDequeueIncomingData();
// refesh endpoint devices
void MidiJackRefreshEndpoints();