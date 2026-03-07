import SwiftUI

class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        // VoiceManager / Engine Facade lifecycle is now owned by the @StateObject ViewModel
    }

    func applicationWillTerminate(_ notification: Notification) {
    }
}

@main
struct SonatrixApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

    @StateObject private var viewModel = SonatrixViewModel()

    var body: some Scene {
        WindowGroup {
            ArrangementView(viewModel: viewModel)
                .frame(minWidth: 1000, minHeight: 700)
                .background(Color.black)
        }
        .windowStyle(HiddenTitleBarWindowStyle())
    }
}
