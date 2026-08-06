#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
#include <licensr/hwid.h>
#include <licensr/licensr.h>

#include <memory>
#include <string>

#include "SineSynth.h"

// The flagship integration test and launch asset for the C++ SDK: a real,
// open-source VST3/AU/Standalone synth whose only "feature" beyond making
// sound is exercising licensr end to end — activate(), validate(), the
// audio-thread-safe snapshot(), and a stable per-machine identifier via
// licensr::compute_hardware_id. See README.md for the demo's license story.
class LicensrDemoSynthAudioProcessor : public juce::AudioProcessor {
 public:
  // How long the demo plays before muting, and for how long, once
  // isFullyLicensed() is false. Public so the editor can describe the
  // limitation without duplicating the numbers.
  static constexpr double kAudibleSeconds = 10.0;
  static constexpr double kMuteSeconds = 5.0;

  LicensrDemoSynthAudioProcessor();
  ~LicensrDemoSynthAudioProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "Licensr Demo Synth"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  // No plugin state worth persisting beyond the license key itself, which
  // is already saved to its own settings file (see persistLicenseKey).
  void getStateInformation(juce::MemoryBlock&) override {}
  void setStateInformation(const void*, int) override {}

  // Lets the editor drive an on-screen keyboard without reaching into
  // internals: shared with the Synthesiser via processNextMidiBuffer.
  juce::MidiKeyboardState& keyboardState() { return keyboardState_; }

  std::string persistedLicenseKey() const;

  // Registers a seat for `licenseKeyInput` against this machine's hardware
  // ID, then validates it once more so client().snapshot() (read by
  // processBlock on the audio thread) reflects the new license immediately
  // instead of waiting for the next launch. `callback` always runs on the
  // message thread, exactly once, whether this succeeds or fails.
  void activateLicense(const juce::String& licenseKeyInput,
                       std::function<void(bool succeeded, juce::String statusText)> callback);

  // True once a successful validate() has populated client().snapshot()
  // with a currently-valid license. Safe to call from the audio thread.
  bool isFullyLicensed() const;

 private:
  void persistLicenseKey(const juce::String& licenseKey);
  void revalidatePersistedLicense();

  std::string hardwareId_;
  licensr::ClientConfig config_;
  licensr::Client client_;
  std::unique_ptr<juce::PropertiesFile> settings_;

  juce::Synthesiser synth_;
  juce::MidiKeyboardState keyboardState_;

  // Drives the demo's periodic mute gate in processBlock; reset whenever
  // isFullyLicensed() so a license activated mid-session takes effect on
  // the very next block rather than finishing out a stale mute window.
  juce::int64 samplesProcessed_ = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicensrDemoSynthAudioProcessor)
  JUCE_DECLARE_WEAK_REFERENCEABLE(LicensrDemoSynthAudioProcessor)
};
