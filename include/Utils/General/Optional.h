#pragma once

//===----------------------------------------------------------------------===//
/// @file Optional.h
/// @brief Simple optional value implementation
///
/// This file provides a lightweight replacement for std::optional. It supports
/// basic optional semantics including:
/// - Value construction and nullopt construction
/// - Copy and move semantics
/// - Safe value access with assertions
/// - Implicit bool conversion for checking presence
///
/// @note This is a simplified implementation. For production code, consider
/// using std::optional from C++17 or later.
///
///===----------------------------------------------------------------------===//

#include <cassert>
#include <utility>

namespace util {

// Empty optional tag
/// @brief Tag type for constructing empty optionals
struct nullopt_t {
  /// @brief Constructor with dummy parameter
  explicit constexpr nullopt_t(int) {}
};

/// @brief Constant for constructing empty optionals
constexpr nullopt_t nullopt{0};

// A simple replacement for std::optional
/// @brief Optional value container
///
/// This template class provides optional semantics for any type T. It stores
/// a value in inline storage and tracks whether a value is present.
///
/// @tparam T The type of the contained value
template <typename T> class Optional {
private:
  bool has_value_;
  alignas(T) char storage_[sizeof(T)];

  /// @brief Get pointer to the stored value
  T *ptr() { return reinterpret_cast<T *>(storage_); }
  /// @brief Get const pointer to the stored value
  const T *ptr() const { return reinterpret_cast<const T *>(storage_); }

public:
  /// @brief Default constructor (creates empty optional)
  Optional() : has_value_(false) {}

  /// @brief Construct from nullopt (creates empty optional)
  /// @param Ignored parameter for disambiguation
  Optional(nullopt_t) : has_value_(false) {}

  /// @brief Copy constructor
  Optional(const Optional &other) : has_value_(other.has_value_) {
    if (has_value_) {
      new (storage_) T(*other.ptr());
    }
  }

  /// @brief Move constructor
  Optional(Optional &&other) noexcept : has_value_(other.has_value_) {
    if (has_value_) {
      new (storage_) T(std::move(*other.ptr()));
    }
  }

  /// @brief Construct from value (copy)
  Optional(const T &value) : has_value_(true) { new (storage_) T(value); }

  /// @brief Construct from value (move)
  Optional(T &&value) : has_value_(true) { new (storage_) T(std::move(value)); }

  /// @brief Destructor
  ~Optional() {
    if (has_value_) {
      ptr()->~T();
    }
  }

  /// @brief Copy assignment
  Optional &operator=(const Optional &other) {
    if (this != &other) {
      if (has_value_) {
        if (other.has_value_) {
          *ptr() = *other.ptr();
        } else {
          ptr()->~T();
          has_value_ = false;
        }
      } else if (other.has_value_) {
        new (storage_) T(*other.ptr());
        has_value_ = true;
      }
    }
    return *this;
  }

  /// @brief Move assignment
  Optional &operator=(Optional &&other) noexcept {
    if (this != &other) {
      if (has_value_) {
        if (other.has_value_) {
          *ptr() = std::move(*other.ptr());
        } else {
          ptr()->~T();
          has_value_ = false;
        }
      } else if (other.has_value_) {
        new (storage_) T(std::move(*other.ptr()));
        has_value_ = true;
      }
    }
    return *this;
  }

  /// @brief Assign from nullopt (clears the value)
  Optional &operator=(nullopt_t) {
    if (has_value_) {
      ptr()->~T();
      has_value_ = false;
    }
    return *this;
  }

  /// @brief Assign from value (copy)
  Optional &operator=(const T &value) {
    if (has_value_) {
      *ptr() = value;
    } else {
      new (storage_) T(value);
      has_value_ = true;
    }
    return *this;
  }

  /// @brief Assign from value (move)
  Optional &operator=(T &&value) {
    if (has_value_) {
      *ptr() = std::move(value);
    } else {
      new (storage_) T(std::move(value));
      has_value_ = true;
    }
    return *this;
  }

  /// @brief Check if the optional contains a value
  /// @return true if a value is present
  bool has_value() const { return has_value_; }
  /// @brief Implicit conversion to bool for checking presence
  explicit operator bool() const { return has_value_; }

  /// @brief Get reference to the value (non-const lvalue)
  /// @return Reference to the contained value
  /// @pre has_value() must be true
  T &value() & {
    assert(has_value_);
    return *ptr();
  }

  /// @brief Get const reference to the value (const lvalue)
  /// @return Const reference to the contained value
  /// @pre has_value() must be true
  const T &value() const & {
    assert(has_value_);
    return *ptr();
  }

  /// @brief Get rvalue reference to the value
  /// @return Rvalue reference to the contained value
  /// @pre has_value() must be true
  T &&value() && {
    assert(has_value_);
    return std::move(*ptr());
  }

  /// @brief Get const rvalue reference to the value
  /// @return Const rvalue reference to the contained value
  /// @pre has_value() must be true
  const T &&value() const && {
    assert(has_value_);
    return std::move(*ptr());
  }

  // Dereference operators
  /// @brief Dereference operator (non-const)
  /// @return Reference to the contained value
  /// @pre has_value() must be true
  T &operator*() & {
    assert(has_value_);
    return *ptr();
  }

  /// @brief Dereference operator (const)
  /// @return Const reference to the contained value
  /// @pre has_value() must be true
  const T &operator*() const & {
    assert(has_value_);
    return *ptr();
  }

  /// @brief Arrow operator (non-const)
  /// @return Pointer to the contained value
  /// @pre has_value() must be true
  T *operator->() {
    assert(has_value_);
    return ptr();
  }

  /// @brief Arrow operator (const)
  /// @return Const pointer to the contained value
  /// @pre has_value() must be true
  const T *operator->() const {
    assert(has_value_);
    return ptr();
  }
};

/// @brief Create an Optional from a value
///
/// This helper function deduces the type and creates an Optional.
///
/// @tparam T The type of the value (deduced)
/// @param value The value to wrap
/// @return An Optional containing the value
template <typename T>
Optional<typename std::decay<T>::type> make_optional(T &&value) {
  return Optional<typename std::decay<T>::type>(std::forward<T>(value));
}

} // namespace util
