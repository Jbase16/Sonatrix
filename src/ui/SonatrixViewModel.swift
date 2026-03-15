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
    public enum PatternCategory: String, CaseIterable, Identifiable, Codable {
        case strum
        case picking

        public var id: String { rawValue }

        public var displayName: String {
            switch self {
            case .strum:
                return "Strum"
            case .picking:
                return "Picking"
            }
        }
    }

    public struct PatternDescriptor: Identifiable, Hashable, Codable {
        public let id: String
        public let name: String
        public let genre: String
        public let timeSignature: String
        public let category: PatternCategory
        public let eventCount: Int
    }

    public struct ProgressionPreset: Identifiable, Hashable {
        public let id: String
        public let title: String
        public let subtitle: String
        public let chords: [ChordItem]

        public var chordSummary: String {
            chords.map(\.displayName).joined(separator: "  ")
        }
    }

    public struct RootOption: Identifiable, Hashable, Codable {
        public let id: UInt8
        public let displayName: String

        public init(id: UInt8, displayName: String) {
            self.id = id
            self.displayName = displayName
        }
    }

    public struct QualityOption: Identifiable, Hashable, Codable {
        public let id: UInt8
        public let storedName: String
        public let displayLabel: String
        public let suffix: String

        public init(id: UInt8, storedName: String, displayLabel: String, suffix: String) {
            self.id = id
            self.storedName = storedName
            self.displayLabel = displayLabel
            self.suffix = suffix
        }
    }

    public static let ticksPerBeat: UInt16 = 960
    public static let defaultChordBeats: Int = 4
    public static let defaultPatternTemplateID: String = "acoustic_12_8_arpeggiated"
    public static let minimumChordWidth: CGFloat = 84
    public static let rootOptions: [RootOption] = [
        RootOption(id: 0, displayName: "C"),
        RootOption(id: 1, displayName: "C#"),
        RootOption(id: 2, displayName: "D"),
        RootOption(id: 3, displayName: "D#"),
        RootOption(id: 4, displayName: "E"),
        RootOption(id: 5, displayName: "F"),
        RootOption(id: 6, displayName: "F#"),
        RootOption(id: 7, displayName: "G"),
        RootOption(id: 8, displayName: "G#"),
        RootOption(id: 9, displayName: "A"),
        RootOption(id: 10, displayName: "A#"),
        RootOption(id: 11, displayName: "B")
    ]
    public static let circleOfFifthsRoots: [RootOption] = [
        RootOption(id: 0, displayName: "C"),
        RootOption(id: 7, displayName: "G"),
        RootOption(id: 2, displayName: "D"),
        RootOption(id: 9, displayName: "A"),
        RootOption(id: 4, displayName: "E"),
        RootOption(id: 11, displayName: "B"),
        RootOption(id: 6, displayName: "F#"),
        RootOption(id: 1, displayName: "Db"),
        RootOption(id: 8, displayName: "Ab"),
        RootOption(id: 3, displayName: "Eb"),
        RootOption(id: 10, displayName: "Bb"),
        RootOption(id: 5, displayName: "F")
    ]
    public static let qualityOptions: [QualityOption] = [
        QualityOption(id: 0, storedName: "maj", displayLabel: "Major", suffix: ""),
        QualityOption(id: 1, storedName: "min", displayLabel: "Minor", suffix: "m"),
        QualityOption(id: 2, storedName: "dim", displayLabel: "Diminished", suffix: "dim"),
        QualityOption(id: 3, storedName: "aug", displayLabel: "Augmented", suffix: "aug"),
        QualityOption(id: 4, storedName: "dom7", displayLabel: "Dominant 7", suffix: "7"),
        QualityOption(id: 5, storedName: "maj7", displayLabel: "Major 7", suffix: "maj7"),
        QualityOption(id: 6, storedName: "min7", displayLabel: "Minor 7", suffix: "m7"),
        QualityOption(id: 7, storedName: "hdim7", displayLabel: "Half-Diminished", suffix: "m7b5"),
        QualityOption(id: 8, storedName: "sus2", displayLabel: "Sus 2", suffix: "sus2"),
        QualityOption(id: 9, storedName: "sus4", displayLabel: "Sus 4", suffix: "sus4"),
        QualityOption(id: 10, storedName: "add9", displayLabel: "Add 9", suffix: "add9"),
        QualityOption(id: 11, storedName: "power", displayLabel: "Power", suffix: "5")
    ]
    public static let progressionPresets: [ProgressionPreset] = [
        ProgressionPreset(
            id: "singer_songwriter",
            title: "Singer-Songwriter",
            subtitle: "Open-chord acoustic lift",
            chords: [
                makeChord(rootIndex: 7, qualityIndex: 0),
                makeChord(rootIndex: 4, qualityIndex: 6),
                makeChord(rootIndex: 0, qualityIndex: 10),
                makeChord(rootIndex: 2, qualityIndex: 0)
            ]),
        ProgressionPreset(
            id: "pop_cycle",
            title: "Pop Cycle",
            subtitle: "Familiar I-V-vi-IV motion",
            chords: [
                makeChord(rootIndex: 0, qualityIndex: 0),
                makeChord(rootIndex: 7, qualityIndex: 0),
                makeChord(rootIndex: 9, qualityIndex: 1),
                makeChord(rootIndex: 5, qualityIndex: 0)
            ]),
        ProgressionPreset(
            id: "minor_lift",
            title: "Minor Lift",
            subtitle: "Melancholy verse contour",
            chords: [
                makeChord(rootIndex: 9, qualityIndex: 1),
                makeChord(rootIndex: 5, qualityIndex: 0),
                makeChord(rootIndex: 0, qualityIndex: 0),
                makeChord(rootIndex: 7, qualityIndex: 0)
            ])
    ]

    // The bridged Objective-C++ Engine orchestrator
    private var engineFacade: SonatrixEngineFacade

    // Expose basic UI state
    @Published public var isPlaying: Bool = false
    @Published public var isCompiling: Bool = false
    @Published public private(set) var availablePatterns: [PatternDescriptor] = []
    @Published public private(set) var selectedPatternID: String = SonatrixViewModel.defaultPatternTemplateID

    // -----------------------------------------------------------------------------
    // Shared Data Models
    // -----------------------------------------------------------------------------
    public struct ChordItem: Identifiable, Equatable, Hashable, Codable {
        public var id = UUID()
        public var rootName: String
        public var qualityName: String
        // For C++ interop
        public var rootIndex: UInt8
        public var qualityIndex: UInt8
        public var durationTicks: UInt16 = UInt16(SonatrixViewModel.defaultChordBeats) * SonatrixViewModel.ticksPerBeat

        public var durationBeats: Int {
            max(1, Int(durationTicks) / Int(SonatrixViewModel.ticksPerBeat))
        }

        public var displayName: String {
            let quality = SonatrixViewModel.qualityOption(for: qualityIndex)
            return rootName + quality.suffix
        }
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
        public let selectedPatternID: String?
    }

    public init() {
        self.engineFacade = SonatrixEngineFacade()
        self.availablePatterns = Self.loadPatternCatalog()

        let initialPatternID = Self.resolvedPatternID(
            requestedID: Self.defaultPatternTemplateID,
            availablePatterns: availablePatterns)
        self.selectedPatternID = initialPatternID
        self.engineFacade.setPatternTemplateId(initialPatternID)
    }

    private func bundledPlaybackKitPath() throws -> String {
        guard let resourcePath = Bundle.main.resourcePath else {
            throw NSError(domain: "Sonatrix.AssetsNotFound", code: 1, userInfo: nil)
        }

        let candidatePaths = [
            resourcePath + "/Assets/Exciters/FS_Guitars",
            resourcePath + "/Assets/samples/bass_mock"
        ]

        if let kitPath = candidatePaths.first(where: { FileManager.default.fileExists(atPath: $0) }) {
            return kitPath
        }

        throw NSError(
            domain: "Sonatrix.AssetsNotFound",
            code: 1,
            userInfo: [NSLocalizedDescriptionKey: "Missing bundled playback kit in Assets/Exciters/FS_Guitars or Assets/samples/bass_mock"])
    }

    public static func rootOption(for rootIndex: UInt8) -> RootOption {
        rootOptions.first(where: { $0.id == rootIndex })
            ?? circleOfFifthsRoots.first(where: { $0.id == rootIndex })
            ?? rootOptions[0]
    }

    public static func qualityOption(for qualityIndex: UInt8) -> QualityOption {
        qualityOptions.first(where: { $0.id == qualityIndex }) ?? qualityOptions[0]
    }

    public static func makeChord(rootIndex: UInt8,
                                 qualityIndex: UInt8,
                                 durationBeats: Int = defaultChordBeats) -> ChordItem {
        let root = rootOption(for: rootIndex)
        let quality = qualityOption(for: qualityIndex)
        let clampedBeats = max(1, min(durationBeats, 32))
        let durationTicks = UInt16(clampedBeats * Int(ticksPerBeat))
        return ChordItem(
            rootName: root.displayName,
            qualityName: quality.storedName,
            rootIndex: root.id,
            qualityIndex: quality.id,
            durationTicks: durationTicks)
    }

    public static func defaultChord() -> ChordItem {
        makeChord(rootIndex: 0, qualityIndex: 0)
    }

    public var selectedPattern: PatternDescriptor? {
        availablePatterns.first(where: { $0.id == selectedPatternID })
    }

    public var patternGenres: [String] {
        Array(Set(availablePatterns.map(\.genre))).sorted()
    }

    // MARK: - Project File API

    public func saveProject(to url: URL) throws {
        let state = ProjectState(
            chords: arrangementChords,
            busVolumes: busVolumes,
            selectedPatternID: selectedPatternID)
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
            self.arrangementChords = state.chords.map { self.normalized($0) }
            let restoredPatternID = Self.resolvedPatternID(
                requestedID: state.selectedPatternID,
                availablePatterns: self.availablePatterns)
            self.selectedPatternID = restoredPatternID
            self.engineFacade.setPatternTemplateId(restoredPatternID)
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
        let assetsPath = try bundledPlaybackKitPath()

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
        } else if !arrangementChords.isEmpty {
            compileArrangement()
            engineFacade.play()
            isPlaying = true
        } else {
            isPlaying = false
        }
    }

    // MARK: - Mixer API

    public func setVolume(bus: Int, volume: Float) {
        if bus >= 0 && bus < busVolumes.count {
            busVolumes[bus] = volume
            engineFacade.setVolume(volume, forBus: UInt8(bus))
        }
    }

    public func selectPattern(id: String) {
        let resolvedID = Self.resolvedPatternID(
            requestedID: id,
            availablePatterns: availablePatterns)

        if resolvedID == selectedPatternID {
            return
        }

        selectedPatternID = resolvedID
        engineFacade.setPatternTemplateId(resolvedID)

        if !arrangementChords.isEmpty {
            compileArrangement()
        }
    }

    public func applyProgressionPreset(_ preset: ProgressionPreset) {
        arrangementChords = preset.chords.map { normalized($0) }
        compileArrangement()
    }

    // MARK: - Arrangement API

    public func clearArrangement() {
        engineFacade.clearChords()
        arrangementChords.removeAll()
        if isPlaying {
            engineFacade.stop()
            isPlaying = false
        }
    }

    public func addChord(_ chord: ChordItem) {
        arrangementChords.append(normalized(chord))
        compileArrangement()
    }

    public func insertChord(_ chord: ChordItem, at index: Int) {
        let insertionIndex = max(0, min(index, arrangementChords.count))
        arrangementChords.insert(normalized(chord), at: insertionIndex)
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
            arrangementChords[index] = normalized(newChord)
            compileArrangement()
        }
    }

    public func width(for chord: ChordItem, pixelsPerBeat: CGFloat) -> CGFloat {
        max(CGFloat(chord.durationBeats) * pixelsPerBeat, Self.minimumChordWidth)
    }

    public func timelineWidth(pixelsPerBeat: CGFloat, blockSpacing: CGFloat) -> CGFloat {
        let blockWidths = arrangementChords.reduce(CGFloat.zero) { partial, chord in
            partial + width(for: chord, pixelsPerBeat: pixelsPerBeat)
        }
        let spacing = CGFloat(max(arrangementChords.count - 1, 0)) * blockSpacing
        return max(520, blockWidths + spacing + 24)
    }

    public func insertionIndex(forTimelineX timelineX: CGFloat,
                               pixelsPerBeat: CGFloat,
                               blockSpacing: CGFloat) -> Int {
        let clampedX = max(0, timelineX)
        var cursor: CGFloat = 0

        for (index, chord) in arrangementChords.enumerated() {
            let blockWidth = width(for: chord, pixelsPerBeat: pixelsPerBeat)
            let midpoint = cursor + (blockWidth / 2)
            if clampedX < midpoint {
                return index
            }
            cursor += blockWidth + blockSpacing
        }

        return arrangementChords.count
    }

    private func normalized(_ chord: ChordItem) -> ChordItem {
        Self.makeChord(
            rootIndex: chord.rootIndex,
            qualityIndex: chord.qualityIndex,
            durationBeats: chord.durationBeats)
    }

    private struct PatternLibraryDocument: Decodable {
        let templates: [PatternTemplate]
    }

    private struct PatternTemplate: Decodable {
        let id: String
        let name: String
        let genre: String
        let timeSignature: String
        let patterns: [String: PatternEngine]
    }

    private struct PatternEngine: Decodable {
        let events: [PatternEvent]
    }

    private struct PatternEvent: Decodable {
        let type: String?
    }

    private static func loadPatternCatalog() -> [PatternDescriptor] {
        guard let libraryURL = resolvePatternLibraryURL(),
              let data = try? Data(contentsOf: libraryURL),
              let document = try? JSONDecoder().decode(PatternLibraryDocument.self, from: data)
        else {
            return [
                PatternDescriptor(
                    id: defaultPatternTemplateID,
                    name: "Acoustic 12/8 Arpeggiated",
                    genre: "Acoustic Pop",
                    timeSignature: "4/4",
                    category: .picking,
                    eventCount: 12)
            ]
        }

        let guitarTemplates = document.templates.compactMap { template -> PatternDescriptor? in
            guard let guitarPattern = template.patterns["Guitar"] else {
                return nil
            }

            return PatternDescriptor(
                id: template.id,
                name: template.name,
                genre: template.genre,
                timeSignature: template.timeSignature,
                category: classifyPattern(events: guitarPattern.events),
                eventCount: guitarPattern.events.count)
        }

        return guitarTemplates.isEmpty ? [
            PatternDescriptor(
                id: defaultPatternTemplateID,
                name: "Acoustic 12/8 Arpeggiated",
                genre: "Acoustic Pop",
                timeSignature: "4/4",
                category: .picking,
                eventCount: 12)
        ] : guitarTemplates
    }

    private static func classifyPattern(events: [PatternEvent]) -> PatternCategory {
        let eventTypes = Set(events.compactMap(\.type))
        if eventTypes.contains("GuitarPluck") || eventTypes.contains("GuitarPinch") {
            return .picking
        }
        return .strum
    }

    private static func resolvePatternLibraryURL() -> URL? {
        let fileManager = FileManager.default

        if let resourcePath = Bundle.main.resourcePath {
            let bundledURL = URL(fileURLWithPath: resourcePath)
                .appendingPathComponent("Assets/Patterns/default_library.json")
            if fileManager.fileExists(atPath: bundledURL.path) {
                return bundledURL
            }
        }

        let repoURL = URL(fileURLWithPath: "/Users/jason/Developer/Sonatrix/assets/Patterns/default_library.json")
        if fileManager.fileExists(atPath: repoURL.path) {
            return repoURL
        }

        return nil
    }

    private static func resolvedPatternID(requestedID: String?,
                                          availablePatterns: [PatternDescriptor]) -> String {
        if let requestedID,
           availablePatterns.contains(where: { $0.id == requestedID }) {
            return requestedID
        }

        if availablePatterns.contains(where: { $0.id == defaultPatternTemplateID }) {
            return defaultPatternTemplateID
        }

        return availablePatterns.first?.id ?? defaultPatternTemplateID
    }

    private func compileArrangement() {
        isCompiling = true
        defer { isCompiling = false }

        engineFacade.clearChords()

        if arrangementChords.isEmpty {
            if isPlaying {
                engineFacade.stop()
                isPlaying = false
            }
            return
        }

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
    }
}
