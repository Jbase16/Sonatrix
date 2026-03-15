import CoreAudioKit
import SwiftUI
import UniformTypeIdentifiers

private enum ChordPaletteLayoutMode: String, CaseIterable, Identifiable {
    case circle
    case grid

    var id: String { rawValue }

    var title: String {
        switch self {
        case .circle:
            return "Circle"
        case .grid:
            return "Grid"
        }
    }
}

private struct DragChordPayload: Codable, Hashable {
    let rootName: String
    let qualityName: String
    let rootIndex: UInt8
    let qualityIndex: UInt8
    let durationBeats: Int

    var displayName: String {
        let quality = SonatrixViewModel.qualityOption(for: qualityIndex)
        return rootName + quality.suffix
    }

    func makeChordItem() -> SonatrixViewModel.ChordItem {
        SonatrixViewModel.makeChord(
            rootIndex: rootIndex,
            qualityIndex: qualityIndex,
            durationBeats: durationBeats)
    }
}

private struct FavoriteChordPreset: Identifiable {
    let id: String
    let rootIndex: UInt8
    let qualityIndex: UInt8
}

private extension UTType {
    static let sonatrixChord = UTType(exportedAs: "com.sonatrix.chord")
}

private func makeChordItemProvider(for payload: DragChordPayload) -> NSItemProvider {
    let provider = NSItemProvider()
    let encoder = JSONEncoder()

    if let data = try? encoder.encode(payload) {
        provider.registerDataRepresentation(forTypeIdentifier: UTType.sonatrixChord.identifier,
                                            visibility: .all) { completion in
            completion(data, nil)
            return nil
        }
    }

    provider.suggestedName = payload.displayName
    return provider
}

// -----------------------------------------------------------------------------
// Sonatrix Arrangement View
//
// The main SwiftUI view for the AUv3 AudioExtension. Hosted within a generic
// AUViewController wrapper to expose it to Logic Pro.
// -----------------------------------------------------------------------------

public struct ArrangementView: View {
    private let transportHeight: CGFloat = 60
    private let chordTrackHeight: CGFloat = 118
    private let mixerHeight: CGFloat = 220

    #if STANDALONE
        @ObservedObject var viewModel: SonatrixViewModel

        public init(viewModel: SonatrixViewModel) {
            self.viewModel = viewModel
        }
    #else
        public init() {}
    #endif

    public var body: some View {
        GeometryReader { geometry in
            let middleHeight = max(0, geometry.size.height - transportHeight - chordTrackHeight - mixerHeight)

            VStack(spacing: 0) {
                #if STANDALONE
                    TransportRibbon(viewModel: viewModel)
                        .frame(height: transportHeight)
                        .background(Color.black)
                #else
                    TransportRibbon()
                        .frame(height: transportHeight)
                        .background(Color.black)
                #endif

                #if STANDALONE
                    ChordTrackRuler(viewModel: viewModel)
                        .frame(height: chordTrackHeight)
                        .background(Color(white: 0.15))
                #else
                    ChordTrackRuler()
                        .frame(height: chordTrackHeight)
                        .background(Color(white: 0.15))
                #endif

                HStack(spacing: 0) {
                    #if STANDALONE
                        ScrollView(.vertical, showsIndicators: true) {
                            ChordPaletteView(viewModel: viewModel)
                                .frame(width: 320)
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
                            .frame(width: 320, height: middleHeight)
                            .background(
                                LinearGradient(
                                    colors: [Color(red: 0.11, green: 0.12, blue: 0.15),
                                             Color(red: 0.08, green: 0.08, blue: 0.1)],
                                    startPoint: .top,
                                    endPoint: .bottom)
                            )
                            .border(Color.white.opacity(0.08), width: 1)
                    #else
                        ScrollView(.vertical, showsIndicators: true) {
                            ChordPaletteView()
                                .frame(width: 320)
                                .frame(maxWidth: .infinity, alignment: .leading)
                        }
                            .frame(width: 320, height: middleHeight)
                            .background(Color(white: 0.1))
                            .border(Color.white.opacity(0.08), width: 1)
                    #endif

                    #if STANDALONE
                        PatternBrowserView(viewModel: viewModel)
                            .frame(maxWidth: .infinity,
                                   minHeight: middleHeight,
                                   maxHeight: middleHeight,
                                   alignment: .topLeading)
                            .border(Color.white.opacity(0.08), width: 1)
                    #else
                        PatternBrowserView()
                            .frame(maxWidth: .infinity,
                                   minHeight: middleHeight,
                                   maxHeight: middleHeight,
                                   alignment: .topLeading)
                    #endif
                }
                .frame(maxWidth: .infinity,
                       minHeight: middleHeight,
                       maxHeight: middleHeight,
                       alignment: .topLeading)

                #if STANDALONE
                    MixerView(viewModel: viewModel)
                        .frame(height: mixerHeight)
                        .background(Color.black)
                        .clipped()
                #else
                    Text("Mixer Not Available in AUv3 Mock")
                        .frame(height: mixerHeight)
                        .background(Color.black)
                #endif
            }
            .frame(width: geometry.size.width, height: geometry.size.height, alignment: .top)
            .background(Color.black)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .preferredColorScheme(.dark)
    }
}

// MARK: - Subcomponents

struct TransportRibbon: View {
    #if STANDALONE
        @ObservedObject var viewModel: SonatrixViewModel
    #endif

    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 2) {
                Text("SONATRIX")
                    .font(.headline)
                    .fontWeight(.black)
                    .foregroundColor(.white)

                #if STANDALONE
                    Text(viewModel.selectedPattern?.name ?? "Acoustic 12/8 Arpeggiated")
                        .font(.caption)
                        .foregroundColor(.gray)
                #endif
            }
            .padding()

            Spacer()

            #if STANDALONE
                Button(action: {
                    viewModel.restartPlayback()
                }) {
                    Image(systemName: "backward.end.fill")
                        .font(.title3)
                        .foregroundColor(.white)
                }.buttonStyle(PlainButtonStyle())

                Button(action: {
                    viewModel.togglePlayback()
                }) {
                    Image(systemName: viewModel.isPlaying ? "pause.fill" : "play.fill")
                        .font(.title)
                        .foregroundColor(viewModel.isPlaying ? .green : .white)
                }.buttonStyle(PlainButtonStyle())

                Button(action: {
                    viewModel.isLoopEnabled.toggle()
                }) {
                    Image(systemName: "repeat")
                        .font(.title3)
                        .foregroundColor(viewModel.isLoopEnabled ? .orange : .white)
                }.buttonStyle(PlainButtonStyle())

                Image(systemName: "square.and.arrow.down.fill")
                    .foregroundColor(.white)
                    .padding(8)
                    .background(Color.blue.opacity(0.8))
                    .cornerRadius(6)
                    .contextMenu {
                        Text("Drag this icon to export MIDI")
                    }
                    .onDrag {
                        let tempURL = FileManager.default.temporaryDirectory.appendingPathComponent(
                            "SonatrixArrangement.mid")
                        do {
                            try viewModel.exportMIDI(to: tempURL)
                            return NSItemProvider(
                                item: tempURL as NSSecureCoding,
                                typeIdentifier: UTType.midi.identifier)
                        } catch {
                            print("Failed to export MIDI for drag: \(error)")
                            return NSItemProvider()
                        }
                    }

                VStack(alignment: .leading, spacing: 4) {
                    Text("Tempo")
                        .font(.caption2)
                        .foregroundColor(.gray)

                    HStack(spacing: 10) {
                        Slider(
                            value: Binding(
                                get: { viewModel.tempoBPM },
                                set: { viewModel.setTempo($0) }
                            ),
                            in: 40...240,
                            step: 1
                        )
                        .frame(width: 150)

                        Text("\(Int(viewModel.tempoBPM)) BPM")
                            .font(.caption)
                            .foregroundColor(.white)
                            .frame(width: 62, alignment: .leading)
                    }
                }
            #endif

            Spacer()

            #if STANDALONE
                if let selectedPattern = viewModel.selectedPattern {
                    Text("\(selectedPattern.category.displayName) • \(selectedPattern.timeSignature)")
                        .font(.caption)
                        .padding(.horizontal, 10)
                        .padding(.vertical, 6)
                        .background(Color.orange.opacity(0.18))
                        .cornerRadius(8)
                }
            #endif

            Text("SYNC: HOST").font(.caption).padding().background(Color.green.opacity(0.3))
                .cornerRadius(4)
        }
        .padding(.horizontal)
    }
}

struct ChordTrackRuler: View {
    #if STANDALONE
        private struct ChordEditSession: Identifiable {
            let id: UUID
            let index: Int
            let chord: SonatrixViewModel.ChordItem

            init(index: Int, chord: SonatrixViewModel.ChordItem) {
                self.id = chord.id
                self.index = index
                self.chord = chord
            }
        }

        @ObservedObject var viewModel: SonatrixViewModel
        @State private var editingSession: ChordEditSession?
        @State private var isDropTargeted = false
        @State private var resumePlaybackAfterPlayheadDrag = false
    #endif

    private let pixelsPerBeat: CGFloat = 30
    private let blockSpacing: CGFloat = 6
    private let horizontalInset: CGFloat = 12
    private let trackLaneHeight: CGFloat = 68

    var body: some View {
        VStack(spacing: 8) {
            HStack {
                Text("CHORD TRACK").font(.caption).foregroundColor(.gray)

                #if STANDALONE
                    Button(action: {
                        viewModel.addChord(SonatrixViewModel.defaultChord())
                    }) {
                        Image(systemName: "plus.circle.fill")
                            .foregroundColor(.green)
                    }
                    .buttonStyle(PlainButtonStyle())
                #endif

                Spacer()

                #if STANDALONE
                    Text("Drag from the palette or tap a chord to append")
                        .font(.caption2)
                        .foregroundColor(.gray)
                #endif

                Spacer()

                #if STANDALONE
                    Button("Clear") {
                        viewModel.clearArrangement()
                    }
                    .font(.caption)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 5)
                    .background(Color.red.opacity(0.22))
                    .cornerRadius(6)
                    .buttonStyle(PlainButtonStyle())
                #endif
            }

            ScrollView(.horizontal, showsIndicators: false) {
                #if STANDALONE
                    let timelineWidth = viewModel.timelineWidth(
                        pixelsPerBeat: pixelsPerBeat,
                        blockSpacing: blockSpacing)
                    let sectionFrames = viewModel.timelineSectionFrames(
                        pixelsPerBeat: pixelsPerBeat,
                        blockSpacing: blockSpacing)

                    ZStack(alignment: .leading) {
                        RoundedRectangle(cornerRadius: 10)
                            .fill(Color.white.opacity(0.04))

                        TimelineChordLane(
                            sectionFrames: sectionFrames,
                            chords: viewModel.arrangementChords,
                            sectionInset: horizontalInset,
                            height: trackLaneHeight,
                            onSelect: { index in
                                viewModel.stopPlayback()
                                let chord = viewModel.arrangementChords[index]
                                editingSession = ChordEditSession(index: index, chord: chord)
                            },
                            onRemove: { index in
                                viewModel.removeChord(at: index)
                            }
                        )
                        .clipShape(RoundedRectangle(cornerRadius: 10))

                        if viewModel.arrangementChords.isEmpty {
                            VStack(alignment: .leading, spacing: 4) {
                                Text("Drop chords here")
                                    .font(.subheadline)
                                    .foregroundColor(.white)
                                Text("The engine will recompile the current progression through AcousticOpen guitar voice leading.")
                                    .font(.caption2)
                                    .foregroundColor(.gray)
                            }
                            .padding(.horizontal, 16)
                        } else {
                            TimelinePlayhead(
                                x: horizontalInset + viewModel.playheadX(
                                    pixelsPerBeat: pixelsPerBeat,
                                    blockSpacing: blockSpacing),
                                height: trackLaneHeight
                            )
                        }
                    }
                    .frame(width: timelineWidth, height: trackLaneHeight)
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .stroke(isDropTargeted ? Color.orange : Color.white.opacity(0.08),
                                    lineWidth: isDropTargeted ? 2 : 1)
                    )
                    .onDrop(of: [UTType.sonatrixChord.identifier],
                            isTargeted: $isDropTargeted,
                            perform: { providers, location in
                                handleChordDrop(providers: providers, location: location)
                            })
                    .gesture(
                        DragGesture(minimumDistance: 0)
                            .onChanged { value in
                                updatePlayhead(location: value.location)
                            }
                            .onEnded { value in
                                finishPlayheadDrag(location: value.location)
                            }
                    )
                #else
                    HStack(spacing: blockSpacing) {
                        ChordBlock(title: "C", subtitle: "4 beats", width: 120, height: trackLaneHeight - 8, sectionIndex: 0)
                        ChordBlock(title: "Am", subtitle: "4 beats", width: 120, height: trackLaneHeight - 8, sectionIndex: 1)
                        ChordBlock(title: "F", subtitle: "4 beats", width: 120, height: trackLaneHeight - 8, sectionIndex: 2)
                        ChordBlock(title: "G", subtitle: "4 beats", width: 120, height: trackLaneHeight - 8, sectionIndex: 3)
                    }
                #endif
            }
        }
        .padding(.horizontal)
        #if STANDALONE
            .sheet(item: $editingSession) { session in
                ChordEditorSheet(
                    viewModel: viewModel,
                    chord: session.chord,
                    onSave: { updatedChord in
                        viewModel.updateChord(at: session.index, with: updatedChord)
                        editingSession = nil
                    },
                    onDelete: {
                        viewModel.removeChord(at: session.index)
                        editingSession = nil
                    },
                    onCancel: {
                        editingSession = nil
                    }
                )
            }
        #endif
    }

    #if STANDALONE
        private func handleChordDrop(providers: [NSItemProvider], location: CGPoint) -> Bool {
            guard let provider = providers.first(where: {
                $0.hasItemConformingToTypeIdentifier(UTType.sonatrixChord.identifier)
            }) else {
                return false
            }

            provider.loadDataRepresentation(forTypeIdentifier: UTType.sonatrixChord.identifier) {
                data, _ in
                guard let data = data,
                      let payload = try? JSONDecoder().decode(DragChordPayload.self, from: data)
                else {
                    return
                }

                let insertionX = max(0, location.x - 12)
                let insertionIndex = viewModel.insertionIndex(
                    forTimelineX: insertionX,
                    pixelsPerBeat: pixelsPerBeat,
                    blockSpacing: blockSpacing)

                DispatchQueue.main.async {
                    viewModel.insertChord(payload.makeChordItem(), at: insertionIndex)
                }
            }

            return true
        }

        private func updatePlayhead(location: CGPoint) {
            guard !viewModel.arrangementChords.isEmpty else {
                return
            }

            if viewModel.isPlaying && !resumePlaybackAfterPlayheadDrag {
                resumePlaybackAfterPlayheadDrag = true
                viewModel.stopPlayback()
            }

            let targetTick = viewModel.tick(
                forTimelineX: max(0, location.x - horizontalInset),
                pixelsPerBeat: pixelsPerBeat,
                blockSpacing: blockSpacing)
            viewModel.seekPlayhead(to: targetTick)
        }

        private func finishPlayheadDrag(location: CGPoint) {
            updatePlayhead(location: location)

            if resumePlaybackAfterPlayheadDrag {
                resumePlaybackAfterPlayheadDrag = false
                viewModel.playFromCurrentPlayhead()
            }
        }
    #endif
}

#if STANDALONE
    struct ChordEditorSheet: View {
        @ObservedObject var viewModel: SonatrixViewModel
        let originalChord: SonatrixViewModel.ChordItem
        @State var chord: SonatrixViewModel.ChordItem
        @State private var guitarNotes: [SonatrixViewModel.ChordStringNote]
        @State private var draggedNoteID: Int?
        @State private var rememberedFrets: [Int: Int]
        @State private var previewShouldLoop: Bool = false
        var onSave: (SonatrixViewModel.ChordItem) -> Void
        var onDelete: () -> Void
        var onCancel: () -> Void

        init(viewModel: SonatrixViewModel,
             chord: SonatrixViewModel.ChordItem,
             onSave: @escaping (SonatrixViewModel.ChordItem) -> Void,
             onDelete: @escaping () -> Void,
             onCancel: @escaping () -> Void) {
            let initialNotes = viewModel.editableGuitarNotes(for: chord)
            self.viewModel = viewModel
            self.originalChord = chord
            self._chord = State(initialValue: chord)
            self._guitarNotes = State(initialValue: initialNotes)
            self._rememberedFrets = State(initialValue: Self.makeRememberedFrets(notes: initialNotes, fallbackNotes: initialNotes))
            self.onSave = onSave
            self.onDelete = onDelete
            self.onCancel = onCancel
        }

        var body: some View {
            VStack(spacing: 18) {
                Text("Edit Chord")
                    .font(.headline)

                Form {
                    Picker("Root Note", selection: $chord.rootIndex) {
                        ForEach(SonatrixViewModel.rootOptions) { option in
                            Text(option.displayName).tag(option.id)
                        }
                    }
                    .onChange(of: chord.rootIndex) { newValue in
                        chord.rootName = SonatrixViewModel.rootOption(for: newValue).displayName
                        refreshRememberedFretsForCurrentChord()
                        scheduleLoopPreviewRefreshIfNeeded()
                    }

                    Picker("Quality", selection: $chord.qualityIndex) {
                        ForEach(SonatrixViewModel.qualityOptions) { option in
                            Text(option.displayLabel).tag(option.id)
                        }
                    }
                    .onChange(of: chord.qualityIndex) { newValue in
                        chord.qualityName = SonatrixViewModel.qualityOption(for: newValue).storedName
                        refreshRememberedFretsForCurrentChord()
                        scheduleLoopPreviewRefreshIfNeeded()
                    }

                    Stepper(
                        "Duration (Beats): \(chord.durationBeats)",
                        value: Binding(
                            get: { chord.durationBeats },
                            set: {
                                chord.durationTicks = UInt16($0) * SonatrixViewModel.ticksPerBeat
                                scheduleLoopPreviewRefreshIfNeeded()
                            }
                        ),
                        in: 1...32)
                }
                .frame(width: 420, height: 170)

                HStack {
                    Text("Chord Notes")
                        .font(.headline)
                    Spacer()
                    Button(previewShouldLoop ? "Loop Preview" : "Preview") {
                        let previewChord = currentPreviewChord()
                        viewModel.previewChord(previewChord,
                                               notes: guitarNotes,
                                               looped: previewShouldLoop)
                    }
                    .buttonStyle(BorderedButtonStyle())
                    .tint(.green)
                    Button("Stop Preview") {
                        viewModel.stopChordPreview()
                    }
                    .buttonStyle(BorderedButtonStyle())
                    Button(action: {
                        previewShouldLoop.toggle()
                    }) {
                        Image(systemName: "repeat")
                            .foregroundColor(previewShouldLoop ? .orange : .white)
                    }
                    .buttonStyle(BorderedButtonStyle())
                    Button("Reset Notes") {
                        resetNotesToSuggestedShape()
                    }
                    .buttonStyle(BorderedButtonStyle())
                    Button("Revert") {
                        revertEditorState()
                    }
                    .buttonStyle(BorderedButtonStyle())
                }

                VStack(alignment: .leading, spacing: 8) {
                    Text("Drag rows to change picking order.")
                        .font(.caption)
                        .foregroundColor(.gray)

                    ScrollView(.vertical, showsIndicators: true) {
                        VStack(spacing: 8) {
                            ForEach($guitarNotes) { $note in
                                ChordNoteRow(
                                    viewModel: viewModel,
                                    note: $note,
                                    onMuteToggle: {
                                        toggleMute(for: note.stringIndex)
                                    },
                                    onFretChange: { newFret in
                                        rememberFret(newFret, for: note.stringIndex)
                                    }
                                )
                                .onDrag {
                                    draggedNoteID = note.id
                                    return NSItemProvider(object: NSString(string: "\(note.id)"))
                                }
                                .onDrop(
                                    of: [UTType.text],
                                    delegate: ChordNoteDropDelegate(
                                        targetNoteID: note.id,
                                        notes: $guitarNotes,
                                        draggedNoteID: $draggedNoteID
                                    )
                                )
                            }
                        }
                        .padding(.vertical, 4)
                    }
                    .frame(width: 520, height: 260)
                    .padding(.horizontal, 2)
                    .background(
                        RoundedRectangle(cornerRadius: 10)
                            .fill(Color.white.opacity(0.04))
                    )
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .stroke(Color.white.opacity(0.08), lineWidth: 1)
                    )
                }

                HStack {
                    Button("Delete Chord") {
                        viewModel.stopChordPreview()
                        onDelete()
                    }
                        .buttonStyle(BorderedButtonStyle())
                        .tint(.red)
                    Button("Cancel") {
                        viewModel.stopChordPreview()
                        onCancel()
                    }
                        .buttonStyle(PlainButtonStyle())
                    Spacer()
                    Button("Save") {
                        viewModel.stopChordPreview()
                        chord.guitarNotes = guitarNotes.enumerated().map { index, note in
                            var updated = note
                            updated.order = index
                            return updated
                        }
                        onSave(chord)
                    }
                        .buttonStyle(BorderedButtonStyle())
                        .tint(.blue)
                }
            }
            .padding()
            .frame(width: 620)
            .onAppear {
                viewModel.stopPlayback()
            }
            .onDisappear {
                viewModel.stopChordPreview()
            }
            .onChange(of: guitarNotes) { _ in
                scheduleLoopPreviewRefreshIfNeeded()
            }
        }

        private static func makeRememberedFrets(
            notes: [SonatrixViewModel.ChordStringNote],
            fallbackNotes: [SonatrixViewModel.ChordStringNote]
        ) -> [Int: Int] {
            var remembered: [Int: Int] = [:]

            for note in notes where note.fret >= 0 {
                remembered[note.stringIndex] = note.fret
            }

            for note in fallbackNotes where note.fret >= 0 && remembered[note.stringIndex] == nil {
                remembered[note.stringIndex] = note.fret
            }

            return remembered
        }

        private func suggestedNotes(for baseChord: SonatrixViewModel.ChordItem) -> [SonatrixViewModel.ChordStringNote] {
            var suggestedChord = baseChord
            suggestedChord.guitarNotes = nil
            return viewModel.editableGuitarNotes(for: suggestedChord)
        }

        private func resetNotesToSuggestedShape() {
            let suggested = suggestedNotes(for: chord)
            guitarNotes = suggested
            rememberedFrets = Self.makeRememberedFrets(notes: suggested, fallbackNotes: suggested)
            scheduleLoopPreviewRefreshIfNeeded()
        }

        private func revertEditorState() {
            chord = originalChord
            let restoredNotes = viewModel.editableGuitarNotes(for: originalChord)
            guitarNotes = restoredNotes
            rememberedFrets = Self.makeRememberedFrets(notes: restoredNotes, fallbackNotes: restoredNotes)
            scheduleLoopPreviewRefreshIfNeeded()
        }

        private func refreshRememberedFretsForCurrentChord() {
            let fallbackNotes = suggestedNotes(for: chord)
            rememberedFrets = Self.makeRememberedFrets(notes: guitarNotes, fallbackNotes: fallbackNotes)
        }

        private func toggleMute(for stringIndex: Int) {
            guard let noteIndex = guitarNotes.firstIndex(where: { $0.stringIndex == stringIndex }) else {
                return
            }

            if guitarNotes[noteIndex].fret >= 0 {
                rememberedFrets[stringIndex] = guitarNotes[noteIndex].fret
                guitarNotes[noteIndex].fret = -1
                return
            }

            let fallbackNotes = suggestedNotes(for: chord)
            let fallbackFret = rememberedFrets[stringIndex]
                ?? fallbackNotes.first(where: { $0.stringIndex == stringIndex })?.fret
                ?? 0
            guitarNotes[noteIndex].fret = max(0, fallbackFret)
            rememberedFrets[stringIndex] = guitarNotes[noteIndex].fret
        }

        private func rememberFret(_ fret: Int, for stringIndex: Int) {
            rememberedFrets[stringIndex] = fret
        }

        private func currentPreviewChord() -> SonatrixViewModel.ChordItem {
            var previewChord = chord
            previewChord.guitarNotes = guitarNotes.enumerated().map { index, note in
                var updated = note
                updated.order = index
                return updated
            }
            return previewChord
        }

        private func scheduleLoopPreviewRefreshIfNeeded() {
            guard previewShouldLoop, viewModel.isLoopPreviewingChord else {
                return
            }

            let previewChord = currentPreviewChord()
            DispatchQueue.main.async {
                viewModel.previewChord(previewChord,
                                       notes: guitarNotes,
                                       looped: true)
            }
        }
    }
#endif

struct ChordPaletteView: View {
    #if STANDALONE
        @ObservedObject var viewModel: SonatrixViewModel

        init(viewModel: SonatrixViewModel) {
            self.viewModel = viewModel
        }
    #else
        init() {}
    #endif

    @State private var layoutMode: ChordPaletteLayoutMode = .circle
    @State private var selectedQualityIndex: UInt8 = 0
    @State private var selectedDurationBeats: Int = SonatrixViewModel.defaultChordBeats

    private let favorites: [FavoriteChordPreset] = [
        FavoriteChordPreset(id: "G", rootIndex: 7, qualityIndex: 0),
        FavoriteChordPreset(id: "Em7", rootIndex: 4, qualityIndex: 6),
        FavoriteChordPreset(id: "Cadd9", rootIndex: 0, qualityIndex: 10),
        FavoriteChordPreset(id: "D", rootIndex: 2, qualityIndex: 0),
        FavoriteChordPreset(id: "Am", rootIndex: 9, qualityIndex: 1),
        FavoriteChordPreset(id: "F", rootIndex: 5, qualityIndex: 0)
    ]

    private var selectedQuality: SonatrixViewModel.QualityOption {
        SonatrixViewModel.qualityOption(for: selectedQualityIndex)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            VStack(alignment: .leading, spacing: 4) {
                Text("CHORD PALETTE")
                    .font(.caption)
                    .foregroundColor(.gray)
                Text("Interactive Chord Wheel")
                    .font(.title3)
                    .fontWeight(.semibold)
                    .foregroundColor(.white)
                Text("Tap to append or drag onto the chord track.")
                    .font(.caption)
                    .foregroundColor(.gray)
            }
            .padding(.top, 16)

            Picker("Layout", selection: $layoutMode) {
                ForEach(ChordPaletteLayoutMode.allCases) { mode in
                    Text(mode.title).tag(mode)
                }
            }
            .pickerStyle(.segmented)

            VStack(alignment: .leading, spacing: 8) {
                Text("Quality")
                    .font(.caption)
                    .foregroundColor(.gray)

                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        ForEach(SonatrixViewModel.qualityOptions) { option in
                            Button(action: {
                                selectedQualityIndex = option.id
                            }) {
                                Text(option.displayLabel)
                                    .font(.caption)
                                    .padding(.horizontal, 10)
                                    .padding(.vertical, 6)
                                    .background(
                                        Capsule()
                                            .fill(option.id == selectedQualityIndex
                                                  ? Color.orange.opacity(0.28)
                                                  : Color.white.opacity(0.06))
                                    )
                                    .overlay(
                                        Capsule()
                                            .stroke(option.id == selectedQualityIndex
                                                    ? Color.orange
                                                    : Color.white.opacity(0.08),
                                                    lineWidth: 1)
                                    )
                            }
                            .buttonStyle(PlainButtonStyle())
                        }
                    }
                }
            }

            VStack(alignment: .leading, spacing: 8) {
                Text("Duration")
                    .font(.caption)
                    .foregroundColor(.gray)

                HStack(spacing: 8) {
                    ForEach([2, 4, 8], id: \.self) { beats in
                        Button(action: {
                            selectedDurationBeats = beats
                        }) {
                            Text("\(beats) beats")
                                .font(.caption)
                                .padding(.horizontal, 10)
                                .padding(.vertical, 6)
                                .background(
                                    Capsule()
                                        .fill(beats == selectedDurationBeats
                                              ? Color.blue.opacity(0.28)
                                              : Color.white.opacity(0.06))
                                )
                                .overlay(
                                    Capsule()
                                        .stroke(beats == selectedDurationBeats
                                                ? Color.blue
                                                : Color.white.opacity(0.08),
                                                lineWidth: 1)
                                )
                        }
                        .buttonStyle(PlainButtonStyle())
                    }
                }
            }

            VStack(alignment: .leading, spacing: 8) {
                Text("Acoustic Quick Start")
                    .font(.caption)
                    .foregroundColor(.gray)

                LazyVGrid(columns: [GridItem(.adaptive(minimum: 84), spacing: 8)], spacing: 8) {
                    ForEach(favorites) { favorite in
                        let payload = makePayload(rootIndex: favorite.rootIndex,
                                                  qualityIndex: favorite.qualityIndex)
                        PaletteChordToken(payload: payload, compact: true, onAppend: appendChord)
                    }
                }
            }

            Group {
                if layoutMode == .circle {
                    CircleOfFifthsPalette(
                        roots: SonatrixViewModel.circleOfFifthsRoots,
                        quality: selectedQuality,
                        durationBeats: selectedDurationBeats,
                        onAppend: appendChord)
                } else {
                    ChordGridPalette(
                        roots: SonatrixViewModel.rootOptions,
                        quality: selectedQuality,
                        durationBeats: selectedDurationBeats,
                        onAppend: appendChord)
                }
            }

            Spacer(minLength: 0)
        }
        .padding(.horizontal, 14)
        .padding(.bottom, 16)
    }

    private func makePayload(rootIndex: UInt8, qualityIndex: UInt8) -> DragChordPayload {
        let root = SonatrixViewModel.rootOption(for: rootIndex)
        let quality = SonatrixViewModel.qualityOption(for: qualityIndex)
        return DragChordPayload(
            rootName: root.displayName,
            qualityName: quality.storedName,
            rootIndex: root.id,
            qualityIndex: quality.id,
            durationBeats: selectedDurationBeats)
    }

    private func appendChord(_ payload: DragChordPayload) {
        #if STANDALONE
            viewModel.addChord(payload.makeChordItem())
        #endif
    }
}

private struct CircleOfFifthsPalette: View {
    let roots: [SonatrixViewModel.RootOption]
    let quality: SonatrixViewModel.QualityOption
    let durationBeats: Int
    let onAppend: (DragChordPayload) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Circle of Fifths")
                .font(.caption)
                .foregroundColor(.gray)

            GeometryReader { geometry in
                ZStack {
                    Circle()
                        .stroke(Color.white.opacity(0.08), lineWidth: 1)
                        .background(Circle().fill(Color.white.opacity(0.02)))

                    VStack(spacing: 4) {
                        Text(quality.displayLabel)
                            .font(.caption)
                            .foregroundColor(.gray)
                        Text("\(durationBeats) beat drag")
                            .font(.headline)
                            .foregroundColor(.white)
                    }
                    .frame(width: 92, height: 92)
                    .background(Circle().fill(Color.white.opacity(0.04)))

                    ForEach(Array(roots.enumerated()), id: \.element.id) { index, root in
                        let payload = DragChordPayload(
                            rootName: root.displayName,
                            qualityName: quality.storedName,
                            rootIndex: root.id,
                            qualityIndex: quality.id,
                            durationBeats: durationBeats)
                        let angle = (Double(index) / Double(roots.count)) * (.pi * 2.0) - (.pi / 2.0)
                        let radius = min(geometry.size.width, geometry.size.height) * 0.36
                        let centerX = geometry.size.width / 2
                        let centerY = geometry.size.height / 2
                        let x = centerX + CGFloat(cos(angle)) * radius
                        let y = centerY + CGFloat(sin(angle)) * radius

                        PaletteChordToken(payload: payload, compact: true, onAppend: onAppend)
                            .position(x: x, y: y)
                    }
                }
            }
            .frame(height: 250)
        }
    }
}

private struct ChordGridPalette: View {
    let roots: [SonatrixViewModel.RootOption]
    let quality: SonatrixViewModel.QualityOption
    let durationBeats: Int
    let onAppend: (DragChordPayload) -> Void

    private let columns = [
        GridItem(.adaptive(minimum: 86), spacing: 8)
    ]

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Chromatic Grid")
                .font(.caption)
                .foregroundColor(.gray)

            LazyVGrid(columns: columns, spacing: 8) {
                ForEach(roots) { root in
                    let payload = DragChordPayload(
                        rootName: root.displayName,
                        qualityName: quality.storedName,
                        rootIndex: root.id,
                        qualityIndex: quality.id,
                        durationBeats: durationBeats)
                    PaletteChordToken(payload: payload, compact: false, onAppend: onAppend)
                }
            }
        }
    }
}

private struct PaletteChordToken: View {
    let payload: DragChordPayload
    let compact: Bool
    let onAppend: (DragChordPayload) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: compact ? 2 : 4) {
            Text(payload.displayName)
                .font(compact ? .caption : .subheadline)
                .fontWeight(.semibold)
                .foregroundColor(.white)
            Text("\(payload.durationBeats) beats")
                .font(.caption2)
                .foregroundColor(.gray)
        }
        .padding(.horizontal, compact ? 10 : 12)
        .padding(.vertical, compact ? 7 : 9)
        .frame(minWidth: compact ? 68 : 86,
               maxWidth: compact ? nil : .infinity,
               alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 10)
                .fill(
                    LinearGradient(
                        colors: [Color.orange.opacity(0.18), Color.blue.opacity(0.14)],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing)
                )
        )
        .overlay(
            RoundedRectangle(cornerRadius: 10)
                .stroke(Color.white.opacity(0.08), lineWidth: 1)
        )
        .contentShape(RoundedRectangle(cornerRadius: 10))
        .onTapGesture {
            onAppend(payload)
        }
        .onDrag {
            makeChordItemProvider(for: payload)
        }
    }
}

private struct TimelineChordLane: View {
    let sectionFrames: [(index: Int, x: CGFloat, width: CGFloat)]
    let chords: [SonatrixViewModel.ChordItem]
    let sectionInset: CGFloat
    let height: CGFloat
    let onSelect: (Int) -> Void
    let onRemove: (Int) -> Void

    var body: some View {
        ZStack(alignment: .leading) {
            ForEach(sectionFrames, id: \.index) { frame in
                if frame.index < chords.count {
                    ZStack(alignment: .topTrailing) {
                        ChordBlock(
                            title: chords[frame.index].displayName,
                            subtitle: "\(chords[frame.index].durationBeats) beats",
                            width: frame.width,
                            height: height - 8,
                            sectionIndex: frame.index
                        )
                        .onTapGesture {
                            onSelect(frame.index)
                        }

                        Button(action: {
                            onRemove(frame.index)
                        }) {
                            Image(systemName: "xmark.circle.fill")
                                .foregroundColor(.white.opacity(0.92))
                                .background(Circle().fill(Color.red.opacity(0.82)))
                        }
                        .buttonStyle(PlainButtonStyle())
                        .padding(6)
                    }
                    .position(
                        x: sectionInset + frame.x + (frame.width / 2),
                        y: height / 2
                    )
                    .contentShape(RoundedRectangle(cornerRadius: 8))
                    .contextMenu {
                        Button("Remove") {
                            onRemove(frame.index)
                        }
                    }
                }
            }
        }
    }
}

private struct TimelinePlayhead: View {
    let x: CGFloat
    let height: CGFloat

    var body: some View {
        Rectangle()
            .fill(Color.orange)
            .frame(width: 2, height: height)
            .overlay(alignment: .top) {
                Circle()
                    .fill(Color.orange)
                    .frame(width: 10, height: 10)
                    .offset(y: -6)
            }
            .offset(x: x)
    }
}

struct ChordBlock: View {
    let title: String
    let subtitle: String
    let width: CGFloat
    let height: CGFloat
    let sectionIndex: Int

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title)
                .font(.subheadline)
                .fontWeight(.semibold)
                .foregroundColor(.white)
            Text(subtitle)
                .font(.caption2)
                .foregroundColor(.white.opacity(0.7))
        }
        .padding(.horizontal, 10)
        .frame(width: width, height: height, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 8)
                .fill(
                    LinearGradient(
                        colors: sectionIndex.isMultiple(of: 2)
                            ? [Color.blue.opacity(0.36), Color.cyan.opacity(0.18)]
                            : [Color.green.opacity(0.28), Color.blue.opacity(0.2)],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing)
                )
        )
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .stroke(Color.blue.opacity(0.85), lineWidth: 1)
        )
    }
}

#if STANDALONE
    private struct ChordNoteRow: View {
        @ObservedObject var viewModel: SonatrixViewModel
        @Binding var note: SonatrixViewModel.ChordStringNote
        let onMuteToggle: () -> Void
        let onFretChange: (Int) -> Void

        var body: some View {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Image(systemName: "line.3.horizontal")
                        .foregroundColor(.gray)
                    Text(viewModel.stringName(for: note.stringIndex))
                        .font(.subheadline)
                        .fontWeight(.semibold)
                    Spacer()
                    Text(viewModel.noteDisplayName(for: note))
                        .font(.caption)
                        .foregroundColor(.gray)
                }

                HStack(spacing: 12) {
                    Button(note.fret < 0 ? "Unmute" : "Mute") {
                        onMuteToggle()
                    }
                    .buttonStyle(BorderedButtonStyle())

                    Stepper(
                        "Fret \(note.fret < 0 ? "Muted" : "\(note.fret)")",
                        value: Binding(
                            get: { max(note.fret, 0) },
                            set: {
                                note.fret = $0
                                onFretChange($0)
                            }
                        ),
                        in: 0...15
                    )
                    .disabled(note.fret < 0)
                }

                HStack(spacing: 10) {
                    Text("Volume")
                        .font(.caption)
                        .foregroundColor(.gray)
                        .frame(width: 48, alignment: .leading)

                    Slider(
                        value: Binding(
                            get: { Double(note.velocity) },
                            set: { note.velocity = UInt8($0) }
                        ),
                        in: 1...127,
                        step: 1
                    )

                    Text("\(Int(note.velocity))")
                        .font(.caption)
                        .monospacedDigit()
                        .frame(width: 32, alignment: .trailing)
                }
            }
            .padding(12)
            .background(
                RoundedRectangle(cornerRadius: 10)
                    .fill(Color.white.opacity(0.05))
            )
            .overlay(
                RoundedRectangle(cornerRadius: 10)
                    .stroke(Color.white.opacity(0.08), lineWidth: 1)
            )
        }
    }

    private struct ChordNoteDropDelegate: DropDelegate {
        let targetNoteID: Int
        @Binding var notes: [SonatrixViewModel.ChordStringNote]
        @Binding var draggedNoteID: Int?

        func dropEntered(info: DropInfo) {
            guard let draggedNoteID = draggedNoteID,
                  draggedNoteID != targetNoteID,
                  let fromIndex = notes.firstIndex(where: { $0.id == draggedNoteID }),
                  let toIndex = notes.firstIndex(where: { $0.id == targetNoteID }) else {
                return
            }

            withAnimation(.easeInOut(duration: 0.12)) {
                notes.move(
                    fromOffsets: IndexSet(integer: fromIndex),
                    toOffset: toIndex > fromIndex ? toIndex + 1 : toIndex
                )
                normalizeOrders()
            }
        }

        func dropUpdated(info: DropInfo) -> DropProposal? {
            DropProposal(operation: .move)
        }

        func performDrop(info: DropInfo) -> Bool {
            normalizeOrders()
            draggedNoteID = nil
            return true
        }

        private func normalizeOrders() {
            for index in notes.indices {
                notes[index].order = index
            }
        }
    }
#endif
