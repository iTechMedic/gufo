#include "src/models/qwen38_flash_next/prompt_lookup.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "src/models/qwen38_flash_next/mtp_policy.hpp"

namespace qfn = gufo::models::qwen38_flash_next;
using Lookup = qfn::PromptLookup;

void Require(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}

std::vector<std::int32_t> Range(std::int32_t first, std::size_t count) {
  std::vector<std::int32_t> out(count);
  for (std::size_t i = 0; i < count; ++i)
    out[i] = first + static_cast<std::int32_t>(i);
  return out;
}

bool Same(Lookup::Match a, Lookup::Match b) {
  return a.start == b.start && a.length == b.length;
}

// A copied passage is proposed once the context repeats kMinMatch tokens of
// it; the proposal continues where the earlier copy continued.
void CheckCopy() {
  auto committed = Range(1000, 40);  // the "file"
  const auto noise = Range(5000, 7);
  committed.insert(committed.end(), noise.begin(), noise.end());
  // The answer starts quoting the file: 10 committed + anchor + one draft.
  committed.insert(committed.end(), committed.begin() + 5,
                   committed.begin() + 15);
  Lookup lookup;
  lookup.Sync(committed);
  const std::vector<std::int32_t> tail = {1015, 1016};
  const auto match = lookup.Find(tail);
  Require(match.length == 12, "a 12-token copy was not matched in full");
  Require(match.start == 17 && committed[match.start] == 1017,
          "the proposal does not continue the copied passage");
  // One token shorter is below the gate.
  const std::vector<std::int32_t> short_tail = {1015};
  Require(lookup.Find(short_tail).length == 0,
          "an 11-token match passed the 12-token gate");
}

// Several earlier occurrences of the key: the one matching furthest back
// wins, not merely the most recent.
void CheckLongestWins() {
  std::vector<std::int32_t> committed;
  const auto a = Range(100, 20);  // occurrence with a long shared history
  committed.insert(committed.end(), a.begin(), a.end());
  committed.push_back(7);
  // A later occurrence of the last three tokens (117 118 119) with a
  // different history and continuation.
  committed.insert(committed.end(), {900, 901, 117, 118, 119, 8});
  // Context: the long history again.
  committed.insert(committed.end(), a.begin(), a.begin() + 17);
  Lookup lookup;
  lookup.Sync(committed);
  const std::vector<std::int32_t> tail = {117, 118, 119};
  const auto match = lookup.Find(tail);
  Require(match.length >= 12 && committed[match.start] == 7,
          "the longest-matching occurrence was not chosen");
}

// The index is a function of the committed tokens alone: growing step by
// step, rewinding to a shorter or divergent history (cache restoration,
// continuation transfer) and rebuilding from scratch all agree, including
// keys whose only occurrences were removed.
void CheckSync() {
  auto committed = Range(1, 30);
  committed.insert(committed.end(), committed.begin(), committed.begin() + 20);
  Lookup whole;
  whole.Sync(committed);
  Lookup steps;
  for (std::size_t n = 1; n <= committed.size(); n += 3)
    steps.Sync(std::span<const std::int32_t>(committed).first(n));
  steps.Sync(committed);
  const std::vector<std::int32_t> tail = {21};
  const auto a = whole.Find(tail);
  Require(a.length >= 12, "repeated history was not matched");
  Require(Same(a, steps.Find(tail)),
          "incremental indexing differs from a full build");

  const std::vector<std::int32_t> repeat_tail = {11, 12};
  for (const std::size_t prefix : {0UL, 2UL, 3UL, 4UL, 29UL, 31UL, 45UL}) {
    auto branch = std::vector<std::int32_t>(committed.begin(),
                                            committed.begin() + prefix);
    const auto suffix = Range(700, 25);
    branch.insert(branch.end(), suffix.begin(), suffix.end());
    branch.insert(branch.end(), branch.begin(), branch.begin() + 12);
    Lookup rewound = whole;
    rewound.Sync(branch);
    Lookup fresh;
    fresh.Sync(branch);
    Require(Same(rewound.Find(tail), fresh.Find(tail)) &&
                Same(rewound.Find(repeat_tail), fresh.Find(repeat_tail)) &&
                Same(rewound.Find({}), fresh.Find({})),
            "a rewound index differs from a fresh build");
    // Return to the original history: the removed occurrences come back.
    rewound.Sync(committed);
    Require(Same(rewound.Find(tail), a),
            "re-extending a rewound index lost occurrences");
  }
  Lookup cleared = whole;
  cleared.Sync({});
  Require(cleared.Find(tail).length == 0, "an empty history kept old keys");
}

void CheckNoMatch() {
  const auto committed = Range(1, 100);
  Lookup lookup;
  lookup.Sync(committed);
  const std::vector<std::int32_t> tail = {7, 8, 9};
  Require(lookup.Find(tail).length == 0,
          "a context without history of its own tokens matched");
  Require(Lookup{}.Find(tail).length == 0, "an empty index matched");
}

// A copy replaces the MTP drafts a cycle has not proposed yet only when more
// copies than drafts are expected to be accepted.
void CheckCopyEvidence() {
  Require(Lookup::ExpectedCopies(13, 0) == 0.0F &&
              std::abs(Lookup::ExpectedCopies(13, 1) - 14.0F / 15.0F) < 1e-6F,
          "one copy is not the rule-of-succession probability");
  for (std::size_t count = 1; count <= 7; ++count) {
    Require(
        Lookup::ExpectedCopies(64, count) > Lookup::ExpectedCopies(12, count) &&
            Lookup::ExpectedCopies(12, count) < static_cast<float>(count) &&
            Lookup::ExpectedCopies(12, count + 1) >
                Lookup::ExpectedCopies(12, count),
        "copy evidence is not monotone in match and copy length");
  }

  // An MTP chain accepted in full (two, four, then seven drafts, as in a
  // repeated tool call) keeps its tail against a 13-token template match.
  qfn::MtpLengthController perfect(7);
  for (const unsigned drafts : {2U, 4U, 7U})
    perfect.Observe(drafts, drafts);
  Require(perfect.ExpectedDrafts(1, 7) == 6.0F &&
              Lookup::ExpectedCopies(13, 6) < perfect.ExpectedDrafts(1, 7),
          "a short match displaced a fully accepted MTP chain");
  Require(perfect.ExpectedDrafts(7, 7) == 0.0F &&
              perfect.ExpectedDrafts(0, 7) + 1.0F == perfect.ExpectedTokens(7),
          "remaining-draft estimate disagrees with the length controller");

  // Rejections in a rewrite leave the tail uncertain; a long match replaces it.
  qfn::MtpLengthController rejected(7);
  for (const unsigned accepted : {7U, 3U, 7U, 5U, 2U, 7U})
    rejected.Observe(accepted, 7);
  Require(Lookup::ExpectedCopies(64, 6) > rejected.ExpectedDrafts(1, 7),
          "a long match did not replace an uncertain MTP tail");

  // At the anchor a copy also replaces the first MTP draft. A first draft
  // that has never been rejected keeps the chain even against the longest
  // match, also after a single short cycle (an early tool-call template);
  // recent first-draft rejections let a 16-token match replace the chain.
  qfn::MtpLengthController early(7);
  early.Observe(2, 2);
  for (const auto* controller : {&perfect, &early})
    Require(controller->ExpectedDrafts(0, 1) == 1.0F &&
                Lookup::ExpectedCopies(Lookup::kMaxMatch, 1) <
                    controller->ExpectedDrafts(0, 1),
            "an anchor match displaced a never-rejected first draft");
  qfn::MtpLengthController weak(7);
  for (const auto [accepted, drafted] :
       {std::pair{2U, 2U}, {4U, 4U}, {5U, 7U}, {0U, 5U}, {5U, 5U}, {4U, 4U}})
    weak.Observe(accepted, drafted);
  Require(Lookup::ExpectedCopies(16, 1) > weak.ExpectedDrafts(0, 1) &&
              Lookup::ExpectedCopies(16, 7) > weak.ExpectedDrafts(0, 7),
          "an anchor match did not replace a recently rejected MTP chain");
}

int main() {
  try {
    CheckCopy();
    CheckLongestWins();
    CheckSync();
    CheckNoMatch();
    CheckCopyEvidence();
  } catch (const std::exception& e) {
    std::cerr << "prompt_lookup_test: " << e.what() << "\n";
    return 1;
  }
  std::cout << "prompt_lookup_test: ok\n";
  return 0;
}
