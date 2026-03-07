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

    // In a full implementation, this would hold an array of custom Swift structs
    // mapped back to the active C++ arrangement blocks.
    @Published public var chordsInArrangement: Int = 0

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

    // MARK: - Arrangement API

    public func clearArrangement() {
        engineFacade.clearChords()
        chordsInArrangement = 0
    }

    public func triggerCompilerTest() {
        isCompiling = true

        // Let's drop a basic I-VI-IV-V progression into the C++ State Matrix

        // 1. C Major
        engineFacade.addChord(withRoot: 0, quality: 0, tickOffset: 0)

        // 2. A Minor
        engineFacade.addChord(withRoot: 9, quality: 1, tickOffset: 1920)  // Assumes 4 bars roughly

        // 3. F Major
        engineFacade.addChord(withRoot: 5, quality: 0, tickOffset: 3840)

        // 4. G Dominant 7
        engineFacade.addChord(withRoot: 7, quality: 4, tickOffset: 5760)

        chordsInArrangement = 4

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
