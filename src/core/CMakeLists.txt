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
)
target_include_directories(SonatrixCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(SonatrixCore PUBLIC cxx_std_20)
