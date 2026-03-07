import wave
import struct
import math
import os

def generate_sawtooth_wave(filename, frequency, duration_sec=2.0, sample_rate=44100):
    num_samples = int(duration_sec * sample_rate)
    
    # We'll write 16-bit PCM for simplicity, AudioFileReader handles Apple ExtAudioFile which reads anything to float
    with wave.open(filename, 'w') as wav_file:
        wav_file.setnchannels(2) # Stereo
        wav_file.setsampwidth(2) # 2 bytes = 16-bit
        wav_file.setframerate(sample_rate)
        
        # Generator for band-limited ish sawtooth (just raw math for now)
        period = sample_rate / frequency
        volume = 32767.0 * 0.5 # Max 16-bit int, half volume
        
        for i in range(num_samples):
            # Phase runs from -1 to 1 over the period
            phase = 2.0 * ((i % period) / period) - 1.0
            sample_val = int(volume * phase)
            
            # Simple envelope to prevent clicks
            if i < 441: # 10ms attack
                sample_val = int(sample_val * (i / 441.0))
            elif i > num_samples - 4410: # 100ms release
                sample_val = int(sample_val * ((num_samples - i) / 4410.0))
                
            data = struct.pack('<hh', sample_val, sample_val)
            wav_file.writeframesraw(data)

if __name__ == '__main__':
    # Generate C1 (36)
    generate_sawtooth_wave('assets/samples/bass_mock/C1.wav', 65.41)
    # Generate C2 (48)
    generate_sawtooth_wave('assets/samples/bass_mock/C2.wav', 130.81)
    # Generate C3 (60)
    generate_sawtooth_wave('assets/samples/bass_mock/C3.wav', 261.63)
    
    print("Generated 3 Bass waveform mock samples.")
