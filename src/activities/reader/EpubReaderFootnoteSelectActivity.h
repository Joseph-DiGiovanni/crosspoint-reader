#pragma once

#include <Epub/Page.h>
#include <Epub/PageLink.h>
#include <I18n.h>

#include <memory>
#include <vector>

#include "activities/Activity.h"

// Footnote reference selection overlaid on the current reader page. Each
// reference gets a dotted background and outline without redrawing its text.
// Direction buttons move between markers, Confirm jumps to that footnote,
// Back returns to the reader.
class EpubReaderFootnoteSelectActivity final : public Activity {
 public:
  explicit EpubReaderFootnoteSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                            std::unique_ptr<Page> page, int marginLeft, int marginTop)
      : Activity("EpubReaderFootnoteSelect", renderer, mappedInput),
        page(std::move(page)),
        marginLeft(marginLeft),
        marginTop(marginTop) {}

 private:
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void buildFootnoteLinks();
  void performJump();
  bool drawHighlightWithSnapshot(int index);
  void drawHints() const;

  std::unique_ptr<Page> page;
  const int marginLeft;
  const int marginTop;

  struct FootnoteLink {
    uint32_t links;  // bit i: page->links[i] is one line of this link
    const FootnoteEntry* footnote;
  };
  std::vector<FootnoteLink> footnoteLinks;
  int selected = 0;

  // Differential highlight repaint: pixels under the current highlight's boxes, one per line.
  struct SnapshotBox {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t offset;  // into snapshot
  };
  static constexpr size_t SNAPSHOT_CAPACITY = 4096;
  static constexpr uint8_t MAX_SNAPSHOT_BOXES = 4;
  std::unique_ptr<uint8_t[]> snapshot;
  SnapshotBox snapshotBoxes[MAX_SNAPSHOT_BOXES] = {};
  uint8_t snapshotBoxCount = 0;
  int snapshotIdx = -1;

  unsigned long lastHorizontalMoveTime = 0;
};
