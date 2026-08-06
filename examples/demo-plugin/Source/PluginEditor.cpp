#include "PluginEditor.h"

LicensrDemoSynthAudioProcessorEditor::LicensrDemoSynthAudioProcessorEditor(LicensrDemoSynthAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      processor_(processor),
      keyboard_(processor.keyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard) {
  titleLabel_.setText("Licensr Demo Synth", juce::dontSendNotification);
  titleLabel_.setFont(juce::Font(20.0f, juce::Font::bold));
  titleLabel_.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(titleLabel_);

  licensePromptLabel_.setText("License key", juce::dontSendNotification);
  addAndMakeVisible(licensePromptLabel_);

  licenseKeyEditor_.setText(juce::String(processor_.persistedLicenseKey()), juce::dontSendNotification);
  licenseKeyEditor_.setTextToShowWhenEmpty("Enter your license key", juce::Colours::grey);
  addAndMakeVisible(licenseKeyEditor_);

  activateButton_.setButtonText("Activate");
  activateButton_.onClick = [this] { handleActivateClicked(); };
  addAndMakeVisible(activateButton_);

  licenseStatusLabel_.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(licenseStatusLabel_);
  refreshLicenseStatusLabel();

  addAndMakeVisible(keyboard_);

  setSize(480, 300);
  startTimer(500);
}

LicensrDemoSynthAudioProcessorEditor::~LicensrDemoSynthAudioProcessorEditor() { stopTimer(); }

void LicensrDemoSynthAudioProcessorEditor::paint(juce::Graphics& g) {
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void LicensrDemoSynthAudioProcessorEditor::resized() {
  auto area = getLocalBounds().reduced(12);
  titleLabel_.setBounds(area.removeFromTop(28));
  area.removeFromTop(8);

  licensePromptLabel_.setBounds(area.removeFromTop(18));
  auto row = area.removeFromTop(28);
  activateButton_.setBounds(row.removeFromRight(90));
  row.removeFromRight(8);
  licenseKeyEditor_.setBounds(row);

  area.removeFromTop(6);
  licenseStatusLabel_.setBounds(area.removeFromTop(36));

  area.removeFromTop(8);
  keyboard_.setBounds(area.removeFromBottom(80));
}

void LicensrDemoSynthAudioProcessorEditor::timerCallback() { refreshLicenseStatusLabel(); }

void LicensrDemoSynthAudioProcessorEditor::refreshLicenseStatusLabel() {
  if (processor_.isFullyLicensed()) {
    licenseStatusLabel_.setText("Licensed - play as long as you like.", juce::dontSendNotification);
    licenseStatusLabel_.setColour(juce::Label::textColourId, juce::Colours::green.darker());
    return;
  }

  licenseStatusLabel_.setText(
      "Demo mode - audio mutes for " + juce::String((int)LicensrDemoSynthAudioProcessor::kMuteSeconds) + "s every " +
          juce::String((int)LicensrDemoSynthAudioProcessor::kAudibleSeconds) + "s. Activate to remove the limitation.",
      juce::dontSendNotification);
  licenseStatusLabel_.setColour(juce::Label::textColourId, juce::Colours::orange.darker());
}

void LicensrDemoSynthAudioProcessorEditor::handleActivateClicked() {
  activateButton_.setEnabled(false);
  licenseStatusLabel_.setText("Activating...", juce::dontSendNotification);
  licenseStatusLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);

  juce::Component::SafePointer<LicensrDemoSynthAudioProcessorEditor> safeThis(this);
  processor_.activateLicense(licenseKeyEditor_.getText(), [safeThis](bool succeeded, juce::String statusText) {
    auto* self = safeThis.getComponent();
    if (self == nullptr) return;  // Editor closed while the request was in flight.

    self->activateButton_.setEnabled(true);
    self->licenseStatusLabel_.setText(statusText, juce::dontSendNotification);
    self->licenseStatusLabel_.setColour(juce::Label::textColourId,
                                        succeeded ? juce::Colours::green.darker() : juce::Colours::red);
  });
}
