import SwiftUI

// -----------------------------------------------------------------------------
// PatternBrowser (EZkeys Drag & Drop Paradigm)
//
// Displays searchable MIR patterns. Handles the NSItemProvider drag operations
// for "semantic MIDI export" into Logic Pro or directly into Sonatrix lanes.
// -----------------------------------------------------------------------------

struct PatternItem: Identifiable {
    let id = UUID()
    let name: String
    let genre: String
    let feel: String
}

public struct PatternBrowser: View {
    @State private var searchText: String = ""

    // Mock database
    private let mockDatabase: [PatternItem] = [
        PatternItem(name: "Pop Rock Quarter Strum", genre: "Rock", feel: "Straight"),
        PatternItem(name: "R&B 16th Pulse", genre: "R&B", feel: "Straight"),
        PatternItem(name: "Indie Syncopation", genre: "Indie", feel: "Swing"),
        PatternItem(name: "Cinematic Swell", genre: "Orchestral", feel: "Free"),
    ]

    public init() {}

    var filteredPatterns: [PatternItem] {
        if searchText.isEmpty {
            return mockDatabase
        } else {
            return mockDatabase.filter {
                $0.name.localizedCaseInsensitiveContains(searchText)
                    || $0.genre.localizedCaseInsensitiveContains(searchText)
            }
        }
    }

    public var body: some View {
        VStack(spacing: 0) {
            // Search Bar
            HStack {
                Image(systemName: "magnifyingglass").foregroundColor(.gray)
                TextField("Search Patterns...", text: $searchText)
                    .textFieldStyle(PlainTextFieldStyle())
            }
            .padding()
            .background(Color(white: 0.15))

            // List
            List(filteredPatterns) { pattern in
                PatternRow(pattern: pattern)
                    // Core Feature: Enable Drag and Drop to Logic Pro or internal lanes
                    .onDrag {
                        // In production, this generates a serialized MIR descriptor payload
                        // or triggers the C++ engine to compile a temporary .mid file
                        return NSItemProvider(object: pattern.name as NSString)
                    }
            }
            .listStyle(PlainListStyle())
        }
    }
}

struct PatternRow: View {
    let pattern: PatternItem
    var body: some View {
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text(pattern.name).font(.subheadline).bold()
                HStack {
                    Text(pattern.genre).font(.caption).foregroundColor(.gray)
                    Text("•").font(.caption).foregroundColor(.gray)
                    Text(pattern.feel).font(.caption).foregroundColor(.gray)
                }
            }
            Spacer()
            Image(systemName: "play.circle")
                .foregroundColor(.blue)
                // Audition the pattern temporarily
                .onTapGesture {
                    print("Auditioning \(pattern.name)")
                }
        }
        .padding(.vertical, 4)
    }
}
