#include "PluginProcessor.h"

#include <cstdlib>

#include "PluginEditor.h"

namespace {

// Overridable at runtime (not compile time) so contributors can point the
// demo at their own Licensr account or a local/staging backend without
// rebuilding: `LICENSR_DEMO_API_KEY=pk_test_... LICENSR_DEMO_PLUGIN_SLUG=my-plugin
// LICENSR_DEMO_BASE_URL=http://localhost:8000 ./LicensrDemoSynth`. The
// built-in defaults don't point at a real plugin, so an unconfigured build
// only ever exercises the network-error path — which is still a legitimate
// (if boring) integration smoke test.
std::string env_or(const char* name, std::string fallback) {
  if (const char* value = std::getenv(name)) return std::string(value);
  return fallback;
}

std::string compute_stable_identifier(const std::string& salt) {
  if (auto hwid = licensr::compute_hardware_id(salt)) return *hwid;
  // No stable hardware identifier available (sandboxed build, restricted
  // permissions) — mint a random one for this process's lifetime. A real
  // integration should persist this once generated instead of reminting
  // it every launch, or every launch looks like a new machine to the
  // license API and burns through the plan's seat count.
  return juce::Uuid().toString().toStdString();
}

licensr::ClientConfig make_config(const std::string& deviceId) {
  licensr::ClientConfig config;
  config.api_key = env_or("LICENSR_DEMO_API_KEY", "pk_test_REPLACE_ME");
  config.plugin_slug = env_or("LICENSR_DEMO_PLUGIN_SLUG", "licensr-demo-synth");
  config.base_url = env_or("LICENSR_DEMO_BASE_URL", "https://api.licensr.app");
  config.device_id = deviceId;
  return config;
}

}  // namespace

LicensrDemoSynthAudioProcessor::LicensrDemoSynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      hardwareId_(compute_stable_identifier(env_or("LICENSR_DEMO_PLUGIN_SLUG", "licensr-demo-synth"))),
      config_(make_config(hardwareId_)),
      client_(config_) {
  synth_.clearVoices();
  for (int i = 0; i < 8; ++i) synth_.addVoice(new licensr_demo::SineVoice());
  synth_.clearSounds();
  synth_.addSound(new licensr_demo::SineSound());

  juce::PropertiesFile::Options options;
  options.applicationName = "LicensrDemoSynth";
  options.filenameSuffix = "settings";
  options.osxLibrarySubFolder = "Application Support";
  settings_ = std::make_unique<juce::PropertiesFile>(options);

  revalidatePersistedLicense();
}

void LicensrDemoSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  juce::ignoreUnused(samplesPerBlock);
  synth_.setCurrentPlaybackSampleRate(sampleRate);
  samplesProcessed_ = 0;
}

void LicensrDemoSynthAudioProcessor::releaseResources() {}

bool LicensrDemoSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono() ||
         layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LicensrDemoSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  keyboardState_.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

  buffer.clear();
  synth_.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

  if (isFullyLicensed()) {
    samplesProcessed_ = 0;  // Pin the gate at "audible" so it doesn't resume mid-mute if unlicensed again.
    return;
  }

  const auto sampleRate = getSampleRate();
  const auto cycleLength = static_cast<juce::int64>((kAudibleSeconds + kMuteSeconds) * sampleRate);
  const auto audibleLength = static_cast<juce::int64>(kAudibleSeconds * sampleRate);
  if (cycleLength <= 0) return;  // Sample rate not established yet.

  for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
    const auto positionInCycle = (samplesProcessed_ + sample) % cycleLength;
    if (positionInCycle >= audibleLength) {
      for (int channel = 0; channel < buffer.getNumChannels(); ++channel) buffer.setSample(channel, sample, 0.0f);
    }
  }
  samplesProcessed_ += buffer.getNumSamples();
}

juce::AudioProcessorEditor* LicensrDemoSynthAudioProcessor::createEditor() {
  return new LicensrDemoSynthAudioProcessorEditor(*this);
}

std::string LicensrDemoSynthAudioProcessor::persistedLicenseKey() const {
  return settings_->getValue("licenseKey").toStdString();
}

void LicensrDemoSynthAudioProcessor::persistLicenseKey(const juce::String& licenseKey) {
  settings_->setValue("licenseKey", licenseKey);
  settings_->saveIfNeeded();
}

void LicensrDemoSynthAudioProcessor::revalidatePersistedLicense() {
  auto key = persistedLicenseKey();
  if (key.empty()) return;

  client_.validate(key, [](licensr::Result<licensr::ValidateResult>) {
    // Nothing to notify at construction time — client().snapshot() already
    // reflects the result for processBlock's demo gate to read.
  });
}

void LicensrDemoSynthAudioProcessor::activateLicense(
    const juce::String& licenseKeyInput, std::function<void(bool succeeded, juce::String statusText)> callback) {
  auto key = licenseKeyInput.trim().toStdString();
  if (key.empty()) {
    callback(false, "Enter a license key first.");
    return;
  }

  licensr::ActivateRequest request;
  request.license_key = key;
  request.activation_type = licensr::ActivationType::kSeat;
  request.identifier = hardwareId_;
  request.label = juce::SystemStats::getComputerName().toStdString();

  juce::WeakReference<LicensrDemoSynthAudioProcessor> weakThis(this);

  client_.activate(request, [weakThis, key, callback](licensr::Result<licensr::ActivateResult> activateResult) {
    if (weakThis.get() == nullptr) return;  // Plugin unloaded while the request was in flight.

    if (!activateResult.ok()) {
      auto message = activateResult.error().message;
      juce::MessageManager::callAsync([callback, message] { callback(false, juce::String(message)); });
      return;
    }

    // Activation registered the seat — validate() once more so
    // client().snapshot() picks up this license immediately.
    weakThis.get()->client_.validate(
        key, [weakThis, key, callback](licensr::Result<licensr::ValidateResult> validateResult) {
          if (weakThis.get() == nullptr) return;

          if (!validateResult.ok()) {
            auto message = validateResult.error().message;
            juce::MessageManager::callAsync([callback, message] { callback(false, juce::String(message)); });
            return;
          }

          auto result = validateResult.value();
          juce::MessageManager::callAsync([weakThis, key, callback, result] {
            auto* self = weakThis.get();
            if (self == nullptr) return;
            if (result.valid) self->persistLicenseKey(juce::String(key));
            callback(result.valid, result.valid ? "License activated - demo limitation removed."
                                                : "Activated, but this license is not currently valid.");
          });
        });
  });
}

bool LicensrDemoSynthAudioProcessor::isFullyLicensed() const {
  auto snapshot = client_.snapshot();
  return snapshot != nullptr && snapshot->valid;
}

// Required by juce_audio_plugin_client — the one factory function every
// JUCE plugin target must define.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LicensrDemoSynthAudioProcessor(); }
