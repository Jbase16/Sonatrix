#include <Accelerate/Accelerate.h>
#include <AudioToolbox/AudioToolbox.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// -----------------------------------------------------
// WAV I/O with Enforced CoreAudio SRC
// -----------------------------------------------------
bool LoadWav(const std::string &path, std::vector<float> &outData,
             double targetSampleRate) {
  CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(),
                                              kCFStringEncodingUTF8);
  if (!str)
    return false;

  CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, str,
                                               kCFURLPOSIXPathStyle, false);
  CFRelease(str);
  if (!url)
    return false;

  ExtAudioFileRef audioFile = nullptr;
  if (ExtAudioFileOpenURL(url, &audioFile) != noErr) {
    CFRelease(url);
    return false;
  }
  CFRelease(url);

  AudioStreamBasicDescription clientFormat = {};
  clientFormat.mSampleRate = targetSampleRate;
  clientFormat.mFormatID = kAudioFormatLinearPCM;
  clientFormat.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  clientFormat.mBytesPerPacket = sizeof(float);
  clientFormat.mFramesPerPacket = 1;
  clientFormat.mBytesPerFrame = sizeof(float);
  clientFormat.mChannelsPerFrame = 1;
  clientFormat.mBitsPerChannel = 32;

  if (ExtAudioFileSetProperty(audioFile, kExtAudioFileProperty_ClientDataFormat,
                              sizeof(clientFormat), &clientFormat) != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  AudioStreamBasicDescription sourceFormat = {};
  UInt32 srcSize = sizeof(sourceFormat);
  ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileDataFormat,
                          &srcSize, &sourceFormat);

  SInt64 sourceFrames = 0;
  UInt32 frameSize = sizeof(sourceFrames);
  if (ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileLengthFrames,
                              &frameSize, &sourceFrames) != noErr ||
      sourceFrames <= 0) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  const double srcRate = (sourceFormat.mSampleRate > 0.0)
                             ? sourceFormat.mSampleRate
                             : targetSampleRate;
  const size_t targetFrames = static_cast<size_t>(
      std::ceil(sourceFrames * (targetSampleRate / srcRate)));
  outData.resize(targetFrames);

  AudioBufferList bufferList = {};
  bufferList.mNumberBuffers = 1;
  bufferList.mBuffers[0].mNumberChannels = 1;
  bufferList.mBuffers[0].mDataByteSize =
      static_cast<UInt32>(outData.size() * sizeof(float));
  bufferList.mBuffers[0].mData = outData.data();

  UInt32 ioFrames = static_cast<UInt32>(targetFrames);
  if (ExtAudioFileRead(audioFile, &ioFrames, &bufferList) != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  ExtAudioFileDispose(audioFile);
  return true;
}

bool SaveWav(const std::string &path, const std::vector<float> &data,
             double sampleRate) {
  CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(),
                                              kCFStringEncodingUTF8);
  if (!str)
    return false;

  CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, str,
                                               kCFURLPOSIXPathStyle, false);
  CFRelease(str);
  if (!url)
    return false;

  AudioStreamBasicDescription fileFormat = {};
  fileFormat.mSampleRate = sampleRate;
  fileFormat.mFormatID = kAudioFormatLinearPCM;
  fileFormat.mFormatFlags =
      kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  fileFormat.mBytesPerPacket = 2;
  fileFormat.mFramesPerPacket = 1;
  fileFormat.mBytesPerFrame = 2;
  fileFormat.mChannelsPerFrame = 1;
  fileFormat.mBitsPerChannel = 16;

  ExtAudioFileRef audioFile = nullptr;
  if (ExtAudioFileCreateWithURL(url, kAudioFileWAVEType, &fileFormat, nullptr,
                                kAudioFileFlags_EraseFile,
                                &audioFile) != noErr) {
    CFRelease(url);
    return false;
  }
  CFRelease(url);

  AudioStreamBasicDescription clientFormat = {};
  clientFormat.mSampleRate = sampleRate;
  clientFormat.mFormatID = kAudioFormatLinearPCM;
  clientFormat.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  clientFormat.mBytesPerPacket = sizeof(float);
  clientFormat.mFramesPerPacket = 1;
  clientFormat.mBytesPerFrame = sizeof(float);
  clientFormat.mChannelsPerFrame = 1;
  clientFormat.mBitsPerChannel = 32;

  if (ExtAudioFileSetProperty(audioFile, kExtAudioFileProperty_ClientDataFormat,
                              sizeof(clientFormat), &clientFormat) != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  AudioBufferList bufferList = {};
  bufferList.mNumberBuffers = 1;
  bufferList.mBuffers[0].mNumberChannels = 1;
  bufferList.mBuffers[0].mDataByteSize =
      static_cast<UInt32>(data.size() * sizeof(float));
  bufferList.mBuffers[0].mData = const_cast<float *>(data.data());

  UInt32 frameCount = static_cast<UInt32>(data.size());
  bool ok = ExtAudioFileWrite(audioFile, frameCount, &bufferList) == noErr;
  ExtAudioFileDispose(audioFile);
  return ok;
}

// -----------------------------------------------------
// Math Helpers
// -----------------------------------------------------
static inline double Clamp(double x, double lo, double hi) {
  return std::max(lo, std::min(x, hi));
}

static inline float ClampFloat(float x, float lo, float hi) {
  return std::max(lo, std::min(x, hi));
}

static inline double Hash01(uint32_t x) {
  x ^= x >> 17;
  x *= 0xed5ad4bbU;
  x ^= x >> 11;
  x *= 0xac4c1b51U;
  x ^= x >> 15;
  x *= 0x31848babU;
  x ^= x >> 14;
  return static_cast<double>(x) / static_cast<double>(UINT32_MAX);
}

static inline double HashSigned(uint32_t x) { return (Hash01(x) * 2.0) - 1.0; }

// -----------------------------------------------------
// Real-Time Synthesizer Architecture
// -----------------------------------------------------
class GuitarSynthesizer {
public:
  enum class Articulation { Strum, Pluck };

  struct NoteEvent {
    size_t startSample = 0;
    int stringIndex = 0;
    double targetFreq = 0.0;
    float velocity = 1.0f;
    double durationSec = 1.0;
    bool isDownStrum = true;
    bool isNewChord = false;
    int activeStrings = 1;
    uint32_t gestureId = 0;
    Articulation articulation = Articulation::Strum;
  };

  struct StringVoice {
    const std::vector<float> *anchor = nullptr;
    double freq = 0.0;
    double pitchRatio = 1.0;

    double readHead = 0.0;
    size_t framesRendered = 0;
    size_t maxFrames = 0;

    float gain = 1.0f;
    size_t attackSamples = 0;

    bool releasing = false;
    size_t releaseSamples = 1;
    size_t releasePos = 0;
    float releaseStartAmp = 1.0f;

    float lastAmp = 0.0f;
    bool active = false;

    void Start(const std::vector<float> *inAnchor, double inFreq,
               double inRatio, double startHead, size_t inMaxFrames,
               float inGain, size_t inAttackSamples) {
      anchor = inAnchor;
      freq = inFreq;
      pitchRatio = inRatio;
      readHead = startHead;
      maxFrames = inMaxFrames;
      framesRendered = 0;
      gain = inGain;
      attackSamples = inAttackSamples;
      releasing = false;
      releaseSamples = 1;
      releasePos = 0;
      releaseStartAmp = 1.0f;
      lastAmp = 0.0f;
      active = (anchor != nullptr && !anchor->empty() && maxFrames > 0);
    }

    void StartRelease(size_t inReleaseSamples) {
      if (!active)
        return;
      if (!releasing) {
        releasing = true;
        releaseSamples = std::max<size_t>(1, inReleaseSamples);
        releasePos = 0;
        releaseStartAmp = (lastAmp > 0.0f) ? lastAmp : 1.0f;
      }
    }

    bool IsAudiblyActive() const {
      return active && framesRendered < maxFrames;
    }

    float RenderOne() {
      if (!active || framesRendered >= maxFrames) {
        active = false;
        lastAmp = 0.0f;
        return 0.0f;
      }

      size_t idx = static_cast<size_t>(readHead);
      if (idx >= anchor->size()) {
        active = false;
        lastAmp = 0.0f;
        return 0.0f;
      }

      float amp = 1.0f;
      if (attackSamples > 0 && framesRendered < attackSamples) {
        amp *= static_cast<float>(framesRendered) /
               static_cast<float>(attackSamples);
      }

      if (releasing) {
        float rel = 1.0f - (static_cast<float>(releasePos) /
                            static_cast<float>(releaseSamples));
        amp *= std::max(0.0f, rel) * releaseStartAmp;
      }

      lastAmp = amp;

      float v1 = (*anchor)[idx];
      float v2 = (idx + 1 < anchor->size()) ? (*anchor)[idx + 1] : 0.0f;
      float frac = static_cast<float>(readHead - idx);
      float sample = (v1 + (v2 - v1) * frac) * gain * amp;

      readHead += pitchRatio;
      ++framesRendered;

      if (releasing && ++releasePos >= releaseSamples)
        active = false;

      return sample;
    }
  };

  GuitarSynthesizer(double sr, size_t totalSamples)
      : sampleRate_(sr), totalSamples_(totalSamples) {
    baseFreqs_ = {82.41, 110.00, 146.83, 196.00, 246.94, 329.63};
  }

  bool LoadAnchors(const std::array<std::string, 6> &paths) {
    for (int i = 0; i < 6; ++i) {
      if (!LoadWav(paths[i], anchors_[i], sampleRate_)) {
        std::cerr << "Failed to load anchor: " << paths[i] << std::endl;
        return false;
      }
    }
    return true;
  }

  void ScheduleStrum(double startTimeSec, bool isDownStrum, float velocity,
                     double durationSec, bool isNewChord, double f1, double f2,
                     double f3, double f4, double f5, double f6) {
    const double freqs[6] = {f1, f2, f3, f4, f5, f6};

    int firstActive = -1, lastActive = -1, activeStrings = 0;
    for (int i = 0; i < 6; ++i) {
      if (freqs[i] > 0.0) {
        if (firstActive < 0)
          firstActive = i;
        lastActive = i;
        ++activeStrings;
      }
    }

    if (activeStrings == 0)
      return;

    const uint32_t gestureId = nextGestureId_++;
    double baseDelayMs = isDownStrum ? 12.0 : 9.0;
    baseDelayMs += HashSigned(gestureId * 101u + 7u) * 1.2;
    baseDelayMs = Clamp(baseDelayMs, 7.5, 15.0);

    double curve = 0.92 + (HashSigned(gestureId * 191u + 3u) * 0.025);
    curve = Clamp(curve, 0.88, 0.97);

    const size_t baseStart =
        static_cast<size_t>(std::max(0.0, startTimeSec) * sampleRate_);

    for (int strIndex = 0; strIndex < 6; ++strIndex) {
      if (freqs[strIndex] <= 0.0)
        continue;

      size_t eventStart = baseStart;

      if (activeStrings > 1) {
        const int origin = isDownStrum ? firstActive : lastActive;
        int relIdx = isDownStrum ? (strIndex - origin) : (origin - strIndex);
        relIdx = std::max(0, relIdx);

        double offsetMs = 0.0;
        for (int k = 0; k < relIdx; ++k) {
          offsetMs += baseDelayMs * std::pow(curve, static_cast<double>(k));
        }

        if (relIdx > 0) {
          offsetMs += HashSigned(gestureId * 313u +
                                 static_cast<uint32_t>(strIndex) * 17u) *
                      0.9;
        }

        eventStart += static_cast<size_t>((std::max(0.0, offsetMs) / 1000.0) *
                                          sampleRate_);
      }

      NoteEvent ev;
      ev.startSample = eventStart;
      ev.stringIndex = strIndex;
      ev.targetFreq = freqs[strIndex];
      ev.velocity = velocity;
      ev.durationSec = durationSec;
      ev.isDownStrum = isDownStrum;
      ev.isNewChord = isNewChord;
      ev.activeStrings = activeStrings;
      ev.gestureId = gestureId;
      ev.articulation = Articulation::Strum;
      events_.push_back(ev);
    }
  }

  void SchedulePluck(double startTimeSec, float velocity, double durationSec,
                     bool isNewChord, double f1, double f2, double f3,
                     double f4, double f5, double f6) {
    const double freqs[6] = {f1, f2, f3, f4, f5, f6};

    int activeStrings = 0;
    for (int i = 0; i < 6; ++i) {
      if (freqs[i] > 0.0)
        ++activeStrings;
    }
    if (activeStrings == 0)
      return;

    const uint32_t gestureId = nextGestureId_++;
    const size_t startSample =
        static_cast<size_t>(std::max(0.0, startTimeSec) * sampleRate_);

    for (int strIndex = 0; strIndex < 6; ++strIndex) {
      if (freqs[strIndex] <= 0.0)
        continue;

      NoteEvent ev;
      ev.startSample = startSample;
      ev.stringIndex = strIndex;
      ev.targetFreq = freqs[strIndex];
      ev.velocity = velocity;
      ev.durationSec = durationSec;
      ev.isDownStrum = true;
      ev.isNewChord = isNewChord;
      ev.activeStrings = activeStrings;
      ev.gestureId = gestureId;
      ev.articulation = Articulation::Pluck;
      events_.push_back(ev);
    }
  }

  std::vector<float> Render() {
    std::sort(events_.begin(), events_.end(),
              [](const NoteEvent &a, const NoteEvent &b) {
                if (a.startSample != b.startSample)
                  return a.startSample < b.startSample;
                return a.stringIndex < b.stringIndex;
              });

    std::vector<float> out(totalSamples_, 0.0f);
    size_t eventIndex = 0;

    for (size_t n = 0; n < totalSamples_; ++n) {
      while (eventIndex < events_.size() &&
             events_[eventIndex].startSample == n) {
        TriggerEvent(events_[eventIndex]);
        ++eventIndex;
      }

      float mix = 0.0f;
      for (int s = 0; s < 6; ++s) {
        for (auto &voice : stringVoices_[s])
          mix += voice.RenderOne();
      }

      out[n] = ClampFloat(mix, -1.0f, 1.0f);
    }

    return out;
  }

private:
  static constexpr size_t VOICES_PER_STRING = 3;

  void TriggerEvent(const NoteEvent &ev) {
    if (ev.stringIndex < 0 || ev.stringIndex >= 6 || ev.targetFreq <= 0.0)
      return;

    auto &voices = stringVoices_[ev.stringIndex];
    bool alreadyRinging = false;
    bool sameNoteRinging = false;

    for (const auto &v : voices) {
      if (v.IsAudiblyActive()) {
        alreadyRinging = true;
        if (std::abs(v.freq - ev.targetFreq) <= 0.1)
          sameNoteRinging = true;
      }
    }

    // Isolate soft retriggering to continuous strums; plucks demand clean
    // transients
    const bool softRetrigger =
        (ev.articulation == Articulation::Strum && !ev.isNewChord &&
         alreadyRinging && sameNoteRinging);

    const size_t quickRelease =
        softRetrigger ? MsToSamples(10.0) : MsToSamples(16.0);

    for (auto &v : voices) {
      if (v.IsAudiblyActive())
        v.StartRelease(quickRelease);
    }

    const double pitchRatio = ev.targetFreq / baseFreqs_[ev.stringIndex];
    if (pitchRatio <= 0.0)
      return;

    double readHeadStart = 0.0;
    float retriggerGain = 1.0f;
    size_t attackSamples = MsToSamples(4.0);

    if (softRetrigger) {
      readHeadStart = static_cast<double>(MsToSamples(12.0)) * pitchRatio;
      retriggerGain = 0.40f;
      attackSamples = MsToSamples(1.5);
    }

    const size_t requestedFrames =
        static_cast<size_t>(ev.durationSec * sampleRate_);
    if (requestedFrames == 0)
      return;

    float stringVelocity = ev.velocity * retriggerGain;

    if (ev.articulation == Articulation::Strum) {
      if (ev.isDownStrum) {
        stringVelocity *= (1.0f - static_cast<float>(ev.stringIndex) * 0.04f);
      } else {
        const float upPos = static_cast<float>(ev.stringIndex) / 5.0f;
        stringVelocity *= (0.30f + 0.70f * upPos);
      }
    } else {
      const float pos = static_cast<float>(ev.stringIndex) / 5.0f;
      stringVelocity *= (0.85f + 0.15f * pos);
    }

    float densityScale = 1.0f;
    if (ev.articulation == Articulation::Strum) {
      densityScale = ClampFloat(
          0.95f * std::sqrt(4.0f /
                            static_cast<float>(std::max(1, ev.activeStrings))),
          0.72f, 1.12f);
    }

    const float finalGain = 0.16f * stringVelocity * densityScale;

    StringVoice *targetVoice = nullptr;
    for (auto &v : voices) {
      if (!v.IsAudiblyActive() && !v.releasing) {
        targetVoice = &v;
        break;
      }
    }
    if (!targetVoice)
      targetVoice = &voices[0];

    targetVoice->Start(&anchors_[ev.stringIndex], ev.targetFreq, pitchRatio,
                       readHeadStart, requestedFrames, finalGain,
                       attackSamples);
  }

  size_t MsToSamples(double ms) const {
    return static_cast<size_t>((ms / 1000.0) * sampleRate_);
  }

  double sampleRate_ = 44100.0;
  size_t totalSamples_ = 0;
  std::array<std::vector<float>, 6> anchors_;
  std::array<double, 6> baseFreqs_{};
  std::vector<NoteEvent> events_;
  std::array<std::array<StringVoice, VOICES_PER_STRING>, 6> stringVoices_{};
  uint32_t nextGestureId_ = 1;
};

// -----------------------------------------------------
// Main
// -----------------------------------------------------
int main() {
  const double sampleRate = 44100.0;
  const double totalDurationSec = 26.0;
  const size_t totalSamples =
      static_cast<size_t>(totalDurationSec * sampleRate);

  GuitarSynthesizer synth(sampleRate, totalSamples);

  std::cout << "Loading 6 FSS Acoustic Guitar Anchors..." << std::endl;
  if (!synth.LoadAnchors({
          "Assets/Exciters/FS_Guitars/E2.wav",
          "Assets/Exciters/FS_Guitars/A2.wav",
          "Assets/Exciters/FS_Guitars/D3.wav",
          "Assets/Exciters/FS_Guitars/G3.wav",
          "Assets/Exciters/FS_Guitars/B3.wav",
          "Assets/Exciters/FS_Guitars/E4.wav",
      })) {
    std::cerr << "Failed to load one or more anchors." << std::endl;
    return 1;
  }

  auto playStrum = [&](double barStartSec, bool isNewChord, double f1,
                       double f2, double f3, double f4, double f5, double f6) {
    const double ring = 2.0;
    synth.ScheduleStrum(barStartSec + 0.00, true, 1.00f, ring, isNewChord, f1,
                        f2, f3, f4, f5, f6);
    synth.ScheduleStrum(barStartSec + 0.50, true, 0.85f, ring, false, f1, f2,
                        f3, f4, f5, f6);
    synth.ScheduleStrum(barStartSec + 0.75, false, 0.70f, ring, false, f1, f2,
                        f3, f4, f5, f6);
    synth.ScheduleStrum(barStartSec + 1.25, false, 0.75f, ring, false, f1, f2,
                        f3, f4, f5, f6);
    synth.ScheduleStrum(barStartSec + 1.50, true, 0.80f, ring, false, f1, f2,
                        f3, f4, f5, f6);
    synth.ScheduleStrum(barStartSec + 1.75, false, 0.65f, ring, false, f1, f2,
                        f3, f4, f5, f6);
  };

  auto playArpeggio = [&](double barStartSec, bool isNewChord, double f1,
                          double f2, double f3, double f4, double f5,
                          double f6) {
    const double step = 0.25;
    const double ring = 0.70;

    double primaryBass = 0.0;
    double altBass = 0.0;

    if (f1 > 0.0) {
      primaryBass = f1;
      altBass = (f2 > 0.0) ? f2 : ((f3 > 0.0) ? f3 : 0.0);
    } else if (f2 > 0.0) {
      primaryBass = f2;
      altBass = (f3 > 0.0) ? f3 : 0.0;
    } else if (f3 > 0.0) {
      primaryBass = f3;
      altBass = 0.0;
    }

    auto pluck = [&](double t, double fr1, double fr2, double fr3, double fr4,
                     double fr5, double fr6, float vel, bool nc) {
      synth.SchedulePluck(barStartSec + t, vel, ring, nc, fr1, fr2, fr3, fr4,
                          fr5, fr6);
    };

    pluck(0 * step, (primaryBass == f1) ? f1 : 0.0,
          (primaryBass == f2) ? f2 : 0.0, (primaryBass == f3) ? f3 : 0.0, 0.0,
          0.0, f6, 0.90f, isNewChord);

    pluck(1 * step, 0.0, 0.0, 0.0, f4, 0.0, 0.0, 0.55f, false);

    if (altBass > 0.0) {
      pluck(2 * step, (altBass == f1) ? f1 : 0.0, (altBass == f2) ? f2 : 0.0,
            (altBass == f3) ? f3 : 0.0, 0.0, 0.0, 0.0, 0.75f, false);
    } else {
      pluck(2 * step, 0.0, 0.0, 0.0, 0.0, f5, 0.0, 0.62f, false);
    }

    pluck(3 * step, 0.0, 0.0, 0.0, 0.0, f5, 0.0, 0.60f, false);

    pluck(4 * step, (primaryBass == f1) ? f1 : 0.0,
          (primaryBass == f2) ? f2 : 0.0, (primaryBass == f3) ? f3 : 0.0, 0.0,
          0.0, 0.0, 0.85f, false);

    pluck(5 * step, 0.0, 0.0, 0.0, f4, 0.0, 0.0, 0.55f, false);

    if (altBass > 0.0) {
      pluck(6 * step, (altBass == f1) ? f1 : 0.0, (altBass == f2) ? f2 : 0.0,
            (altBass == f3) ? f3 : 0.0, 0.0, 0.0, 0.0, 0.75f, false);
    } else {
      pluck(6 * step, 0.0, 0.0, 0.0, 0.0, f5, 0.0, 0.62f, false);
    }

    pluck(7 * step, 0.0, 0.0, 0.0, 0.0, 0.0, f6, 0.65f, false);
  };

  const double ca1 = 0.0, ca2 = 130.81, ca3 = 164.81, ca4 = 196.00,
               ca5 = 293.66, ca6 = 392.00;
  const double g1 = 98.00, g2 = 123.47, g3 = 146.83, g4 = 196.00, g5 = 293.66,
               g6 = 392.00;
  const double as1 = 0.0, as2 = 110.00, as3 = 164.81, as4 = 220.00,
               as5 = 246.94, as6 = 329.63;
  const double ds1 = 0.0, ds2 = 0.0, ds3 = 146.83, ds4 = 220.00, ds5 = 293.66,
               ds6 = 392.00;

  std::cout << "Strumming: Cadd9 - G - Asus2 - Dsus4 (120 BPM)" << std::endl;
  playStrum(0.0, true, ca1, ca2, ca3, ca4, ca5, ca6);
  playStrum(2.0, true, g1, g2, g3, g4, g5, g6);
  playStrum(4.0, true, as1, as2, as3, as4, as5, as6);
  playStrum(6.0, true, ds1, ds2, ds3, ds4, ds5, ds6);
  playStrum(8.0, true, ca1, ca2, ca3, ca4, ca5, ca6);
  playStrum(10.0, true, g1, g2, g3, g4, g5, g6);
  playStrum(12.0, true, as1, as2, as3, as4, as5, as6);
  playStrum(14.0, true, ds1, ds2, ds3, ds4, ds5, ds6);

  std::cout << "Arpeggios: Cadd9 - G - Asus2 - Dsus4" << std::endl;
  playArpeggio(16.0, true, ca1, ca2, ca3, ca4, ca5, ca6);
  playArpeggio(18.0, true, g1, g2, g3, g4, g5, g6);
  playArpeggio(20.0, true, as1, as2, as3, as4, as5, as6);
  playArpeggio(22.0, true, ds1, ds2, ds3, ds4, ds5, ds6);

  std::cout << "Rendering chronologically..." << std::endl;
  std::vector<float> sequence = synth.Render();

  std::cout << "Saving test_pecr_output.wav..." << std::endl;
  if (!SaveWav("test_pecr_output.wav", sequence, sampleRate)) {
    std::cerr << "Failed to save output WAV." << std::endl;
    return 1;
  }

  std::cout << "Done." << std::endl;
  return 0;
}
