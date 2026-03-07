import SwiftUI
import UniformTypeIdentifiers

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
        .commands {
            CommandGroup(replacing: .saveItem) {
                Button("Save Project...") {
                    saveProject()
                }
                .keyboardShortcut("s", modifiers: .command)
            }
            CommandGroup(replacing: .newItem) {
                Button("Open Project...") {
                    openProject()
                }
                .keyboardShortcut("o", modifiers: .command)
            }
            CommandMenu("Export") {
                Button("Export Audio...") {
                    exportAudio()
                }
                .keyboardShortcut("b", modifiers: .command)
            }
        }
    }

    private func saveProject() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.json]
        panel.canCreateDirectories = true
        panel.nameFieldStringValue = "MyProgression.json"

        if panel.runModal() == .OK, let url = panel.url {
            do {
                try viewModel.saveProject(to: url)
            } catch {
                print("Failed to save project: \(error)")
            }
        }
    }

    private func openProject() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.json]
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        panel.canChooseFiles = true

        if panel.runModal() == .OK, let url = panel.url {
            do {
                try viewModel.loadProject(from: url)
            } catch {
                print("Failed to load project: \(error)")
            }
        }
    }

    private func exportAudio() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [UTType.wav]
        panel.canCreateDirectories = true
        panel.nameFieldStringValue = "ExportedAudio.wav"

        if panel.runModal() == .OK, let url = panel.url {
            do {
                try viewModel.bounceAudio(to: url)
            } catch {
                print("Failed to bounce audio: \(error)")
            }
        }
    }
}
