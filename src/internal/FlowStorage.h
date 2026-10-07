#pragma once

#include <Strata.h>

#include <cstddef>
#include <memory>
#include <utility>

template <typename T> class FlowStorage {
  public:
	FlowStorage() = default;

	~FlowStorage() {
		reset();
	}

	FlowStorage(const FlowStorage &) = delete;
	FlowStorage &operator=(const FlowStorage &) = delete;

	FlowStorage(FlowStorage &&other) noexcept {
		moveFrom(other);
	}

	FlowStorage &operator=(FlowStorage &&other) noexcept {
		if (this != &other) {
			reset();
			moveFrom(other);
		}
		return *this;
	}

	[[nodiscard]] static FlowStorage create(
	    size_t count,
	    Strata::Placement placement
	) {
		FlowStorage storage;
		if (count == 0) {
			return storage;
		}

		storage._items = Strata::allocateArray<T>(count, placement);
		if (storage._items == nullptr) {
			return storage;
		}
		storage._placement = placement;

		for (size_t index = 0; index < count; ++index) {
			std::construct_at(storage._items + index);
			storage._size++;
		}

		return storage;
	}

	void reset() {
		if (_items == nullptr) {
			_size = 0;
			_placement = Strata::Placement::Default;
			return;
		}

		for (size_t index = _size; index > 0; --index) {
			std::destroy_at(_items + (index - 1));
		}
		Strata::free(_items);
		_items = nullptr;
		_size = 0;
		_placement = Strata::Placement::Default;
	}

	[[nodiscard]] T *data() {
		return _items;
	}

	[[nodiscard]] const T *data() const {
		return _items;
	}

	[[nodiscard]] size_t size() const {
		return _size;
	}

	[[nodiscard]] Strata::Placement placement() const {
		return _placement;
	}

	[[nodiscard]] Strata::Region region() const {
		return Strata::regionOf(_items);
	}

	[[nodiscard]] explicit operator bool() const {
		return _items != nullptr;
	}

	T &operator[](size_t index) {
		return _items[index];
	}

	const T &operator[](size_t index) const {
		return _items[index];
	}

  private:
	void moveFrom(FlowStorage &other) noexcept {
		_items = std::exchange(other._items, nullptr);
		_size = std::exchange(other._size, 0);
		_placement = std::exchange(other._placement, Strata::Placement::Default);
	}

	T *_items = nullptr;
	size_t _size = 0;
	Strata::Placement _placement = Strata::Placement::Default;
};
