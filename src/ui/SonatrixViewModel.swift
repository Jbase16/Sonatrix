import Combine
import SwiftUI

#if !STANDALONE
private final class SonatrixEngineFacade {
    private(set) var isPlaying: Bool = false
    private(set) var currentPlayheadTick: Double = 0
    private(set) var tempoBPM: Double = 120.0
    private(set) var arrangementLoopEnabled: Bool = false

    init() {}

    func play() {
        isPlaying = true
    }

    func play(fromTick tickOffset: Double) {
        currentPlayheadTick = max(0.0, tickOffset)
        isPlaying = true
    }

    func stop() {
        isPlaying = false
    }

    func seek(toTick tickOffset: Double) {
        currentPlayheadTick = max(0.0, tickOffset)
    }

    func setTempoBPM(_ bpm: Double) {
        tempoBPM = bpm
    }

    func setArrangementLoopEnabled(_ enabled: Bool) {
        arrangementLoopEnabled = enabled
    }

    func setArrangementLengthTicks(_ lengthTicks: Double) {}

    func clearChords() {}

    func addChord(withRoot rootKey: UInt8,
                  quality: UInt8,
                  tickOffset offset: Double) {}

    func addChord(withRoot rootKey: UInt8,
                  quality: UInt8,
                  tickOffset offset: Double,
                  guitarFrets: [NSNumber]?,
                  noteOrder: [NSNumber]?,
                  noteVelocities: [NSNumber]?) {}

    func compileAndSchedule() {}

    func previewChord(withRoot rootKey: UInt8,
                      quality: UInt8,
                      durationTicks: Double,
                      guitarFrets: [NSNumber]?,
                      noteOrder: [NSNumber]?,
                      noteVelocities: [NSNumber]?,
                      shouldLoop: Bool) {
        isPlaying = shouldLoop
    }

    func setPatternTemplateId(_ patternTemplateId: String) {}

    func setVolume(_ volume: Float, forBus busIndex: UInt8) {}

    func bounceAudio(toPath path: String,
                     assetsPath: String,
                     volumes: [NSNumber]) -> Bool {
        false
    }

    func exportMIDI(toPath path: String) -> Bool {
        false
    }

    func suggestGuitarFrets(forRoot rootKey: UInt8,
                            quality: UInt8) -> [NSNumber] {
        Array(repeating: NSNumber(value: -1), count: 6)
    }
}
#endif

// -----------------------------------------------------------------------------
// SonatrixViewModel
//
// The central @Observable state container for the SwiftUI layer.
// It safely wraps the Objective-C++ `SonatrixEngineFacade` and exposes
// transport controls and arrangement mutations to the UI cleanly.
// -----------------------------------------------------------------------------

public class SonatrixViewModel: ObservableObject {
    public struct ChordStringNote: Identifiable, Equatable, Hashable, Codable {
        public var stringIndex: Int
        public var fret: Int
        public var velocity: UInt8
        public var order: Int

        public var id: Int { stringIndex }
    }

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
    public static let defaultTempoBPM: Double = 120.0
    public static let minimumChordWidth: CGFloat = 84
    public static let standardTuningMIDINotes: [Int] = [40, 45, 50, 55, 59, 64]
    public static let stringNames: [String] = ["Low E", "A", "D", "G", "B", "High E"]
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
    private var playheadTimer: Timer?
    private var lastPlayheadUpdate: Date?

    // Expose basic UI state
    @Published public var isPlaying: Bool = false
    @Published public var isCompiling: Bool = false
    @Published public private(set) var availablePatterns: [PatternDescriptor] = []
    @Published public private(set) var selectedPatternID: String = SonatrixViewModel.defaultPatternTemplateID
    @Published public var tempoBPM: Double = SonatrixViewModel.defaultTempoBPM
    @Published public var playheadTick: Double = 0
    @Published public var isLoopEnabled: Bool = false {
        didSet {
            engineFacade.setArrangementLoopEnabled(isLoopEnabled)
        }
    }
    @Published public private(set) var isPreviewingChord: Bool = false
    @Published public private(set) var isLoopPreviewingChord: Bool = false

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
        public var guitarNotes: [ChordStringNote]? = nil

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
        public let tempoBPM: Double?
        public let isLoopEnabled: Bool?
    }

    public init() {
        self.engineFacade = SonatrixEngineFacade()
        self.availablePatterns = Self.loadPatternCatalog()

        let initialPatternID = Self.resolvedPatternID(
            requestedID: Self.defaultPatternTemplateID,
            availablePatterns: availablePatterns)
        self.selectedPatternID = initialPatternID
        self.engineFacade.setPatternTemplateId(initialPatternID)
        self.engineFacade.setTempoBPM(Self.defaultTempoBPM)
        self.engineFacade.setArrangementLoopEnabled(false)
        self.tempoBPM = Self.defaultTempoBPM
        self.playheadTick = 0
    }

    deinit {
        stopPlayheadTimer()
        stopChordPreview()
        engineFacade.stop()
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
            durationTicks: durationTicks,
            guitarNotes: nil)
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

    public var arrangementDurationTicks: Double {
        arrangementChords.reduce(0) { partial, chord in
            partial + Double(chord.durationTicks)
        }
    }

    public var arrangementDurationBeats: Double {
        arrangementDurationTicks / Double(Self.ticksPerBeat)
    }

    // MARK: - Project File API

    public func saveProject(to url: URL) throws {
        let state = ProjectState(
            chords: arrangementChords,
            busVolumes: busVolumes,
            selectedPatternID: selectedPatternID,
            tempoBPM: tempoBPM,
            isLoopEnabled: isLoopEnabled)
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
            let restoredTempo = state.tempoBPM ?? Self.defaultTempoBPM
            self.setTempo(restoredTempo)
            self.isLoopEnabled = state.isLoopEnabled ?? false
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
            stopPlayback()
        } else if !arrangementChords.isEmpty {
            playFromCurrentPlayhead()
        } else {
            isPlaying = false
        }
    }

    public func playFromCurrentPlayhead() {
        guard !arrangementChords.isEmpty else {
            return
        }

        compileArrangement()
        startPlaybackFromScheduledArrangement(at: playheadTick)
    }

    public func stopPlayback() {
        isPreviewingChord = false
        isLoopPreviewingChord = false
        engineFacade.stop()
        isPlaying = false
        stopPlayheadTimer()
    }

    public func restartPlayback() {
        guard !arrangementChords.isEmpty else {
            return
        }

        playheadTick = 0
        startPlaybackFromScheduledArrangement(at: 0)
    }

    public func previewChord(_ chord: ChordItem,
                             notes: [ChordStringNote],
                             looped: Bool) {
        let shouldReuseLoopPreview = looped && isLoopPreviewingChord
        if !shouldReuseLoopPreview {
            stopPlayback()
        }

        let normalizedNotes = normalizedGuitarNotes(notes)
        let guitarFrets = normalizedNotes.map { NSNumber(value: $0.fret) }
        let noteOrder = normalizedNotes.map { NSNumber(value: $0.order) }
        let noteVelocities = normalizedNotes.map { NSNumber(value: Int($0.velocity)) }

        engineFacade.previewChord(
            withRoot: chord.rootIndex,
            quality: chord.qualityIndex,
            durationTicks: Double(chord.durationTicks),
            guitarFrets: guitarFrets,
            noteOrder: noteOrder,
            noteVelocities: noteVelocities,
            shouldLoop: looped)
        isPreviewingChord = true
        isLoopPreviewingChord = looped
    }

    public func stopChordPreview() {
        if isPreviewingChord {
            engineFacade.stop()
        }
        isPreviewingChord = false
        isLoopPreviewingChord = false
    }

    public func setTempo(_ bpm: Double) {
        let clampedTempo = min(max(bpm, 40.0), 240.0)
        tempoBPM = clampedTempo
        engineFacade.setTempoBPM(clampedTempo)
        lastPlayheadUpdate = Date()
    }

    public func seekPlayhead(to tick: Double) {
        let clampedTick = min(max(0.0, tick), arrangementDurationTicks)
        playheadTick = clampedTick
        engineFacade.seek(toTick: clampedTick)
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
        playheadTick = 0
        stopPlayheadTimer()
        if isPlaying {
            stopPlayback()
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

    public func updateChordNotes(at index: Int, notes: [ChordStringNote]) {
        guard index >= 0 && index < arrangementChords.count else {
            return
        }

        var chord = arrangementChords[index]
        chord.guitarNotes = normalizedGuitarNotes(notes)
        arrangementChords[index] = chord
        compileArrangement()
    }

    public func editableGuitarNotes(for chord: ChordItem) -> [ChordStringNote] {
        if let guitarNotes = chord.guitarNotes, !guitarNotes.isEmpty {
            return normalizedGuitarNotes(guitarNotes)
        }

        let suggestedFrets = engineFacade.suggestGuitarFrets(
            forRoot: chord.rootIndex,
            quality: chord.qualityIndex)
        let notes = suggestedFrets.enumerated().map { index, fretNumber in
            ChordStringNote(
                stringIndex: index,
                fret: fretNumber.intValue,
                velocity: 100,
                order: index)
        }
        return normalizedGuitarNotes(notes)
    }

    public func timelineSectionFrames(pixelsPerBeat: CGFloat,
                                      blockSpacing: CGFloat) -> [(index: Int, x: CGFloat, width: CGFloat)] {
        var frames: [(index: Int, x: CGFloat, width: CGFloat)] = []
        var cursor: CGFloat = 0

        for (index, chord) in arrangementChords.enumerated() {
            let blockWidth = width(for: chord, pixelsPerBeat: pixelsPerBeat)
            frames.append((index: index, x: cursor, width: blockWidth))
            cursor += blockWidth + blockSpacing
        }

        return frames
    }

    public func tick(forTimelineX timelineX: CGFloat,
                     pixelsPerBeat: CGFloat,
                     blockSpacing: CGFloat) -> Double {
        let frames = timelineSectionFrames(pixelsPerBeat: pixelsPerBeat, blockSpacing: blockSpacing)
        let clampedX = max(0, timelineX)

        for frame in frames {
            let frameStart = frame.x
            let frameEnd = frame.x + frame.width
            if clampedX <= frameEnd {
                let chord = arrangementChords[frame.index]
                let localRatio = frame.width > 0 ? min(max((clampedX - frameStart) / frame.width, 0), 1) : 0
                let ticksBeforeChord = arrangementChords.prefix(frame.index).reduce(0.0) {
                    $0 + Double($1.durationTicks)
                }
                return ticksBeforeChord + (Double(chord.durationTicks) * Double(localRatio))
            }
        }

        return arrangementDurationTicks
    }

    public func playheadX(pixelsPerBeat: CGFloat,
                          blockSpacing: CGFloat) -> CGFloat {
        let frames = timelineSectionFrames(pixelsPerBeat: pixelsPerBeat, blockSpacing: blockSpacing)
        var ticksRemaining = playheadTick

        for frame in frames {
            let chordTicks = Double(arrangementChords[frame.index].durationTicks)
            if ticksRemaining <= chordTicks {
                let progress = chordTicks > 0 ? ticksRemaining / chordTicks : 0
                return frame.x + (frame.width * CGFloat(progress))
            }
            ticksRemaining -= chordTicks
        }

        return frames.last.map { $0.x + $0.width } ?? 0
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
        var normalizedChord = Self.makeChord(
            rootIndex: chord.rootIndex,
            qualityIndex: chord.qualityIndex,
            durationBeats: chord.durationBeats)
        normalizedChord.id = chord.id
        normalizedChord.guitarNotes = chord.guitarNotes.map { normalizedGuitarNotes($0) }
        return normalizedChord
    }

    private func normalizedGuitarNotes(_ notes: [ChordStringNote]) -> [ChordStringNote] {
        let byOrder = notes
            .sorted { lhs, rhs in
                if lhs.order == rhs.order {
                    return lhs.stringIndex < rhs.stringIndex
                }
                return lhs.order < rhs.order
            }

        return byOrder.enumerated().map { index, note in
            ChordStringNote(
                stringIndex: min(max(note.stringIndex, 0), 5),
                fret: min(max(note.fret, -1), 24),
                velocity: UInt8(min(max(Int(note.velocity), 1), 127)),
                order: index)
        }
    }

    public func noteDisplayName(for note: ChordStringNote) -> String {
        if note.fret < 0 {
            return "Muted"
        }

        let midi = Self.standardTuningMIDINotes[note.stringIndex] + note.fret
        let noteNames = ["C", "C#", "D", "D#", "E", "F",
                         "F#", "G", "G#", "A", "A#", "B"]
        let noteName = noteNames[midi % 12]
        let octave = (midi / 12) - 1
        return "\(noteName)\(octave)"
    }

    public func stringName(for stringIndex: Int) -> String {
        guard stringIndex >= 0 && stringIndex < Self.stringNames.count else {
            return "String \(stringIndex + 1)"
        }
        return Self.stringNames[stringIndex]
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
            return fallbackPatternCatalog()
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

        guard !guitarTemplates.isEmpty else {
            return fallbackPatternCatalog()
        }

        return guitarTemplates.sorted { lhs, rhs in
            if lhs.category != rhs.category {
                return lhs.category == .strum
            }
            if lhs.genre != rhs.genre {
                return lhs.genre < rhs.genre
            }
            return lhs.name < rhs.name
        }
    }

    private static func classifyPattern(events: [PatternEvent]) -> PatternCategory {
        let eventTypes = Set(events.compactMap(\.type))
        if eventTypes.contains("GuitarPluck") || eventTypes.contains("GuitarPinch") {
            return .picking
        }
        return .strum
    }

    private static func fallbackPatternCatalog() -> [PatternDescriptor] {
        [
            PatternDescriptor(
                id: "acoustic_basic_8ths",
                name: "Acoustic Basic 8ths (D DU UDU)",
                genre: "Acoustic Pop",
                timeSignature: "4/4",
                category: .strum,
                eventCount: 6),
            PatternDescriptor(
                id: "acoustic_island_soft",
                name: "Acoustic Island Soft (DDUUDU)",
                genre: "Acoustic Pop",
                timeSignature: "4/4",
                category: .strum,
                eventCount: 6),
            PatternDescriptor(
                id: "acoustic_singer_songwriter_1",
                name: "Acoustic Singer-Songwriter 1",
                genre: "Acoustic Pop",
                timeSignature: "4/4",
                category: .strum,
                eventCount: 5),
            PatternDescriptor(
                id: defaultPatternTemplateID,
                name: "12/8 Arpeggiated Strum",
                genre: "Acoustic Pop",
                timeSignature: "4/4",
                category: .picking,
                eventCount: 12)
        ]
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
        if let requestedID = requestedID,
           availablePatterns.contains(where: { $0.id == requestedID }) {
            return requestedID
        }

        if availablePatterns.contains(where: { $0.id == defaultPatternTemplateID }) {
            return defaultPatternTemplateID
        }

        return availablePatterns.first?.id ?? defaultPatternTemplateID
    }

    private func startPlayheadTimer() {
        stopPlayheadTimer()
        lastPlayheadUpdate = Date()

        playheadTimer = Timer.scheduledTimer(withTimeInterval: 1.0 / 30.0, repeats: true) { [weak self] _ in
            self?.advancePlayhead()
        }
        if let playheadTimer = playheadTimer {
            RunLoop.main.add(playheadTimer, forMode: .commonModes)
        }
    }

    private func stopPlayheadTimer() {
        playheadTimer?.invalidate()
        playheadTimer = nil
        lastPlayheadUpdate = nil
    }

    private func advancePlayhead() {
        guard isPlaying else {
            stopPlayheadTimer()
            return
        }

        let now = Date()
        let lastDate = lastPlayheadUpdate ?? now
        lastPlayheadUpdate = now
        let deltaSeconds = now.timeIntervalSince(lastDate)
        let ticksPerSecond = (tempoBPM / 60.0) * Double(Self.ticksPerBeat)

        playheadTick = min(arrangementDurationTicks,
                           playheadTick + (deltaSeconds * ticksPerSecond))

        if playheadTick >= arrangementDurationTicks {
            if isLoopEnabled && !arrangementChords.isEmpty {
                playheadTick.formTruncatingRemainder(dividingBy: arrangementDurationTicks)
            } else {
                stopPlayback()
            }
        }
    }

    private func startPlaybackFromScheduledArrangement(at tick: Double) {
        let clampedTick = min(max(0.0, tick), arrangementDurationTicks)
        stopChordPreview()
        engineFacade.stop()
        playheadTick = clampedTick
        engineFacade.play(fromTick: clampedTick)
        isPlaying = true
        startPlayheadTimer()
    }

    private func compileArrangement() {
        isCompiling = true
        defer { isCompiling = false }

        engineFacade.clearChords()

        if arrangementChords.isEmpty {
            if isPlaying {
                stopPlayback()
            }
            return
        }

        // Push chords to C++ based on the dynamic SwiftUI array
        var currentTick: UInt64 = 0
        for chord in arrangementChords {
            let guitarNotes = chord.guitarNotes.map { normalizedGuitarNotes($0) } ?? []
            let noteCount = guitarNotes.count

            let guitarFrets = noteCount == 6 ? guitarNotes.map { NSNumber(value: $0.fret) } : nil
            let noteOrder = noteCount == 6 ? guitarNotes.map { NSNumber(value: $0.order) } : nil
            let noteVelocities = noteCount == 6 ? guitarNotes.map { NSNumber(value: Int($0.velocity)) } : nil

            engineFacade.addChord(
                withRoot: chord.rootIndex,
                quality: chord.qualityIndex,
                tickOffset: Double(currentTick),
                guitarFrets: guitarFrets,
                noteOrder: noteOrder,
                noteVelocities: noteVelocities)
            currentTick += UInt64(chord.durationTicks)
        }

        // Tell the C++ layer to run the Viterbi Graph Solvers and Neural latencies
        engineFacade.setArrangementLengthTicks(arrangementDurationTicks)
        engineFacade.compileAndSchedule()
        playheadTick = min(playheadTick, arrangementDurationTicks)
        engineFacade.seek(toTick: playheadTick)
    }
}
