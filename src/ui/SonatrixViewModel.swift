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
    public struct ChordItem: Identifiable, Equatable, Codable {
        public var id = UUID()
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

    // Serialization Model
    public struct ProjectState: Codable {
        public let chords: [ChordItem]
        public let busVolumes: [Float]
    }

    public init() {
        self.engineFacade = SonatrixEngineFacade()
    }

    // MARK: - Project File API

    public func saveProject(to url: URL) throws {
        let state = ProjectState(chords: arrangementChords, busVolumes: busVolumes)
        let encoder = JSONEncoder()
        encoder.outputFormatting = .prettyPrinted
        let data = try encoder.encode(state)
        try data.write(to: url)
    }

    public func loadProject(from url: URL) throws {
        let data = try Data(contentsOf: url)
        let decoder = JSONDecoder()
        let state = try decoder.decode(ProjectState.self, from: data)

        DispatchQueue.main.async {
            self.arrangementChords = state.chords
            for (index, volume) in state.busVolumes.enumerated() {
                if index < self.busVolumes.count {
                    self.setVolume(bus: index, volume: volume)
                }
            }
            self.compileArrangement()
        }
    }

    // MARK: - Export API

    public func bounceAudio(to url: URL) throws {
        let nsVolumes = busVolumes.map { NSNumber(value: $0) }

        guard let resourcePath = Bundle.main.resourcePath else {
            throw NSError(domain: "Sonatrix.AssetsNotFound", code: 1, userInfo: nil)
        }
        let assetsPath = resourcePath + "/Assets"

        let success = engineFacade.bounceAudio(
            toPath: url.path,
            assetsPath: assetsPath,
            volumes: nsVolumes)
        if !success {
            throw NSError(domain: "Sonatrix.BounceFailed", code: 2, userInfo: nil)
        }
    }

    public func exportMIDI(to url: URL) throws {
        let success = engineFacade.exportMIDI(toPath: url.path)
        if !success {
            throw NSError(domain: "Sonatrix.MIDIExportFailed", code: 3, userInfo: nil)
        }
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
