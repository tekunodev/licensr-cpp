#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace licensr_demo {

// Accepts every note/channel — this demo doesn't need per-sound filtering.
class SineSound : public juce::SynthesiserSound {
 public:
  bool appliesToNote(int) override { return true; }
  bool appliesToChannel(int) override { return true; }
};

// A minimal polyphonic sine-wave voice with a linear attack/release, just
// enough to make the demo's activation flow audible without pulling in a
// real synthesis engine — this plugin exists to exercise licensr, not to
// be a good instrument.
class SineVoice : public juce::SynthesiserVoice {
 public:
  bool canPlaySound(juce::SynthesiserSound* sound) override { return dynamic_cast<SineSound*>(sound) != nullptr; }

  void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int /*pitchWheel*/) override {
    currentAngle_ = 0.0;
    level_ = velocity * 0.2;
    tailOff_ = 0.0;
    auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    angleDelta_ = cyclesPerSecond / getSampleRate() * juce::MathConstants<double>::twoPi;
  }

  void stopNote(float /*velocity*/, bool allowTailOff) override {
    if (allowTailOff) {
      if (tailOff_ == 0.0) tailOff_ = 1.0;
    } else {
      clearCurrentNote();
      angleDelta_ = 0.0;
    }
  }

  void pitchWheelMoved(int) override {}
  void controllerMoved(int, int) override {}

  void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override {
    if (angleDelta_ == 0.0) return;

    while (--numSamples >= 0) {
      auto currentSample = static_cast<float>(std::sin(currentAngle_) * level_ * (tailOff_ > 0.0 ? tailOff_ : 1.0));
      for (auto channel = outputBuffer.getNumChannels(); --channel >= 0;)
        outputBuffer.addSample(channel, startSample, currentSample);

      currentAngle_ += angleDelta_;
      ++startSample;

      if (tailOff_ > 0.0) {
        tailOff_ *= 0.99;
        if (tailOff_ <= 0.005) {
          clearCurrentNote();
          angleDelta_ = 0.0;
          break;
        }
      }
    }
  }

 private:
  double currentAngle_ = 0.0;
  double angleDelta_ = 0.0;
  double level_ = 0.0;
  double tailOff_ = 0.0;
};

}  // namespace licensr_demo
