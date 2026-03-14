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
    #if STANDALONE
        @ObservedObject var viewModel: SonatrixViewModel

        public init(viewModel: SonatrixViewModel) {
            self.viewModel = viewModel
        }
    #else
        public init() {}
    #endif

    public var body: some View {
        VStack(spacing: 0) {
            #if STANDALONE
                TransportRibbon(viewModel: viewModel)
                    .frame(height: 60)
                    .background(Color.black)
            #else
                TransportRibbon()
                    .frame(height: 60)
                    .background(Color.black)
            #endif

            #if STANDALONE
                ChordTrackRuler(viewModel: viewModel)
                    .frame(height: 118)
                    .background(Color(white: 0.15))
            #else
                ChordTrackRuler()
                    .frame(height: 118)
                    .background(Color(white: 0.15))
            #endif

            HStack(spacing: 0) {
                #if STANDALONE
                    ChordPaletteView(viewModel: viewModel)
                        .frame(width: 320)
                        .background(
                            LinearGradient(
                                colors: [Color(red: 0.11, green: 0.12, blue: 0.15),
                                         Color(red: 0.08, green: 0.08, blue: 0.1)],
                                startPoint: .top,
                                endPoint: .bottom)
                        )
                        .border(Color.white.opacity(0.08), width: 1)
                #else
                    ChordPaletteView()
                        .frame(width: 320)
                        .background(Color(white: 0.1))
                        .border(Color.white.opacity(0.08), width: 1)
                #endif

                ScrollView(.vertical) {
                    VStack(spacing: 2) {
                        InstrumentLane(name: "Drums", color: .purple)
                        InstrumentLane(name: "Bass", color: .blue)
                        InstrumentLane(name: "Guitar", color: .orange)
                        InstrumentLane(name: "Piano", color: .green)
                        InstrumentLane(name: "Strings", color: .cyan)
                    }
                    .padding(.top, 4)
                }
                .background(Color(white: 0.05))
            }

            #if STANDALONE
                MixerView(viewModel: viewModel)
                    .frame(height: 200)
                    .background(Color.black)
            #else
                Text("Mixer Not Available in AUv3 Mock")
                    .frame(height: 200)
                    .background(Color.black)
            #endif
        }
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
            Text("SONATRIX").font(.headline).fontWeight(.black).foregroundColor(.white).padding()
            Spacer()

            #if STANDALONE
                Button(action: {
                    viewModel.togglePlayback()
                }) {
                    Image(systemName: viewModel.isPlaying ? "pause.fill" : "play.fill")
                        .font(.title)
                        .foregroundColor(viewModel.isPlaying ? .green : .white)
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
            #endif

            Spacer()
            Text("SYNC: HOST").font(.caption).padding().background(Color.green.opacity(0.3))
                .cornerRadius(4)
        }
        .padding(.horizontal)
    }
}

struct ChordTrackRuler: View {
    #if STANDALONE
        @ObservedObject var viewModel: SonatrixViewModel
        @State private var isShowingEditor = false
        @State private var editingChord: SonatrixViewModel.ChordItem?
        @State private var editingIndex: Int?
        @State private var isDropTargeted = false
    #endif

    private let pixelsPerBeat: CGFloat = 30
    private let blockSpacing: CGFloat = 6

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

                    ZStack(alignment: .leading) {
                        RoundedRectangle(cornerRadius: 10)
                            .fill(Color.white.opacity(0.04))

                        TimelineBarBackground(
                            width: timelineWidth,
                            pixelsPerBeat: pixelsPerBeat)
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
                            HStack(spacing: blockSpacing) {
                                ForEach(Array(viewModel.arrangementChords.enumerated()), id: \.element.id) {
                                    index, chord in
                                    ChordBlock(
                                        title: chord.displayName,
                                        subtitle: "\(chord.durationBeats) beats",
                                        width: viewModel.width(for: chord, pixelsPerBeat: pixelsPerBeat)
                                    )
                                    .contextMenu {
                                        Button("Remove") {
                                            viewModel.removeChord(at: index)
                                        }
                                    }
                                    .onTapGesture {
                                        editingIndex = index
                                        editingChord = chord
                                        isShowingEditor = true
                                    }
                                }
                            }
                            .padding(.horizontal, 12)
                            .padding(.vertical, 8)
                        }
                    }
                    .frame(width: timelineWidth, height: 60)
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
                #else
                    HStack(spacing: blockSpacing) {
                        ChordBlock(title: "C", subtitle: "4 beats", width: 120)
                        ChordBlock(title: "Am", subtitle: "4 beats", width: 120)
                        ChordBlock(title: "F", subtitle: "4 beats", width: 120)
                        ChordBlock(title: "G", subtitle: "4 beats", width: 120)
                    }
                #endif
            }
        }
        .padding(.horizontal)
        #if STANDALONE
            .sheet(isPresented: $isShowingEditor) {
                if let chord = editingChord, let index = editingIndex {
                    ChordEditorSheet(
                        chord: chord,
                        onSave: { updatedChord in
                            viewModel.updateChord(at: index, with: updatedChord)
                            isShowingEditor = false
                        },
                        onCancel: {
                            isShowingEditor = false
                        }
                    )
                }
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
                guard let data,
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
    #endif
}

#if STANDALONE
    struct ChordEditorSheet: View {
        @State var chord: SonatrixViewModel.ChordItem
        var onSave: (SonatrixViewModel.ChordItem) -> Void
        var onCancel: () -> Void

        var body: some View {
            VStack(spacing: 20) {
                Text("Edit Chord")
                    .font(.headline)

                Form {
                    Picker("Root Note", selection: $chord.rootIndex) {
                        ForEach(SonatrixViewModel.rootOptions) { option in
                            Text(option.displayName).tag(option.id)
                        }
                    }
                    .onChange(of: chord.rootIndex) { _, newValue in
                        chord.rootName = SonatrixViewModel.rootOption(for: newValue).displayName
                    }

                    Picker("Quality", selection: $chord.qualityIndex) {
                        ForEach(SonatrixViewModel.qualityOptions) { option in
                            Text(option.displayLabel).tag(option.id)
                        }
                    }
                    .onChange(of: chord.qualityIndex) { _, newValue in
                        chord.qualityName = SonatrixViewModel.qualityOption(for: newValue).storedName
                    }

                    Stepper(
                        "Duration (Beats): \(chord.durationBeats)",
                        value: Binding(
                            get: { chord.durationBeats },
                            set: { chord.durationTicks = UInt16($0) * SonatrixViewModel.ticksPerBeat }
                        ),
                        in: 1...32)
                }
                .frame(width: 320, height: 170)

                HStack {
                    Button("Cancel", action: onCancel)
                        .buttonStyle(PlainButtonStyle())
                    Spacer()
                    Button("Save") { onSave(chord) }
                        .buttonStyle(BorderedButtonStyle())
                        .tint(.blue)
                }
            }
            .padding()
            .frame(width: 420)
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

private struct TimelineBarBackground: View {
    let width: CGFloat
    let pixelsPerBeat: CGFloat

    var body: some View {
        Canvas { context, size in
            let barWidth = pixelsPerBeat * 4
            var x: CGFloat = 0
            while x <= max(width, size.width) {
                let rect = CGRect(x: x, y: 0, width: 1, height: size.height)
                context.fill(Path(rect), with: .color(Color.white.opacity(0.06)))
                x += barWidth
            }
        }
    }
}

struct ChordBlock: View {
    let title: String
    let subtitle: String
    let width: CGFloat

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
        .frame(width: width, height: 42, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 8)
                .fill(
                    LinearGradient(
                        colors: [Color.blue.opacity(0.35), Color.cyan.opacity(0.18)],
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

struct InstrumentLane: View {
    let name: String
    let color: Color

    var body: some View {
        HStack {
            VStack {
                Text(name).font(.subheadline).bold()
            }
            .frame(width: 80, height: 80)
            .background(Color(white: 0.2))

            ZStack(alignment: .leading) {
                Rectangle().fill(Color(white: 0.1)).frame(height: 80)

                HStack {
                    Text("Pattern Clip")
                        .font(.caption)
                        .padding(4)
                        .frame(width: 150, height: 60)
                        .background(color.opacity(0.3))
                        .cornerRadius(6)
                        .overlay(
                            RoundedRectangle(cornerRadius: 6)
                                .stroke(color, style: StrokeStyle(lineWidth: 1, dash: [4]))
                        )
                }
                .padding(.leading, 50)
            }
        }
    }
}
