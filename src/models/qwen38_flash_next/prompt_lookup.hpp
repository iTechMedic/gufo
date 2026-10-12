#ifndef GUFO_MODELS_QWEN38_FLASH_NEXT_PROMPT_LOOKUP_HPP_
#define GUFO_MODELS_QWEN38_FLASH_NEXT_PROMPT_LOOKUP_HPP_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <unordered_map>
#include <vector>

namespace gufo::models::qwen38_flash_next {

/// Prompt lookup: proposals copied from the session's own committed tokens.
/// The drafter's context (committed tokens plus the cycle's chain so far)
/// ends in a key of kKeyTokens; among the most recent earlier occurrences of
/// that key, the one whose preceding tokens match the context furthest back
/// wins, and the tokens that followed it are the proposals. Matches shorter
/// than kMinMatch propose nothing: short matches are poor predictions and
/// would displace accepted MTP drafts.
///
/// The index is a pure function of the committed tokens, so proposals do not
/// depend on cache restoration, batching or request timing. Proposals depend
/// only on the prefix; point-mass rejection sampling keeps the target
/// distribution exact.
class PromptLookup {
public:
  static constexpr std::size_t kKeyTokens = 3;
  static constexpr std::size_t kMinMatch = 12;
  static constexpr std::size_t kMaxMatch = 64;
  static constexpr std::size_t kMaxCandidates = 16;

  struct Match {
    std::size_t start{0};   ///< committed index of the first proposal
    std::size_t length{0};  ///< matched context tokens (>= kMinMatch)
  };

  void Clear() {
    head_.clear();
    prev_.clear();
    tokens_.clear();
  }

  /// Indexes exactly `committed`. The common prefix with the previous call
  /// is kept; later entries are unlinked in reverse order, which restores the
  /// index a fresh build of that prefix would have.
  void Sync(std::span<const std::int32_t> committed) {
    const auto limit = std::min(tokens_.size(), committed.size());
    const auto common = static_cast<std::size_t>(
        std::mismatch(tokens_.begin(), tokens_.begin() + limit,
                      committed.begin())
            .first -
        tokens_.begin());
    for (auto end = tokens_.size(); end > common; --end) {
      const auto at = end - 1;
      if (at < kKeyTokens)
        continue;
      const auto key =
          Key(std::span(tokens_).subspan(at - kKeyTokens, kKeyTokens));
      const auto found = head_.find(key);
      if (prev_[at] == kNone)
        head_.erase(found);
      else
        found->second = prev_[at];
    }
    tokens_.resize(common);
    prev_.resize(common);
    tokens_.insert(tokens_.end(), committed.begin() + common, committed.end());
    for (std::size_t end = common; end < tokens_.size(); ++end) {
      // prev_[end] chains earlier occurrences of the key ending at `end`.
      std::uint32_t previous = kNone;
      if (end >= kKeyTokens) {
        const auto key =
            Key(std::span(tokens_).subspan(end - kKeyTokens, kKeyTokens));
        auto [it, inserted] =
            head_.try_emplace(key, static_cast<std::uint32_t>(end));
        if (!inserted) {
          previous = it->second;
          it->second = static_cast<std::uint32_t>(end);
        }
      }
      prev_.push_back(previous);
    }
    // The newest key (ending at committed.size()) has no continuation yet;
    // it is indexed once the next token commits.
  }

  /// Best match for the context `committed` + `tail`, or a zero-length match.
  /// `tail` holds the uncommitted chain (anchor and drafts); `committed` is
  /// the span most recently passed to Sync().
  [[nodiscard]] Match Find(std::span<const std::int32_t> tail) const {
    const std::span<const std::int32_t> committed = tokens_;
    const std::size_t context = committed.size() + tail.size();
    if (context < kKeyTokens)
      return {};
    const auto at = [&](std::size_t i) {
      return i < committed.size() ? committed[i] : tail[i - committed.size()];
    };
    std::int32_t key_tokens[kKeyTokens];
    for (std::size_t i = 0; i < kKeyTokens; ++i)
      key_tokens[i] = at(context - kKeyTokens + i);
    const auto found = head_.find(Key(key_tokens));
    if (found == head_.end())
      return {};
    Match best{};
    std::uint32_t end = found->second;
    for (std::size_t n = 0; n < kMaxCandidates && end != kNone; ++n) {
      // An occurrence ending at `end` proposes committed[end...]. The key
      // itself matched; extend backwards against the context.
      std::size_t length = kKeyTokens;
      while (length < kMaxMatch && end > length && context > length &&
             committed[end - length - 1] == at(context - length - 1)) {
        ++length;
      }
      if (length > best.length)
        best = {end, length};
      end = prev_[end];
    }
    return best.length >= kMinMatch ? best : Match{};
  }

  [[nodiscard]] std::span<const std::int32_t> Tokens() const noexcept {
    return tokens_;
  }

private:
  static constexpr std::uint32_t kNone =
      std::numeric_limits<std::uint32_t>::max();

  /// Token ids fit in 21 bits, so a key packs exactly: equal keys are equal
  /// token triples.
  static std::uint64_t Key(std::span<const std::int32_t> tokens) {
    std::uint64_t key = 0;
    for (const auto t : tokens) {
      key = (key << 21) |
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(t)) &
             0x1FFFFF);
    }
    return key;
  }

  std::unordered_map<std::uint64_t, std::uint32_t> head_;
  std::vector<std::uint32_t> prev_;
  std::vector<std::int32_t> tokens_;
};

}  // namespace gufo::models::qwen38_flash_next

#endif  // GUFO_MODELS_QWEN38_FLASH_NEXT_PROMPT_LOOKUP_HPP_
