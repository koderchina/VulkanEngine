#pragma once

#include <functional>

namespace Engine
{

// from: https://stackoverflow.com/a/57595105
/**
 * @brief Folds the hash of each argument into `seed` using boost's hash_combine formula.
 *
 * Used by the std::hash<Model::Vertex> specialization (see Model.cpp) to
 * combine position/color/normal/uv into a single hash for vertex
 * deduplication while loading .obj files.
 * @param seed Accumulator hash, modified in place.
 * @param v First value to hash in.
 * @param rest Remaining values to hash in, folded left to right.
 */
template <typename T, typename... Rest>
void hashCombine(std::size_t& seed, const T& v, const Rest&... rest) {
  seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  (hashCombine(seed, rest), ...);
};

}  // namespace lve
