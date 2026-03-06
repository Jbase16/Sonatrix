import SwiftUI

class AppDelegate: NSObject, NSApplicationDelegate {
    var audioEngine: StandaloneAudioEngine?

    func applicationDidFinishLaunching(_ notification: Notification) {
        audioEngine = StandaloneAudioEngine()
        audioEngine?.start()
    }

    func applicationWillTerminate(_ notification: Notification) {
        audioEngine?.stop()
    }
}

@main
struct SonatrixApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

    var body: some Scene {
        WindowGroup {
            ArrangementView()
                .frame(minWidth: 1000, minHeight: 700)
                .background(Color.black)
        }
        .windowStyle(HiddenTitleBarWindowStyle())
    }
}
