import SwiftUI
import CoreAudioKit

// -----------------------------------------------------------------------------
// Sonatrix Arrangement View
// 
// The main SwiftUI view for the AUv3 AudioExtension. Hosted within a generic 
// AUViewController wrapper to expose it to Logic Pro.
// -----------------------------------------------------------------------------

public struct ArrangementView: View {
    
    // Binding to the objective-C++ AUv3 parameter tree wrapper
    // @ObservedObject var audioUnitWrapper: AUv3Wrapper
    
    public init() {}
    
    public var body: some View {
        VStack(spacing: 0) {
            // 1. Top Ribbon: Global Controls
            TransportRibbon()
                .frame(height: 60)
                .background(Color.black)
            
            // 2. Chord Track Ruler
            ChordTrackRuler()
                .frame(height: 80)
                .background(Color(white: 0.15))
            
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
        }
        .preferredColorScheme(.dark)
    }
}

// MARK: - Subcomponents

struct TransportRibbon: View {
    var body: some View {
        HStack {
            Text("SONATRIX").font(.headline).fontWeight(.black).foregroundColor(.white).padding()
            Spacer()
            // Placeholder: Sync, Tempo override, Master Volume
            Text("SYNC: HOST").font(.caption).padding().background(Color.green.opacity(0.3)).cornerRadius(4)
        }
        .padding(.horizontal)
    }
}

struct ChordTrackRuler: View {
    var body: some View {
        HStack {
            Text("CHORD TRACK").font(.caption).foregroundColor(.gray)
            Spacer()
            // Mock Chord Blocks
            HStack(spacing: 2) {
                ChordBlock(chord: "Cmaj7", width: 100)
                ChordBlock(chord: "Fmaj9", width: 100)
                ChordBlock(chord: "G13", width: 100)
            }
            Spacer()
        }
        .padding(.horizontal)
    }
}

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
