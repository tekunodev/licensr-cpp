#pragma once

#include <licensr/licensr.h>

#include <functional>
#include <string>

// This header is only ever reached via licensr_juce.h (which pulls in
// <juce_gui_basics/juce_gui_basics.h> first), matching every other JUCE
// module's convention of not re-including juce_gui_basics.h from headers
// further down the include chain.

namespace licensr {

// A self-contained, drop-in JUCE Component that prompts for a license
// key, activates a seat against the Licensr license API, and reports the
// outcome. Owns its own Client (and its background worker thread) for the
// lifetime of the component — construct one per plugin/editor instance,
// not per window if you have several editor windows open at once.
//
// Every Client callback fires on Licensr's worker thread (see
// licensr/licensr.h's Client class comment); this component always hops
// back to the JUCE message thread via juce::MessageManager::callAsync
// before touching UI state or invoking `onActivated`/`onError`, guarded
// by a juce::Component::SafePointer in case the component is destroyed
// (e.g. the editor closed) while a request is still in flight. Those two
// callbacks are therefore always safe to touch UI from directly.
//
// Deliberately uses JUCE's own camelCase naming for this component's
// members and methods (matching the surrounding JUCE codebase it's
// dropped into), while everything it calls on the core `licensr::` API —
// ClientConfig, ActivateRequest, etc. — keeps that API's snake_case.
class LicenseActivationComponent : public juce::Component {
 public:
  // `deviceId` is typically `licensr::compute_hardware_id(...)`, computed
  // by the caller and passed in rather than computed internally, so
  // callers that already have a stable per-install identifier (or a
  // fallback UUID for when compute_hardware_id() returns std::nullopt)
  // can reuse it without this component making that decision for them.
  LicenseActivationComponent(ClientConfig config, std::string deviceId);
  ~LicenseActivationComponent() override;

  void paint(juce::Graphics& g) override;
  void resized() override;

  // Fired on the message thread after a successful activate().
  std::function<void(const ActivateResult&)> onActivated;
  // Fired on the message thread for a failed activate() (network or API error).
  std::function<void(const Error&)> onError;

  // True once this component has seen a successful activate() this
  // session. Does not persist across restarts — that's what
  // licensr::FileTokenStore (see licensr/token_store.h) and re-validating
  // on launch are for.
  bool isActivated() const { return isActivated_; }

  // The underlying client, for callers that need the rest of the license
  // API surface (checkout, deactivate, activations) alongside this UI.
  Client& client() { return client_; }

 private:
  void handleActivateClicked();
  void setStatus(const juce::String& text, bool isError);

  Client client_;
  std::string deviceId_;
  bool isActivated_ = false;

  juce::Label promptLabel_;
  juce::TextEditor licenseKeyEditor_;
  juce::TextButton activateButton_;
  juce::Label statusLabel_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseActivationComponent)
};

}  // namespace licensr
