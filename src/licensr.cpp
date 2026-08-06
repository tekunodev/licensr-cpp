#include "licensr/licensr.h"

#include "detail/client.h"

namespace licensr {

// --- Json -------------------------------------------------------------

Json::Json() = default;
Json::Json(bool value) : type_(Type::kBool), bool_(value) {}
Json::Json(double value) : type_(Type::kNumber), number_(value) {}
Json::Json(std::int64_t value) : type_(Type::kNumber), number_(static_cast<double>(value)) {}
Json::Json(std::string value) : type_(Type::kString), string_(std::move(value)) {}
Json::Json(Array value) : type_(Type::kArray), array_(std::move(value)) {}
Json::Json(Object value) : type_(Type::kObject), object_(std::move(value)) {}

bool Json::as_bool(bool fallback) const { return type_ == Type::kBool ? bool_ : fallback; }
double Json::as_double(double fallback) const { return type_ == Type::kNumber ? number_ : fallback; }
std::int64_t Json::as_int64(std::int64_t fallback) const {
  return type_ == Type::kNumber ? static_cast<std::int64_t>(number_) : fallback;
}

const std::string& Json::as_string() const {
  static const std::string kEmpty;
  return type_ == Type::kString ? string_ : kEmpty;
}

const Json::Array& Json::as_array() const {
  static const Array kEmpty;
  return type_ == Type::kArray ? array_ : kEmpty;
}

const Json::Object& Json::as_object() const {
  static const Object kEmpty;
  return type_ == Type::kObject ? object_ : kEmpty;
}

const Json* Json::find(const std::string& key) const {
  if (type_ != Type::kObject) return nullptr;
  for (const auto& [k, v] : object_) {
    if (k == key) return &v;
  }
  return nullptr;
}

// --- Enum <-> string ----------------------------------------------------

std::string to_string(ActivationType value) {
  switch (value) {
    case ActivationType::kSeat:
      return "seat";
    case ActivationType::kDomain:
      return "domain";
    case ActivationType::kUnknown:
      return "unknown";
  }
  return "unknown";
}

std::string to_string(LicenseStatus value) {
  switch (value) {
    case LicenseStatus::kActive:
      return "active";
    case LicenseStatus::kInactive:
      return "inactive";
    case LicenseStatus::kExpired:
      return "expired";
    case LicenseStatus::kUnknown:
      return "unknown";
  }
  return "unknown";
}

std::string to_string(Entitlement value) {
  switch (value) {
    case Entitlement::kFull:
      return "full";
    case Entitlement::kLimited:
      return "limited";
    case Entitlement::kNone:
      return "none";
    case Entitlement::kUnknown:
      return "unknown";
  }
  return "unknown";
}

// --- Client ---------------------------------------------------------------

Client::Client(ClientConfig config)
    : core_(detail::acquire_client_core(std::move(config))), events_(std::make_shared<Events>()) {
  core_->add_listener(events_);
}

Client::~Client() {
  if (core_) core_->remove_listener(events_);
}

Client::Client(Client&& other) noexcept : core_(std::move(other.core_)), events_(std::move(other.events_)) {
  other.core_ = nullptr;
  other.events_ = nullptr;
}

Client& Client::operator=(Client&& other) noexcept {
  if (this != &other) {
    if (core_) core_->remove_listener(events_);
    core_ = std::move(other.core_);
    events_ = std::move(other.events_);
    other.core_ = nullptr;
    other.events_ = nullptr;
  }
  return *this;
}

Client::Events& Client::events() { return *events_; }

void Client::validate(const std::string& license_key, std::function<void(Result<ValidateResult>)> callback) {
  core_->validate(license_key, std::move(callback));
}

void Client::token(const std::string& license_key, std::function<void(Result<TokenResult>)> callback) {
  core_->token(license_key, std::move(callback));
}

void Client::activate(const ActivateRequest& request, std::function<void(Result<ActivateResult>)> callback) {
  core_->activate(request, std::move(callback));
}

void Client::deactivate(const std::string& license_key, const std::string& activation_id,
                        std::function<void(Result<DeactivateResult>)> callback) {
  core_->deactivate(license_key, activation_id, std::move(callback));
}

void Client::activations(const std::string& license_key, std::function<void(Result<ActivationsResult>)> callback) {
  core_->activations(license_key, std::move(callback));
}

void Client::checkout(const CheckoutRequest& request, std::function<void(Result<CheckoutResult>)> callback) {
  core_->checkout(request, std::move(callback));
}

std::shared_ptr<const ValidateResult> Client::snapshot() const { return core_->snapshot(); }

}  // namespace licensr
