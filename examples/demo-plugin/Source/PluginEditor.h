#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

// A compact activation UI plus an on-screen keyboard so the demo is
// playable without a MIDI controller. Deliberately hand-rolled directly on
// top of licensr::Client rather than the optional licensr_juce module's
// LicenseActivationComponent: this editor needs to persist the entered
// license key itself (see LicensrDemoSynthAudioProcessor::activateLicense),
// which that simpler drop-in component doesn't expose — see
// sdks/cpp/juce/README.md for when it's the better fit than hand-rolling.
class LicensrDemoSynthAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit LicensrDemoSynthAudioProcessorEditor(LicensrDemoSynthAudioProcessor& processor);
  ~LicensrDemoSynthAudioProcessorEditor() override;

  void paint(juce::Graphics& g) override;
  void resized() override;

 private:
  void timerCallback() override;
  void handleActivateClicked();
  void refreshLicenseStatusLabel();

  LicensrDemoSynthAudioProcessor& processor_;

  juce::Label titleLabel_;
  juce::Label licensePromptLabel_;
  juce::TextEditor licenseKeyEditor_;
  juce::TextButton activateButton_;
  juce::Label licenseStatusLabel_;
  juce::MidiKeyboardComponent keyboard_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicensrDemoSynthAudioProcessorEditor)
};
