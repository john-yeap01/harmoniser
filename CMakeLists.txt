cmake_minimum_required (VERSION 3.22)
project (Harmoniser VERSION 0.1.0)

add_subdirectory (JUCE)             # JUCE folder lives beside this file

juce_add_plugin (Harmoniser
    PRODUCT_NAME              "Harmoniser2"
    PLUGIN_MANUFACTURER_CODE  John
    PLUGIN_CODE               Grad
    FORMATS                   AU VST3 Standalone
    NEEDS_MIDI_INPUT          TRUE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT
        "This plug‑in needs the mic so it can pitch‑shift your voice."
)

target_sources (Harmoniser PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
)

target_link_libraries (Harmoniser PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)
