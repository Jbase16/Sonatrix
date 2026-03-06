import SwiftUI

@main
struct SonatrixApp: App {
    var body: some Scene {
        WindowGroup {
            ArrangementView()
                .frame(minWidth: 1000, minHeight: 700)
                .background(Color.black)
        }
        .windowStyle(HiddenTitleBarWindowStyle())
    }
}
