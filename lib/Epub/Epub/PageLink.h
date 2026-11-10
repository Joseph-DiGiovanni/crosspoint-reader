#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FootnoteEntry.h"

// One laid-out internal EPUB link. Coordinates are relative to the page origin;
// the reader adds its oriented margins when hit-testing the displayed page.
struct PageLink {
  char href[FOOTNOTE_HREF_LEN];
  int16_t x = 0;
  int16_t y = 0;
  int16_t width = 0;
  int16_t height = 0;
  // A wrapped link has one PageLink per line: these mark the line breaks inside it.
  bool continuesPrevious = false;
  bool continuesNext = false;

  PageLink() { href[0] = '\0'; }

  bool contains(const int pageX, const int pageY, const int slop, const int minWidth) const {
    const int horizontalSlop = std::max(slop, (minWidth - width) / 2);
    return pageX >= x - horizontalSlop && pageX < x + width + horizontalSlop && pageY >= y - slop &&
           pageY < y + height + slop;
  }
};

// Links per page matchFootnoteLinks can return: its masks hold one bit per link.
constexpr size_t MAX_MATCHED_LINKS = 32;

// Pairs each footnote entry with the page links of its own link occurrence, as a mask of
// indices into `links` (0 when it has none). Entries and links are both in page order, so
// each entry takes the next unclaimed link with its href, then follows that link's line
// breaks: on each later line, the first unclaimed same-href link continuing a line. Spans on
// one line share a bottom edge; right-to-left lines list spans left to right, so a
// continuation may come after other links. Mask bits run top to bottom.
inline void matchFootnoteLinks(const FootnoteEntry* footnotes, const size_t footnoteCount, const PageLink* links,
                               size_t linkCount, uint32_t* masks) {
  linkCount = std::min(linkCount, MAX_MATCHED_LINKS);
  const auto bottom = [links](const size_t i) { return links[i].y + links[i].height; };
  uint32_t claimed = 0;
  for (size_t f = 0; f < footnoteCount; f++) {
    masks[f] = 0;
    size_t last = 0;
    while (last < linkCount && (((claimed >> last) & 1u) || strcmp(links[last].href, footnotes[f].href) != 0)) last++;
    if (last == linkCount) continue;
    masks[f] = 1u << last;
    while (links[last].continuesNext) {
      size_t next = last + 1;
      while (next < linkCount && (((claimed >> next) & 1u) || !links[next].continuesPrevious ||
                                  bottom(next) <= bottom(last) || strcmp(links[next].href, links[last].href) != 0)) {
        next++;
      }
      if (next == linkCount) break;
      masks[f] |= 1u << next;
      last = next;
    }
    claimed |= masks[f];
  }
}
