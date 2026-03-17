cmake_minimum_required(VERSION 3.22)
project(Afroplug VERSION 0.1.0)

# ── macOS universal binary (Intel + Apple Silicon) ───────────────────────────
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
    set(CMAKE_OSX_ARCHITECTURES    "arm64;x86_64" CACHE STRING "Universal binary" FORCE)
endif()

# ── JUCE ─────────────────────────────────────────────────────────────────────
add_subdirectory(JUCE)

# ── Platform-conditional copy destinations ───────────────────────────────────
# Pass -DAFROPLUG_COPY_AFTER_BUILD=OFF in CI to skip the privileged system copy.
option(AFROPLUG_COPY_AFTER_BUILD "Copy plugin to system folder after build" ON)

if(WIN32)
    set(_VST3_DIR "C:/Program Files/Common Files/VST3")
    set(_AU_DIR   "")                       # AU not supported on Windows
elseif(APPLE)
    set(_VST3_DIR "/Library/Audio/Plug-Ins/VST3")
    set(_AU_DIR   "/Library/Audio/Plug-Ins/Components")
endif()

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(Afroplug
    # ── Identity ──────────────────────────────────────────────────────────────
    COMPANY_NAME                "Afroplug"
    BUNDLE_ID                   "com.afroplug.plugin"   # Required for AU on macOS
    PLUGIN_MANUFACTURER_CODE    AplG                    # Unique 4-char manufacturer code
    PLUGIN_CODE                 Af01                    # Unique 4-char plugin code

    # ── Formats ───────────────────────────────────────────────────────────────
    # AU is only compiled when building on macOS (JUCE ignores it on Windows)
    FORMATS                     VST3 AU

    # ── Plugin properties ─────────────────────────────────────────────────────
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    PRODUCT_NAME                "Afroplug"

    # ── Post-build copy ───────────────────────────────────────────────────────
    COPY_PLUGIN_AFTER_BUILD     ${AFROPLUG_COPY_AFTER_BUILD}
    VST3_COPY_DIR               "${_VST3_DIR}"
    AU_COPY_DIR                 "${_AU_DIR}"
)

juce_generate_juce_header(Afroplug)

target_sources(Afroplug PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Afroplug PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Afroplug PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
