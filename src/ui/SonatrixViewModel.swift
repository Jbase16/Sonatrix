import Combine
import SwiftUI

// -----------------------------------------------------------------------------
// SonatrixViewModel
//
// The central @Observable state container for the SwiftUI layer.
// It safely wraps the Objective-C++ `SonatrixEngineFacade` and exposes
// transport controls and arrangement mutations to the UI cleanly.
// -----------------------------------------------------------------------------

public class SonatrixViewModel: ObservableObject {

    // The bridged Objective-C++ Engine orchestrator
    private var engineFacade: SonatrixEngineFacade

    // Expose basic UI state
    @Published public var isPlaying: Bool = false
    @Published public var isCompiling: Bool = false

    // -----------------------------------------------------------------------------
    // Shared Data Models
    // -----------------------------------------------------------------------------
    public struct ChordItem: Identifiable, Equatable {
        public let id = UUID()
        public var rootName: String
        public var qualityName: String
        // For C++ interop
        public var rootIndex: UInt8
        public var qualityIndex: UInt8
        public var durationTicks: UInt16 = 1920  // 1 bar at 480 PPQ
    }

    // In a full implementation, this would hold an array of custom Swift structs
    // mapped back to the active C++ arrangement blocks.
    @Published public var arrangementChords: [ChordItem] = []

    // Mixer State (Drums, Bass, Guitar, Piano, Strings)
    // Indexes match the C++ MixerBus enum
    @Published public var busVolumes: [Float] = [0.8, 0.8, 0.8, 0.8, 0.8]

    public init() {
        self.engineFacade = SonatrixEngineFacade()
    }

    // MARK: - Transport API

    public func togglePlayback() {
        if isPlaying {
            engineFacade.stop()
            isPlaying = false
        } else {
            engineFacade.play()
            isPlaying = true
        }
    }

    // MARK: - Mixer API

    public func setVolume(bus: Int, volume: Float) {
        if bus >= 0 && bus < busVolumes.count {
            busVolumes[bus] = volume
            engineFacade.setVolume(volume, forBus: UInt8(bus))
        }
    }

    // MARK: - Arrangement API

    public func clearArrangement() {
        engineFacade.clearChords()
        arrangementChords.removeAll()
    }

    public func addChord(_ chord: ChordItem) {
        arrangementChords.append(chord)
        compileArrangement()
    }

    public func removeChord(at index: Int) {
        if index >= 0 && index < arrangementChords.count {
            arrangementChords.remove(at: index)
            compileArrangement()
        }
    }

    public func updateChord(at index: Int, with newChord: ChordItem) {
        if index >= 0 && index < arrangementChords.count {
            arrangementChords[index] = newChord
            compileArrangement()
        }
    }

    private func compileArrangement() {
        isCompiling = true
        engineFacade.clearChords()

        // Push chords to C++ based on the dynamic SwiftUI array
        var currentTick: UInt64 = 0
        for chord in arrangementChords {
            engineFacade.addChord(
                withRoot: chord.rootIndex,
                quality: chord.qualityIndex,
                tickOffset: Double(currentTick))
            currentTick += UInt64(chord.durationTicks)
        }

        // Tell the C++ layer to run the Viterbi Graph Solvers and Neural latencies
        engineFacade.compileAndSchedule()

        // Once scheduled, ensure playback is on
        if !isPlaying {
            engineFacade.play()
            isPlaying = true
        }

        isCompiling = false
    }
}
