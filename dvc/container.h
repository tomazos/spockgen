#pragma once

#include <algorithm>
#include <utility>

namespace dvc {

template<typename Container, typename Element>
void insert_or_die(Container& container, Element&& element) {
  auto result = container.insert(std::forward<Element>(element));
  CHECK(result.second) << "insert_or_die failed";
}

template<typename Container, typename Key, typename Value>
void insert_or_die(Container& container, Key&& key, Value&& value) {
  auto result = container.try_emplace(std::forward<Key>(key), std::forward<Value>(value));
  CHECK(result.second) << "insert_or_die failed";
}

template<typename Container>
void sort(Container& container) {
  std::sort(container.begin(), container.end());
}

}  // namespace dvc
