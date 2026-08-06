#include "LicenseActivationComponent.h"

namespace licensr {

LicenseActivationComponent::LicenseActivationComponent(ClientConfig config, std::string deviceId)
    : client_(std::move(config)), deviceId_(std::move(deviceId)) {
  promptLabel_.setText("License key", juce::dontSendNotification);
  addAndMakeVisible(promptLabel_);

  licenseKeyEditor_.setTextToShowWhenEmpty("Enter your license key", juce::Colours::grey);
  addAndMakeVisible(licenseKeyEditor_);

  activateButton_.setButtonText("Activate");
  activateButton_.onClick = [this] { handleActivateClicked(); };
  addAndMakeVisible(activateButton_);

  statusLabel_.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(statusLabel_);
}

LicenseActivationComponent::~LicenseActivationComponent() = default;

void LicenseActivationComponent::paint(juce::Graphics& g) {
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void LicenseActivationComponent::resized() {
  auto area = getLocalBounds().reduced(8);
  promptLabel_.setBounds(area.removeFromTop(20));

  auto row = area.removeFromTop(28);
  activateButton_.setBounds(row.removeFromRight(90));
  row.removeFromRight(8);
  licenseKeyEditor_.setBounds(row);

  area.removeFromTop(8);
  statusLabel_.setBounds(area.removeFromTop(20));
}

void LicenseActivationComponent::handleActivateClicked() {
  auto licenseKey = licenseKeyEditor_.getText().trim();
  if (licenseKey.isEmpty()) {
    setStatus("Enter a license key first.", true);
    return;
  }

  activateButton_.setEnabled(false);
  setStatus("Activating...", false);

  ActivateRequest request;
  request.license_key = licenseKey.toStdString();
  request.activation_type = ActivationType::kSeat;
  request.identifier = deviceId_;

  juce::Component::SafePointer<LicenseActivationComponent> safeThis(this);
  client_.activate(request, [safeThis](Result<ActivateResult> activateResult) {
    juce::MessageManager::callAsync([safeThis, result = std::move(activateResult)]() mutable {
      auto* self = safeThis.getComponent();
      if (self == nullptr) return;  // Destroyed (e.g. editor closed) before this ran.

      self->activateButton_.setEnabled(true);
      if (!result.ok()) {
        self->isActivated_ = false;
        self->setStatus(juce::String(result.error().message), true);
        if (self->onError) self->onError(result.error());
        return;
      }

      self->isActivated_ = true;
      self->setStatus("License activated.", false);
      if (self->onActivated) self->onActivated(result.value());
    });
  });
}

void LicenseActivationComponent::setStatus(const juce::String& text, bool isError) {
  statusLabel_.setText(text, juce::dontSendNotification);
  statusLabel_.setColour(juce::Label::textColourId, isError ? juce::Colours::red : juce::Colours::green.darker());
}

}  // namespace licensr
