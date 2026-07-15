#pragma once

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

template <typename Signature, size_t StorageSize> class FlowFixedFunction;

template <typename ReturnType, size_t StorageSize, typename... Args>
class FlowFixedFunction<ReturnType(Args...), StorageSize> {
  public:
	FlowFixedFunction() = default;

	~FlowFixedFunction() {
		reset();
	}

	FlowFixedFunction(const FlowFixedFunction &other) {
		copyFrom(other);
	}

	FlowFixedFunction &operator=(const FlowFixedFunction &other) {
		if (this != &other) {
			reset();
			copyFrom(other);
		}
		return *this;
	}

	FlowFixedFunction(FlowFixedFunction &&other) noexcept {
		moveFrom(other);
	}

	FlowFixedFunction &operator=(FlowFixedFunction &&other) noexcept {
		if (this != &other) {
			reset();
			moveFrom(other);
		}
		return *this;
	}

	template <typename Callable> bool assign(Callable callable) {
		using CallableType = std::decay_t<Callable>;
		static_assert(
		    std::is_invocable_r_v<ReturnType, CallableType &, Args...>,
		    "Flow callback signature does not match"
		);
		static_assert(
		    std::is_copy_constructible_v<CallableType>,
		    "Flow callbacks must be copy constructible"
		);
		static_assert(
		    std::is_move_constructible_v<CallableType>,
		    "Flow callbacks must be move constructible"
		);

		if constexpr (
		    sizeof(CallableType) > StorageSize ||
		    alignof(CallableType) > alignof(Storage)
		) {
			return false;
		} else {
			reset();
			new (&_storage) CallableType(std::move(callable));
			_invoke = [](void *storage, Args... args) -> ReturnType {
				return (*reinterpret_cast<CallableType *>(storage))(
				    std::forward<Args>(args)...
				);
			};
			_copy = [](void *destination, const void *source) {
				new (destination) CallableType(*reinterpret_cast<const CallableType *>(source));
			};
			_move = [](void *destination, void *source) {
				new (destination) CallableType(std::move(*reinterpret_cast<CallableType *>(source)));
			};
			_destroy = [](void *storage) {
				reinterpret_cast<CallableType *>(storage)->~CallableType();
			};
			return true;
		}
	}

	void reset() {
		if (_destroy != nullptr) {
			_destroy(&_storage);
		}
		_invoke = nullptr;
		_copy = nullptr;
		_move = nullptr;
		_destroy = nullptr;
	}

	explicit operator bool() const {
		return _invoke != nullptr;
	}

	ReturnType operator()(Args... args) const {
		if constexpr (std::is_void_v<ReturnType>) {
			_invoke(
			    const_cast<void *>(static_cast<const void *>(&_storage)),
			    std::forward<Args>(args)...
			);
		} else {
			return _invoke(
			    const_cast<void *>(static_cast<const void *>(&_storage)),
			    std::forward<Args>(args)...
			);
		}
	}

  private:
	using Storage = std::aligned_storage_t<StorageSize, alignof(std::max_align_t)>;
	using Invoke = ReturnType (*)(void *, Args...);
	using Copy = void (*)(void *, const void *);
	using Move = void (*)(void *, void *);
	using Destroy = void (*)(void *);

	void copyFrom(const FlowFixedFunction &other) {
		if (other._copy == nullptr) {
			return;
		}
		other._copy(&_storage, &other._storage);
		_invoke = other._invoke;
		_copy = other._copy;
		_move = other._move;
		_destroy = other._destroy;
	}

	void moveFrom(FlowFixedFunction &other) {
		if (other._move == nullptr) {
			return;
		}
		other._move(&_storage, &other._storage);
		_invoke = other._invoke;
		_copy = other._copy;
		_move = other._move;
		_destroy = other._destroy;
		other.reset();
	}

	mutable Storage _storage;
	Invoke _invoke = nullptr;
	Copy _copy = nullptr;
	Move _move = nullptr;
	Destroy _destroy = nullptr;
};
