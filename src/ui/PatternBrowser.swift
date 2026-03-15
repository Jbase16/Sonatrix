import SwiftUI

private enum PatternFilter: String, CaseIterable, Identifiable {
    case all
    case strum
    case picking

    var id: String { rawValue }

    var title: String {
        switch self {
        case .all:
            return "All"
        case .strum:
            return "Strum"
        case .picking:
            return "Picking"
        }
    }
}

private let allGenresTag = "All Genres"

public struct PatternBrowserView: View {
    #if STANDALONE
        @ObservedObject var viewModel: SonatrixViewModel
        @State private var searchText: String = ""
        @State private var selectedFilter: PatternFilter = .all
        @State private var selectedGenre: String = allGenresTag

        public init(viewModel: SonatrixViewModel) {
            self.viewModel = viewModel
        }
    #else
        public init() {}
    #endif

    public var body: some View {
        #if STANDALONE
            ScrollView(.vertical, showsIndicators: true) {
                VStack(alignment: .leading, spacing: 18) {
                    header
                    currentPatternCard
                    progressionSection
                    searchSection
                    filterSection
                    patternListSection
                }
                .padding(18)
            }
            .background(
                LinearGradient(
                    colors: [Color(red: 0.07, green: 0.08, blue: 0.11),
                             Color(red: 0.04, green: 0.05, blue: 0.07)],
                    startPoint: .top,
                    endPoint: .bottom)
            )
        #else
            Text("Pattern Browser Unavailable in AUv3 Mock")
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(Color.black)
        #endif
    }

    #if STANDALONE
        private var filteredPatterns: [SonatrixViewModel.PatternDescriptor] {
            viewModel.availablePatterns.filter { pattern in
                let matchesSearch = searchText.isEmpty
                    || pattern.name.localizedCaseInsensitiveContains(searchText)
                    || pattern.genre.localizedCaseInsensitiveContains(searchText)
                    || pattern.timeSignature.localizedCaseInsensitiveContains(searchText)

                let matchesFilter: Bool
                switch selectedFilter {
                case .all:
                    matchesFilter = true
                case .strum:
                    matchesFilter = pattern.category == .strum
                case .picking:
                    matchesFilter = pattern.category == .picking
                }

                let matchesGenre = selectedGenre == allGenresTag || pattern.genre == selectedGenre
                return matchesSearch && matchesFilter && matchesGenre
            }
        }

        private var header: some View {
            VStack(alignment: .leading, spacing: 4) {
                Text("PATTERN BROWSER")
                    .font(.caption)
                    .foregroundColor(.gray)
                Text("Strum and Picking Patterns")
                    .font(.title3)
                    .fontWeight(.semibold)
                    .foregroundColor(.white)
                Text("Choose the guitar feel for the whole progression, then auto-fill a chord sequence if you want a quick listening pass.")
                    .font(.caption)
                    .foregroundColor(.gray)
            }
        }

        private var currentPatternCard: some View {
            VStack(alignment: .leading, spacing: 10) {
                Text("Current Playback Pattern")
                    .font(.caption)
                    .foregroundColor(.gray)

                if let selectedPattern = viewModel.selectedPattern {
                    HStack(alignment: .top) {
                        VStack(alignment: .leading, spacing: 6) {
                            Text(selectedPattern.name)
                                .font(.headline)
                                .foregroundColor(.white)
                            Text("\(selectedPattern.genre)  •  \(selectedPattern.timeSignature)")
                                .font(.caption)
                                .foregroundColor(.gray)
                        }

                        Spacer()

                        PatternBadge(category: selectedPattern.category)
                    }
                } else {
                    Text("No guitar patterns are available.")
                        .font(.subheadline)
                        .foregroundColor(.white)
                }
            }
            .padding(14)
            .background(
                RoundedRectangle(cornerRadius: 14)
                    .fill(Color.white.opacity(0.05))
            )
            .overlay(
                RoundedRectangle(cornerRadius: 14)
                    .stroke(Color.white.opacity(0.08), lineWidth: 1)
            )
        }

        private var progressionSection: some View {
            VStack(alignment: .leading, spacing: 10) {
                Text("Auto-Fill Progression")
                    .font(.caption)
                    .foregroundColor(.gray)

                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 10) {
                        ForEach(SonatrixViewModel.progressionPresets) { preset in
                            Button(action: {
                                viewModel.applyProgressionPreset(preset)
                            }) {
                                ProgressionPresetCard(preset: preset)
                            }
                            .buttonStyle(PlainButtonStyle())
                        }
                    }
                    .padding(.vertical, 2)
                }
            }
        }

        private var searchSection: some View {
            HStack(spacing: 10) {
                Image(systemName: "magnifyingglass")
                    .foregroundColor(.gray)
                TextField("Search patterns, genres, or time signatures", text: $searchText)
                    .textFieldStyle(PlainTextFieldStyle())
                    .foregroundColor(.white)
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .background(
                RoundedRectangle(cornerRadius: 12)
                    .fill(Color.white.opacity(0.05))
            )
            .overlay(
                RoundedRectangle(cornerRadius: 12)
                    .stroke(Color.white.opacity(0.08), lineWidth: 1)
            )
        }

        private var filterSection: some View {
            VStack(alignment: .leading, spacing: 12) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("Pattern Type")
                        .font(.caption)
                        .foregroundColor(.gray)

                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 8) {
                            ForEach(PatternFilter.allCases) { filter in
                                FilterChip(
                                    title: filter.title,
                                    isSelected: selectedFilter == filter,
                                    accentColor: filter == .picking ? .orange : .blue
                                ) {
                                    selectedFilter = filter
                                }
                            }
                        }
                        .padding(.vertical, 2)
                    }
                }

                VStack(alignment: .leading, spacing: 8) {
                    Text("Genre")
                        .font(.caption)
                        .foregroundColor(.gray)

                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 8) {
                            ForEach([allGenresTag] + viewModel.patternGenres, id: \.self) { genre in
                                FilterChip(
                                    title: genre,
                                    isSelected: selectedGenre == genre,
                                    accentColor: .green
                                ) {
                                    selectedGenre = genre
                                }
                            }
                        }
                        .padding(.vertical, 2)
                    }
                }
            }
        }

        private var patternListSection: some View {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text("Available Patterns")
                        .font(.caption)
                        .foregroundColor(.gray)
                    Spacer()
                    Text("\(filteredPatterns.count)")
                        .font(.caption)
                        .foregroundColor(.gray)
                }

                if filteredPatterns.isEmpty {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("No patterns match the current filters.")
                            .font(.subheadline)
                            .foregroundColor(.white)
                        Text("Try clearing the search text or choosing a different tag.")
                            .font(.caption)
                            .foregroundColor(.gray)
                    }
                    .padding(14)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(
                        RoundedRectangle(cornerRadius: 14)
                            .fill(Color.white.opacity(0.04))
                    )
                } else {
                    LazyVStack(spacing: 10) {
                        ForEach(filteredPatterns) { pattern in
                            Button(action: {
                                viewModel.selectPattern(id: pattern.id)
                            }) {
                                PatternCard(
                                    pattern: pattern,
                                    isSelected: viewModel.selectedPatternID == pattern.id)
                            }
                            .buttonStyle(PlainButtonStyle())
                        }
                    }
                }
            }
        }
    #endif
}

private struct FilterChip: View {
    let title: String
    let isSelected: Bool
    let accentColor: Color
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.caption)
                .foregroundColor(.white)
                .padding(.horizontal, 12)
                .padding(.vertical, 7)
                .background(
                    Capsule()
                        .fill(isSelected ? accentColor.opacity(0.28) : Color.white.opacity(0.05))
                )
                .overlay(
                    Capsule()
                        .stroke(isSelected ? accentColor : Color.white.opacity(0.08), lineWidth: 1)
                )
        }
        .buttonStyle(PlainButtonStyle())
    }
}

private struct ProgressionPresetCard: View {
    let preset: SonatrixViewModel.ProgressionPreset

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(preset.title)
                .font(.subheadline)
                .fontWeight(.semibold)
                .foregroundColor(.white)
            Text(preset.subtitle)
                .font(.caption)
                .foregroundColor(.gray)
            Text(preset.chordSummary)
                .font(.caption2)
                .foregroundColor(.white.opacity(0.8))
        }
        .padding(12)
        .frame(width: 190, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 14)
                .fill(
                    LinearGradient(
                        colors: [Color.blue.opacity(0.22), Color.orange.opacity(0.18)],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing)
                )
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14)
                .stroke(Color.white.opacity(0.08), lineWidth: 1)
        )
    }
}

private struct PatternCard: View {
    let pattern: SonatrixViewModel.PatternDescriptor
    let isSelected: Bool

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            VStack(alignment: .leading, spacing: 6) {
                Text(pattern.name)
                    .font(.subheadline)
                    .fontWeight(.semibold)
                    .foregroundColor(.white)

                Text("\(pattern.genre)  •  \(pattern.timeSignature)  •  \(pattern.eventCount) events")
                    .font(.caption)
                    .foregroundColor(.gray)
            }

            Spacer()

            VStack(alignment: .trailing, spacing: 8) {
                PatternBadge(category: pattern.category)

                Text(isSelected ? "Selected" : "Use")
                    .font(.caption)
                    .foregroundColor(isSelected ? .orange : .white)
                    .padding(.horizontal, 10)
                    .padding(.vertical, 5)
                    .background(
                        Capsule()
                            .fill(isSelected ? Color.orange.opacity(0.18) : Color.white.opacity(0.05))
                    )
            }
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 14)
                .fill(isSelected ? Color.orange.opacity(0.12) : Color.white.opacity(0.04))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 14)
                .stroke(isSelected ? Color.orange : Color.white.opacity(0.08),
                        lineWidth: isSelected ? 1.5 : 1)
        )
    }
}

private struct PatternBadge: View {
    let category: SonatrixViewModel.PatternCategory

    var body: some View {
        Text(category.displayName)
            .font(.caption2)
            .fontWeight(.semibold)
            .foregroundColor(.white)
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
            .background(
                Capsule()
                    .fill(category == .picking ? Color.orange.opacity(0.28) : Color.blue.opacity(0.28))
            )
            .overlay(
                Capsule()
                    .stroke(category == .picking ? Color.orange : Color.blue, lineWidth: 1)
            )
    }
}
