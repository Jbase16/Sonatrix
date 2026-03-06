import SwiftUI

// -----------------------------------------------------------------------------
// DeltaGraphEditor (Note-Level Rhythmic Override)
//
// Allows the user to select a generated clip on the timeline and mutate the
// specific strokes (MIR events) non-destructively by writing to the Delta Graph.
// -----------------------------------------------------------------------------

struct MIREventMock: Identifiable {
    let id = UUID()
    let positionPct: CGFloat  // 0.0 to 1.0 (relative to pattern length)
    let typeName: String
    var isDeleted: Bool = false
}

public struct DeltaGraphEditor: View {
    @State private var events: [MIREventMock] = [
        MIREventMock(positionPct: 0.1, typeName: "Downstroke"),
        MIREventMock(positionPct: 0.3, typeName: "Upstroke"),
        MIREventMock(positionPct: 0.5, typeName: "Downstroke"),
        MIREventMock(positionPct: 0.7, typeName: "Upstroke"),
        MIREventMock(positionPct: 0.9, typeName: "Palm Mute"),
    ]

    public init() {}

    public var body: some View {
        VStack(alignment: .leading, spacing: 0) {

            // Header
            HStack {
                Text("MIR RHYTHMIC EDITOR").font(.caption).bold()
                Spacer()
                Button(action: {
                    // Reset all delta graph overrides for this clip
                    for i in 0..<events.count {
                        events[i].isDeleted = false
                    }
                }) {
                    Text("Clear Overrides").font(.caption2)
                }
            }
            .padding()
            .background(Color(white: 0.15))

            // Abstract Note Canvas
            GeometryReader { geo in
                ZStack(alignment: .leading) {

                    // Grid lines
                    HStack {
                        Spacer()
                        Divider().background(Color.gray.opacity(0.3))
                        Spacer()
                        Divider().background(Color.gray.opacity(0.3))
                        Spacer()
                        Divider().background(Color.gray.opacity(0.3))
                        Spacer()
                    }

                    // Events
                    ForEach(events.indices, id: \.self) { index in
                        let ev = events[index]
                        let rectWidth: CGFloat = 20

                        VStack {
                            Image(
                                systemName: ev.isDeleted
                                    ? "xmark.circle.fill"
                                    : (ev.typeName == "Downstroke"
                                        ? "arrow.down.circle.fill" : "arrow.up.circle.fill")
                            )
                            .resizable()
                            .frame(width: 16, height: 16)
                            .foregroundColor(ev.isDeleted ? .red : .blue)

                            Text(ev.typeName)
                                .font(.system(size: 8))
                                .foregroundColor(ev.isDeleted ? .gray : .white)
                        }
                        .frame(width: 40)
                        .position(x: geo.size.width * ev.positionPct, y: geo.size.height / 2)
                        .opacity(ev.isDeleted ? 0.3 : 1.0)
                        // The Core Rhythmic Override Action
                        .onTapGesture {
                            events[index].isDeleted.toggle()
                            print(
                                "Mutated Delta Graph for event \(index): isDeleted = \(events[index].isDeleted)"
                            )
                            // In C++, this signals updating the DeltaGraph layer
                        }
                    }
                }
            }
            .frame(height: 100)
            .background(Color(white: 0.08))
            .border(Color(white: 0.2), width: 1)
        }
    }
}
