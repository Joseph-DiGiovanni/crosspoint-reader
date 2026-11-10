#include <Epub/Epub/PageLink.h>
#include <gtest/gtest.h>

#include <vector>

namespace {

PageLink makeLink(const char* href, int16_t x, int16_t y, bool continuesPrevious = false, bool continuesNext = false) {
  PageLink link;
  strncpy(link.href, href, sizeof(link.href) - 1);
  link.x = x;
  link.y = y;
  link.width = 40;
  link.height = 20;
  link.continuesPrevious = continuesPrevious;
  link.continuesNext = continuesNext;
  return link;
}

FootnoteEntry makeEntry(const char* href) {
  FootnoteEntry entry;
  strncpy(entry.number, "x", sizeof(entry.number) - 1);
  strncpy(entry.href, href, sizeof(entry.href) - 1);
  return entry;
}

std::vector<uint32_t> match(const std::vector<FootnoteEntry>& footnotes, const std::vector<PageLink>& links) {
  std::vector<uint32_t> masks(footnotes.size());
  matchFootnoteLinks(footnotes.data(), footnotes.size(), links.data(), links.size(), masks.data());
  return masks;
}

}  // namespace

TEST(PageLinkTest, HitAreaIncludesFingerSlopAndMinimumWidth) {
  PageLink link;
  link.x = 100;
  link.y = 50;
  link.width = 8;
  link.height = 20;

  EXPECT_TRUE(link.contains(90, 44, 6, 28));
  EXPECT_TRUE(link.contains(117, 75, 6, 28));
  EXPECT_FALSE(link.contains(89, 50, 6, 28));
  EXPECT_FALSE(link.contains(118, 50, 6, 28));
  EXPECT_FALSE(link.contains(100, 76, 6, 28));
}

TEST(PageLinkTest, RepeatedHrefEntriesTakeTheirOwnOccurrence) {
  // A reused citation on lines 1 and 4 with nothing linked between them.
  const std::vector<PageLink> links = {makeLink("#note5", 300, 0), makeLink("#note5", 40, 72),
                                       makeLink("#note6", 80, 72)};
  const auto masks = match({makeEntry("#note5"), makeEntry("#note5"), makeEntry("#note6")}, links);
  EXPECT_EQ(masks[0], 0b001u);
  EXPECT_EQ(masks[1], 0b010u);
  EXPECT_EQ(masks[2], 0b100u);
}

TEST(PageLinkTest, WrappedLinkTakesEveryLine) {
  // One link over three lines, then a separate link to the same target that starts line 4.
  const std::vector<PageLink> links = {makeLink("wiki:7", 380, 0, false, true), makeLink("wiki:7", 0, 24, true, true),
                                       makeLink("wiki:7", 0, 48, true, false), makeLink("wiki:7", 0, 72)};
  const auto masks = match({makeEntry("wiki:7"), makeEntry("wiki:7")}, links);
  EXPECT_EQ(masks[0], 0b0111u);
  EXPECT_EQ(masks[1], 0b1000u);
}

TEST(PageLinkTest, RightToLeftContinuationIsFoundOutOfOrder) {
  // The next line lists its spans left to right, so the continuation comes after another link.
  const std::vector<PageLink> links = {makeLink("#a", 0, 0, false, true), makeLink("#b", 0, 24),
                                       makeLink("#a", 300, 24, true, false)};
  const auto masks = match({makeEntry("#a"), makeEntry("#b")}, links);
  EXPECT_EQ(masks[0], 0b101u);
  EXPECT_EQ(masks[1], 0b010u);
}

TEST(PageLinkTest, ContinuationFromThePreviousPageStartsItsOwnRun) {
  const std::vector<PageLink> links = {makeLink("#a", 0, 0, true, false), makeLink("#a", 100, 0)};
  const auto masks = match({makeEntry("#a"), makeEntry("#a")}, links);
  EXPECT_EQ(masks[0], 0b01u);
  EXPECT_EQ(masks[1], 0b10u);
}

TEST(PageLinkTest, EntryWithoutLinkOnThePageHasNoLinks) {
  const std::vector<PageLink> links = {makeLink("#note1", 0, 0)};
  const auto masks = match({makeEntry("#note1"), makeEntry("#note1"), makeEntry("#note2")}, links);
  EXPECT_EQ(masks[0], 0b1u);
  EXPECT_EQ(masks[1], 0u);
  EXPECT_EQ(masks[2], 0u);
}
