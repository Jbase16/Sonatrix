import SwiftUI

public struct MixerView: View {
    #if STANDALONE
        @ObservedObject public var viewModel: SonatrixViewModel
    #endif

    private let busNames = ["Drums", "Bass", "Guitar", "Piano", "Strings"]
    private let busColors: [Color] = [.red, .blue, .green, .orange, .purple]

    #if STANDALONE
        public init(viewModel: SonatrixViewModel) {
            self.viewModel = viewModel
        }
    #else
        public init() {}
    #endif

    public var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Mixer")
                .font(.headline)
                .padding(.horizontal)

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 20) {
                    ForEach(0..<busNames.count, id: \.self) { index in
                        #if STANDALONE
                            ChannelStrip(
                                name: busNames[index],
                                color: busColors[index],
                                volume: Binding(
                                    get: { viewModel.busVolumes[index] },
                                    set: { newValue in
                                        viewModel.setVolume(bus: index, volume: newValue)
                                    }
                                )
                            )
                        #else
                            ChannelStrip(
                                name: busNames[index],
                                color: busColors[index],
                                volume: .constant(0.8)
                            )
                        #endif
                    }
                }
                .padding(.horizontal)
            }
        }
        .padding(.vertical, 12)
        .background(Color(NSColor.controlBackgroundColor))
        .cornerRadius(8)
    }
}

public struct ChannelStrip: View {
    public let name: String
    public let color: Color
    @Binding public var volume: Float

    public var body: some View {
        VStack(spacing: 8) {
            // Fader (Vertical Slider using a custom geometry or rotated native slider)
            // macOS standard Slider doesn't support vertical natively, so we create a simple custom one
            GeometryReader { geometry in
                ZStack(alignment: .bottom) {
                    // Track
                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color.gray.opacity(0.2))
                        .frame(width: 8)

                    // Fill level
                    RoundedRectangle(cornerRadius: 4)
                        .fill(color)
                        .frame(width: 8, height: geometry.size.height * CGFloat(volume))

                    // Fader Cap (Invisible interaction area)
                    Rectangle()
                        .fill(Color.clear)
                        .contentShape(Rectangle())
                        .gesture(
                            DragGesture(minimumDistance: 0)
                                .onChanged { value in
                                    // Calculate new volume based on vertical drag position (0.0 at bottom, 1.0 at top)
                                    let percentage = 1.0 - (value.location.y / geometry.size.height)
                                    volume = Float(max(0.0, min(1.0, percentage)))
                                }
                        )
                }
                .frame(maxWidth: .infinity)
            }
            .frame(width: 40, height: 112)

            Text(String(format: "%.1f", volume))
                .font(.caption2)
                .monospacedDigit()
                .foregroundColor(.secondary)

            Text(name)
                .font(.caption)
                .fontWeight(.medium)
        }
    }
}
