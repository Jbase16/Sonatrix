#include <Accelerate/Accelerate.h>
#include <AudioToolbox/AudioToolbox.h>
#include <cmath>
#include <iostream>
#include <vector>

// Simple WAV reader logic
bool LoadWav(const std::string &path, std::vector<float> &outData,
             double &outSampleRate) {
  CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(),
                                              kCFStringEncodingUTF8);
  CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, str,
                                               kCFURLPOSIXPathStyle, false);
  CFRelease(str);

  ExtAudioFileRef audioFile;
  if (ExtAudioFileOpenURL(url, &audioFile) != noErr) {
    CFRelease(url);
    return false;
  }
  CFRelease(url);

  AudioStreamBasicDescription clientFormat = {};
  clientFormat.mSampleRate = 44100.0;
  clientFormat.mFormatID = kAudioFormatLinearPCM;
  clientFormat.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  clientFormat.mBytesPerPacket = 4;
  clientFormat.mFramesPerPacket = 1;
  clientFormat.mBytesPerFrame = 4;
  clientFormat.mChannelsPerFrame = 1;
  clientFormat.mBitsPerChannel = 32;

  ExtAudioFileSetProperty(audioFile, kExtAudioFileProperty_ClientDataFormat,
                          sizeof(clientFormat), &clientFormat);

  SInt64 totalFrames = 0;
  UInt32 size = sizeof(totalFrames);
  ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileLengthFrames,
                          &size, &totalFrames);

  outData.resize(totalFrames);
  AudioBufferList bufferList = {};
  bufferList.mNumberBuffers = 1;
  bufferList.mBuffers[0].mNumberChannels = 1;
  bufferList.mBuffers[0].mDataByteSize = totalFrames * sizeof(float);
  bufferList.mBuffers[0].mData = outData.data();

  UInt32 ioFrames = totalFrames;
  ExtAudioFileRead(audioFile, &ioFrames, &bufferList);
  ExtAudioFileDispose(audioFile);
  outSampleRate = 44100.0;
  return true;
}

// Simple WAV writer logic
bool SaveWav(const std::string &path, const std::vector<float> &data,
             double sampleRate) {
  CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(),
                                              kCFStringEncodingUTF8);
  CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, str,
                                               kCFURLPOSIXPathStyle, false);
  CFRelease(str);

  AudioStreamBasicDescription format = {};
  format.mSampleRate = sampleRate;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags =
      kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  format.mBytesPerPacket = 2;
  format.mFramesPerPacket = 1;
  format.mBytesPerFrame = 2;
  format.mChannelsPerFrame = 1;
  format.mBitsPerChannel = 16;

  ExtAudioFileRef audioFile;
  if (ExtAudioFileCreateWithURL(url, kAudioFileWAVEType, &format, nullptr,
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
  clientFormat.mBytesPerPacket = 4;
  clientFormat.mFramesPerPacket = 1;
  clientFormat.mBytesPerFrame = 4;
  clientFormat.mChannelsPerFrame = 1;
  clientFormat.mBitsPerChannel = 32;

  ExtAudioFileSetProperty(audioFile, kExtAudioFileProperty_ClientDataFormat,
                          sizeof(clientFormat), &clientFormat);

  AudioBufferList bufferList = {};
  bufferList.mNumberBuffers = 1;
  bufferList.mBuffers[0].mNumberChannels = 1;
  bufferList.mBuffers[0].mDataByteSize = data.size() * sizeof(float);
  bufferList.mBuffers[0].mData = (void *)data.data();

  ExtAudioFileWrite(audioFile, data.size(), &bufferList);
  ExtAudioFileDispose(audioFile);
  return true;
}

// -----------------------------------------------------
// Phase 14: Direct Multi-Sampler Pitch Shifting
// -----------------------------------------------------
// Since we have an anchor sample for every single string, the required pitch
// shift is extremely small (max ~4 semitones). Therefore, standard linear
// interpolation sounds flawless and avoids all Granular/PSOLA phase
// cancellation artifacts.
std::vector<float> GenerateSampledNote(const std::vector<float> &srcAudio,
                                       double targetFreq, double sampleRate,
                                       double baseFreq) {
  // Determine the pitch ratio
  double ratio = targetFreq / baseFreq;

  // We adjust the length of the output buffer based on the pitch shift
  // (Pitching up makes the sample shorter, pitching down makes it longer)
  size_t newLen = static_cast<size_t>(srcAudio.size() / ratio);
  std::vector<float> out(newLen, 0.0f);

  // Standard Linear Interpolation Playback
  for (size_t i = 0; i < newLen; ++i) {
    double readPos = i * ratio;
    size_t idx = static_cast<size_t>(readPos);
    double frac = readPos - idx;

    float val1 = (idx < srcAudio.size()) ? srcAudio[idx] : 0.0f;
    float val2 = (idx + 1 < srcAudio.size()) ? srcAudio[idx + 1] : 0.0f;

    out[i] = val1 + (val2 - val1) * static_cast<float>(frac);
  }

  // Apply a quick fade out to prevent clicks at the end of the sample if it was
  // cut short
  size_t fadeSamples = std::min(static_cast<size_t>(1000), newLen);
  for (size_t i = 0; i < fadeSamples; ++i) {
    float env = static_cast<float>(fadeSamples - i) / fadeSamples;
    out[newLen - fadeSamples + i] *= env;
  }

  // Hard mute after 1.5 seconds to prevent extremely down-pitched bass notes
  // from ringing forever and muddying up subsequent fast arpeggio picking.
  size_t maxLen = static_cast<size_t>(1.5 * sampleRate);
  if (out.size() > maxLen) {
    size_t hardFade = std::min(static_cast<size_t>(2000), maxLen);
    for (size_t i = maxLen - hardFade; i < maxLen; ++i) {
      float env = static_cast<float>(maxLen - i) / hardFade;
      out[i] *= env;
    }
    out.resize(maxLen);
  }
  return out;
}

int main() {
  double sampleRate = 44100.0;

  std::cout << "Loading 6 FSS Acoustic Guitar Anchors..." << std::endl;
  std::vector<std::vector<float>> anchors(6);
  if (!LoadWav("Assets/Exciters/FS_Guitars/E2.wav", anchors[0], sampleRate) ||
      !LoadWav("Assets/Exciters/FS_Guitars/A2.wav", anchors[1], sampleRate) ||
      !LoadWav("Assets/Exciters/FS_Guitars/D3.wav", anchors[2], sampleRate) ||
      !LoadWav("Assets/Exciters/FS_Guitars/G3.wav", anchors[3], sampleRate) ||
      !LoadWav("Assets/Exciters/FS_Guitars/B3.wav", anchors[4], sampleRate) ||
      !LoadWav("Assets/Exciters/FS_Guitars/E4.wav", anchors[5], sampleRate)) {
    std::cerr << "Failed to load multi-sample anchors!" << std::endl;
    return 1;
  }

  // Base frequencies of the open strings we sampled
  std::vector<double> baseFreqs = {82.41,  110.00, 146.83,
                                   196.00, 246.94, 329.63};

  std::cout << "Synthesizing G-D-Em-C progression..." << std::endl;

  // Per-string state tracking for the voice manager
  std::vector<size_t> stringWritePos(6, 0);
  std::vector<size_t> stringEndPos(6, 0);
  std::vector<double> stringCurrentFreq(6, 0.0);

  // -----------------------------------------------------------------------
  // addStrum: the core DSP stroke function.
  //
  // Incorporates all 5 fixes from the peer-review:
  //  1. Relative strum origin  — delay is relative to first active string, not
  //  abs index
  //  2. Humanized sweep        — base delay 12ms, nonlinear, with subtle
  //  per-string jitter
  //  3. Sustain skip           — on isNewChord, a shared-freq string is NOT
  //  retriggered
  //  4. Soft retrigger         — same-chord restrum skips the transient, plays
  //  at 0.4x gain
  //  5. Attack ramp            — 4ms linear ramp-in prevents click stacking on
  //  close strums
  // -----------------------------------------------------------------------
  auto addStrum = [&](std::vector<float> &dest, double startTimeSec,
                      bool isDownStrum, float velocity, double durationSec,
                      bool isNewChord, double f1, double f2, double f3,
                      double f4, double f5, double f6) {
    const double freqs[6] = {f1, f2, f3, f4, f5, f6};

    // Find the first (and last) active string for relative delay calculation
    int firstActive = -1, lastActive = -1;
    for (int i = 0; i < 6; ++i) {
      if (freqs[i] > 0) {
        if (firstActive < 0)
          firstActive = i;
        lastActive = i;
      }
    }
    if (firstActive < 0)
      return;

    int activeStrings = 0;
    for (int i = 0; i < 6; ++i)
      if (freqs[i] > 0)
        ++activeStrings;

    // Pre-generate all string samples
    std::vector<float> samples[6];
    for (int i = 0; i < 6; ++i) {
      if (freqs[i] > 0)
        samples[i] =
            GenerateSampledNote(anchors[i], freqs[i], sampleRate, baseFreqs[i]);
    }

    // Fix 2: humanized base delay (12ms down, 9ms up)
    double baseDelayMs = isDownStrum ? 12.0 : 9.0;

    for (int strIndex = 0; strIndex < 6; ++strIndex) {
      double targetFreq = freqs[strIndex];
      const std::vector<float> &stringAudio = samples[strIndex];

      if (stringAudio.empty() || targetFreq <= 0.0)
        continue;

      size_t startOffset = static_cast<size_t>(startTimeSec * sampleRate);

      // Fix 1: Relative strum origin
      size_t strOffset;
      if (activeStrings == 1) {
        // Arpeggio single-string: no sweep delay, lock to grid
        strOffset = startOffset;
      } else {
        int origin = isDownStrum ? firstActive : lastActive;
        int relIdx = isDownStrum ? (strIndex - origin) : (origin - strIndex);
        if (relIdx < 0)
          relIdx = 0;

        // Nonlinear acceleration: each subsequent string is slightly faster
        // (~8%)
        double effectiveDelayMs = 0.0;
        for (int k = 0; k < relIdx; ++k)
          effectiveDelayMs += baseDelayMs * std::pow(0.92, k);

        // Subtle jitter: alternating ±0.8ms to avoid robotic pattern
        effectiveDelayMs += (strIndex % 2 == 0) ? -0.8 : 0.8;

        strOffset = startOffset + static_cast<size_t>(
                                      (effectiveDelayMs / 1000.0) * sampleRate);
      }

      size_t currentEnd = stringEndPos[strIndex];
      bool alreadyRinging = strOffset < currentEnd;
      bool sameNote = std::abs(stringCurrentFreq[strIndex] - targetFreq) < 0.1;

      // Fix 3: True harmonic sustain skip on chord transitions
      // If the string is ringing the same note and we just changed chords, skip
      // it entirely.
      if (isNewChord && alreadyRinging && sameNote && activeStrings > 2) {
        stringCurrentFreq[strIndex] = targetFreq;
        continue;
      }

      // Fix 4: Soft retrigger on repeated same-chord strums
      // Skip the transient (~12ms into sample) and play at 40% gain
      bool softRetrigger = !isNewChord && alreadyRinging && sameNote;
      size_t transientSkip =
          softRetrigger ? static_cast<size_t>(0.012 * sampleRate) : 0;
      float retriggerGain = softRetrigger ? 0.4f : 1.0f;

      if (transientSkip >= stringAudio.size())
        continue;

      // Velocity shaping
      float stringVelocity = velocity * retriggerGain;
      if (isDownStrum) {
        stringVelocity *= (1.0f - (strIndex * 0.04f));
      } else {
        // Upstrokes favor treble; bass strings are naturally softer
        float upPos = static_cast<float>(5 - strIndex) / 5.0f;
        stringVelocity *= (0.3f + 0.7f * upPos);
      }

      size_t available = stringAudio.size() - transientSkip;
      size_t maxWriteLen =
          std::min(available, static_cast<size_t>(durationSec * sampleRate));
      if (maxWriteLen == 0)
        continue;

      // Fix 5: Attack ramp (4ms) + tail release
      size_t attackLen =
          std::min(static_cast<size_t>(0.004 * sampleRate), maxWriteLen / 8);
      size_t fadeLen =
          std::min(static_cast<size_t>(0.08 * sampleRate), maxWriteLen / 4);

      for (size_t i = 0; i < maxWriteLen; ++i) {
        if (strOffset + i >= dest.size())
          break;

        float env = 1.0f;
        if (i < attackLen)
          env *= static_cast<float>(i) /
                 static_cast<float>(std::max<size_t>(1, attackLen));
        if (i >= maxWriteLen - fadeLen)
          env *= static_cast<float>(maxWriteLen - i) /
                 static_cast<float>(std::max<size_t>(1, fadeLen));

        dest[strOffset + i] +=
            stringAudio[transientSkip + i] * 0.15f * stringVelocity * env;
      }

      stringWritePos[strIndex] = strOffset;
      stringEndPos[strIndex] = strOffset + maxWriteLen;
      stringCurrentFreq[strIndex] = targetFreq;
    }
  };

  // 25 seconds total playback buffer
  size_t totalPlayTimeSamples = static_cast<size_t>(25.0 * sampleRate);
  std::vector<float> sequence(totalPlayTimeSamples, 0.0f);

  // -----------------------------------------------------------------------
  // Strum pattern at 100 BPM: bar = 2.4s
  // -----------------------------------------------------------------------
  auto playStrum = [&](double barStartSec, double f1, double f2, double f3,
                       double f4, double f5, double f6) {
    double ring = 2.4;
    addStrum(sequence, barStartSec + 0.00, true, 1.0f, ring, true, f1, f2, f3,
             f4, f5, f6);
    addStrum(sequence, barStartSec + 0.60, true, 0.8f, ring, false, f1, f2, f3,
             f4, f5, f6);
    addStrum(sequence, barStartSec + 0.90, false, 0.6f, ring, false, f1, f2, f3,
             f4, f5, f6);
    addStrum(sequence, barStartSec + 1.50, false, 0.7f, ring, false, f1, f2, f3,
             f4, f5, f6);
    addStrum(sequence, barStartSec + 1.80, true, 0.9f, ring, false, f1, f2, f3,
             f4, f5, f6);
    addStrum(sequence, barStartSec + 2.10, false, 0.6f, ring, false, f1, f2, f3,
             f4, f5, f6);
  };

  // -----------------------------------------------------------------------
  // Travis picking arpeggio: Bass -> Treble -> Mid in 0.30s steps (100 BPM
  // 1/8th)
  // -----------------------------------------------------------------------
  auto playArpeggio = [&](double barStartSec, double f1, double f2, double f3,
                          double f4, double f5, double f6) {
    double step = 0.30;
    double ring = 1.2;
    // Bass, High-E, G, B, D, High-E, B, G
    if (f1 > 0)
      addStrum(sequence, barStartSec + (0 * step), true, 1.0f, ring, true, f1,
               0, 0, 0, 0, 0);
    else
      addStrum(sequence, barStartSec + (0 * step), true, 1.0f, ring, true, 0,
               f2, 0, 0, 0, 0);
    if (f6 > 0)
      addStrum(sequence, barStartSec + (1 * step), false, 0.7f, ring, true, 0,
               0, 0, 0, 0, f6);
    if (f4 > 0)
      addStrum(sequence, barStartSec + (2 * step), true, 0.8f, ring, true, 0, 0,
               0, f4, 0, 0);
    if (f5 > 0)
      addStrum(sequence, barStartSec + (3 * step), false, 0.6f, ring, true, 0,
               0, 0, 0, f5, 0);
    if (f3 > 0)
      addStrum(sequence, barStartSec + (4 * step), true, 0.9f, ring, true, 0, 0,
               f3, 0, 0, 0);
    if (f6 > 0)
      addStrum(sequence, barStartSec + (5 * step), false, 0.7f, ring, true, 0,
               0, 0, 0, 0, f6);
    if (f5 > 0)
      addStrum(sequence, barStartSec + (6 * step), false, 0.6f, ring, true, 0,
               0, 0, 0, f5, 0);
    if (f4 > 0)
      addStrum(sequence, barStartSec + (7 * step), false, 0.5f, ring, true, 0,
               0, 0, f4, 0, 0);
  };

  // -----------------------------------------------------------------------
  // Chord definitions — standard tuning, tablature-accurate
  // -----------------------------------------------------------------------
  // G Major: 320033 -> G2, B2, D3, G3, D4, G4
  double g1 = 98.00, g2 = 123.47, g3 = 146.83, g4 = 196.00, g5 = 293.66,
         g6 = 392.00;
  // D Major: xx0232 -> D3, A3, D4, F#4
  double d1 = 0, d2 = 0, d3 = 146.83, d4 = 220.00, d5 = 293.66, d6 = 369.99;
  // E Minor: 022000 -> E2, B2, E3, G3, B3, E4
  double em1 = 82.41, em2 = 123.47, em3 = 164.81, em4 = 196.00, em5 = 246.94,
         em6 = 329.63;
  // C Major: x32010 -> C3, E3, G3, C4, E4
  double cm1 = 0, cm2 = 130.81, cm3 = 164.81, cm4 = 196.00, cm5 = 261.63,
         cm6 = 329.63;

  std::cout << "Strumming: G - D - Em - C" << std::endl;
  playStrum(0.0, g1, g2, g3, g4, g5, g6);
  playStrum(2.4, d1, d2, d3, d4, d5, d6);
  playStrum(4.8, em1, em2, em3, em4, em5, em6);
  playStrum(7.2, cm1, cm2, cm3, cm4, cm5, cm6);

  std::cout << "Arpeggios: G - D - Em - C" << std::endl;
  playArpeggio(9.6, g1, g2, g3, g4, g5, g6);
  playArpeggio(12.0, d1, d2, d3, d4, d5, d6);
  playArpeggio(14.4, em1, em2, em3, em4, em5, em6);
  playArpeggio(16.8, cm1, cm2, cm3, cm4, cm5, cm6);

  std::cout << "Saving test_pecr_output.wav..." << std::endl;
  SaveWav("test_pecr_output.wav", sequence, sampleRate);
  std::cout << "Done." << std::endl;
  return 0;
}
