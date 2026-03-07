import CoreAudioKit
import SwiftUI

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
            // 1. Top Ribbon: Global Controls
            #if STANDALONE
                TransportRibbon(viewModel: viewModel)
                    .frame(height: 60)
                    .background(Color.black)
            #else
                TransportRibbon()
                    .frame(height: 60)
                    .background(Color.black)
            #endif

            // 2. Chord Track Ruler
            #if STANDALONE
                ChordTrackRuler(viewModel: viewModel)
                    .frame(height: 80)
                    .background(Color(white: 0.15))
            #else
                ChordTrackRuler()
                    .frame(height: 80)
                    .background(Color(white: 0.15))
            #endif

            // 3. Main Split View: Browser vs arrangement
            HStack(spacing: 0) {

                // Left: Pattern Browser
                PatternBrowserView()
                    .frame(width: 300)
                    .background(Color(white: 0.1))
                    .border(Color.gray, width: 1)

                // Right: Multi-Lane Arrangement Timeline
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

            // 4. Bottom Panel: Audio Mixer
            #if STANDALONE
                MixerView(viewModel: viewModel)
                    .frame(height: 200)
                    .background(Color.black)
            #else
                // In AUv3, we might need a generic or different view model wrapper
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
            #endif

            Spacer()
            // Placeholder: Sync, Tempo override, Master Volume
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
    #endif

    var body: some View {
        HStack {
            Text("CHORD TRACK").font(.caption).foregroundColor(.gray)

            #if STANDALONE
                Button(action: {
                    let newChord = SonatrixViewModel.ChordItem(
                        rootName: "C",
                        qualityName: "maj",
                        rootIndex: 0,
                        qualityIndex: 0
                    )
                    viewModel.addChord(newChord)
                }) {
                    Image(systemName: "plus.circle.fill")
                        .foregroundColor(.green)
                }
                .buttonStyle(PlainButtonStyle())
            #endif

            Spacer()

            // Dynamic Chord Blocks
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 2) {
                    #if STANDALONE
                        ForEach(Array(viewModel.arrangementChords.enumerated()), id: \.element.id) {
                            index, chord in
                            ChordBlock(
                                chord: "\(chord.rootName)\(chord.qualityName)",
                                width: CGFloat(chord.durationTicks) / 19.2  // Rough pixel scaling
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
                    #else
                        ChordBlock(chord: "Cmaj7", width: 100)
                        ChordBlock(chord: "Fmaj9", width: 100)
                        ChordBlock(chord: "G13", width: 100)
                    #endif
                }
            }
            Spacer()

            #if STANDALONE
                Button("Clear") {
                    viewModel.clearArrangement()
                }
                .font(.caption)
                .padding(4)
                .background(Color.red.opacity(0.3))
                .cornerRadius(4)
                .buttonStyle(PlainButtonStyle())
            #endif
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
}

#if STANDALONE
    struct ChordEditorSheet: View {
        @State var chord: SonatrixViewModel.ChordItem
        var onSave: (SonatrixViewModel.ChordItem) -> Void
        var onCancel: () -> Void

        let rootNames = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
        let qualityNames = ["maj", "min", "dim", "aug", "dom7", "maj7", "min7", "hdim7"]

        var body: some View {
            VStack(spacing: 20) {
                Text("Edit Chord")
                    .font(.headline)

                Form {
                    Picker("Root Note", selection: $chord.rootIndex) {
                        ForEach(0..<rootNames.count, id: \.self) { i in
                            Text(rootNames[i]).tag(UInt8(i))
                        }
                    }
                    .onChange(of: chord.rootIndex) { _, newValue in
                        chord.rootName = rootNames[Int(newValue)]
                    }

                    Picker("Quality", selection: $chord.qualityIndex) {
                        ForEach(0..<qualityNames.count, id: \.self) { i in
                            Text(qualityNames[i]).tag(UInt8(i))
                        }
                    }
                    .onChange(of: chord.qualityIndex) { _, newValue in
                        chord.qualityName = qualityNames[Int(newValue)]
                    }

                    Stepper(
                        "Duration (Beats): \(chord.durationTicks / 480)",
                        value: Binding(
                            get: { Int(chord.durationTicks) / 480 },
                            set: { chord.durationTicks = UInt16($0 * 480) }
                        ), in: 1...32)
                }
                .frame(width: 300, height: 150)

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
            .frame(width: 400)
        }
    }
#endif

struct ChordBlock: View {
    let chord: String
    let width: CGFloat

    var body: some View {
        Text(chord)
            .font(.caption2)
            .frame(width: width, height: 40)
            .background(Color.blue.opacity(0.4))
            .cornerRadius(4)
            .overlay(RoundedRectangle(cornerRadius: 4).stroke(Color.blue, lineWidth: 1))
    }
}

struct InstrumentLane: View {
    let name: String
    let color: Color

    var body: some View {
        HStack {
            // Header
            VStack {
                Text(name).font(.subheadline).bold()
            }
            .frame(width: 80, height: 80)
            .background(Color(white: 0.2))

            // Track Area
            ZStack(alignment: .leading) {
                Rectangle().fill(Color(white: 0.1)).frame(height: 80)

                // Mock Pattern Clip with Delta Graph Overlay Indicator
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
                }.padding(.leading, 50)
            }
        }
    }
}

struct PatternBrowserView: View {
    var body: some View {
        VStack(alignment: .leading) {
            Text("LIBRARY").font(.caption).bold().padding()
            List {
                Text("Pop Rock Quarter Strum")
                Text("R&B 16th Pulse")
                Text("Indie Syncopation")
            }
            .listStyle(PlainListStyle())
        }
    }
}
