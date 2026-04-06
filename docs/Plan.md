# Plugin management already implemented in LiveEffectEngine
- addPlugin (int position, std::string uri)
- deletePlugin (int plugin)
- getPresetList ()
- getPreset (int plugin)
- setFilePath (int position, std::string uri, std::string path)
- startRecording(int fd, int file_type, int quality)
- stopRecording()
- process (float* input, float* output, int frames)

# Plan for Opiqo LV2 plugin host for Linux
## User Interface
- Interface for 4 plugin slots
- Each plugin slot has:
  - A plus button to add a plugin, click to open plugin dialog
  - A bypass toggle
  - A delete button
  - Dynamically generated sliders for each plugin parameter
- 300px Control bar at the bottom with Power toggle, Gain slider, and Record button
- Dropdown menu for input / output device selection
- Drop down Format and quality selection for recording
- Status bar for messages and errors, and display current sample rate and block size
- Settings dialog for configuring audio settings and application preferences
  - Export / import presets functionality
  - Sample rate and block size configuration

## Add Plugin Dialog
- Get json list of available plugins from LiveEffectEngine::getAvailablePlugins()
- Display plugin name and URI in a list
- User selects a plugin and clicks "Add" to load it into the selected slot

## Audio Processing
- Use JACK for low-latency audio input and output
- Use a separate audio thread to call LiveEffectEngine::process() in a loop
- Pass audio data from input device to process() and then to output device
- Ensure that parameter changes from the UI are thread-safe and do not cause audio glitches
- Default sample rate to 48000 Hz and block size to 4096 samples, with options to change in settings
- Get default sample rate and block size from JACK and configure LiveEffectEngine accordingly
- Get default input and output devices from JACK and allow user to change in settings

## Build system
- Use CMake to manage the build process
- Link against the necessary libraries: JACK, Lilv, LV2, and any dependencies

## Guidelines
- Keep the LV2 host core completely UI-agnostic and introduce a thin Linux adapter layer (JACK + GTK binding). That will reduce risk and preserve portability.
- Use C++17 for the Linux port
- Follow best practices for Linux application development, including proper error handling and resource management
- Ensure that the application is responsive and does not block the UI thread during audio processing or plugin management operations
- Provide clear documentation and comments in the code for maintainability and future development
- - Use a separate thread for audio processing to keep the UI responsive
- Ensure that UI interactions update the plugin parameters in real-time without blocking the audio thread
- Parameter updates from the Linux thread need lock-free/atomic transfer to the audio thread to avoid glitches.
- Update plugin parameters in real-time based on UI interactions
- Ensure that the audio thread is not blocked by UI operations or plugin loading/unloading
- Handle any errors gracefully and display messages in the status bar
- Ensure that the application can handle sample rate changes and device changes without crashing