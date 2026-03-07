add_library(SonatrixCore STATIC
    EngineConfig.h
    mir/MusicalTime.h
    mir/MIRPattern.h
    mir/DeltaGraph.h
    arrangement/ChordTrack.h
    midi/MIDIEvent.h
    midi/MIDIEvent.cpp
    midi/IMIRCompiler.h
    midi/GuitarCompiler.h
    midi/GuitarCompiler.cpp
    ml/DynamicGrooveVector.h
    ml/DynamicGrooveVector.cpp
    ml/NeuralTimbreReshaper.h
    ml/NeuralTimbreReshaper.cpp
    engines/drums/DrumCompiler.h
    engines/drums/DrumCompiler.cpp
    engines/bass/BassCompiler.h
    engines/bass/BassCompiler.cpp
    engines/piano/PianoCompiler.h
    engines/piano/PianoCompiler.cpp
    engines/strings/StringCompiler.h
    engines/strings/StringCompiler.cpp
    audio/SampleZone.h
    audio/SampleZone.cpp
    audio/AudioVoice.h
    audio/AudioVoice.cpp
    audio/VoiceManager.h
    audio/VoiceManager.cpp
    audio/AudioFileReader.h
    audio/AudioFileReader.cpp
    audio/AudioMixer.h
    audio/AudioMixer.cpp
    concurrency/SPSCQueue.h
)
target_include_directories(SonatrixCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(SonatrixCore PUBLIC cxx_std_20)
