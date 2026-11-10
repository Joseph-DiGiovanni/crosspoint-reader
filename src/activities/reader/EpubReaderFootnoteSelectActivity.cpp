#include "EpubReaderFootnoteSelectActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace {
constexpr unsigned long WORD_REPEAT_START_MS = 500;
constexpr unsigned long WORD_REPEAT_INTERVAL_MS = 500;
}  // namespace

void EpubReaderFootnoteSelectActivity::onEnter() {
  Activity::onEnter();
  snapshot = makeUniqueNoThrow<uint8_t[]>(SNAPSHOT_CAPACITY);
  buildFootnoteLinks();
  requestUpdate();
}

void EpubReaderFootnoteSelectActivity::buildFootnoteLinks() {
  footnoteLinks.clear();
  const size_t count = std::min<size_t>(page->footnotes.size(), Page::MAX_FOOTNOTES_PER_PAGE);
  footnoteLinks.reserve(count);

  // A repeated href (a reused citation) has one entry per occurrence; each gets its own links.
  uint32_t masks[Page::MAX_FOOTNOTES_PER_PAGE];
  matchFootnoteLinks(page->footnotes.data(), count, page->links.data(), page->links.size(), masks);
  for (size_t i = 0; i < count; i++) {
    if (masks[i] != 0) footnoteLinks.push_back({masks[i], &page->footnotes[i]});
  }
}

void EpubReaderFootnoteSelectActivity::performJump() {
  if (footnoteLinks.empty()) return;
  ActivityResult result;
  result.data = FootnoteResult{footnoteLinks[selected].footnote->href};
  setResult(std::move(result));
  finish();
}

void EpubReaderFootnoteSelectActivity::drawHints() const {
  const auto labels = mappedInput.mapDirectionalLabels(tr(STR_BACK), tr(STR_OPEN), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT),
                                                       tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void EpubReaderFootnoteSelectActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if ((mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
       mappedInput.wasReleased(MappedInputManager::Button::Power)) &&
      !footnoteLinks.empty()) {
    performJump();
    return;
  }

  if (footnoteLinks.empty()) return;

  const bool hasNext = selected + 1 < static_cast<int>(footnoteLinks.size());
  const unsigned long now = millis();
  const bool repeat =
      mappedInput.getHeldTime() >= WORD_REPEAT_START_MS && now - lastHorizontalMoveTime >= WORD_REPEAT_INTERVAL_MS;
  const bool moveLeft = mappedInput.wasPressed(MappedInputManager::Button::ScreenLeft) ||
                        (repeat && mappedInput.isPressed(MappedInputManager::Button::ScreenLeft));
  const bool moveRight = mappedInput.wasPressed(MappedInputManager::Button::ScreenRight) ||
                         (repeat && mappedInput.isPressed(MappedInputManager::Button::ScreenRight));
  if ((moveLeft || mappedInput.wasPressed(MappedInputManager::Button::ScreenUp)) && selected > 0) {
    selected--;
    if (moveLeft) lastHorizontalMoveTime = now;
    requestUpdate();
  } else if ((moveRight || mappedInput.wasPressed(MappedInputManager::Button::ScreenDown)) && hasNext) {
    selected++;
    if (moveRight) lastHorizontalMoveTime = now;
    requestUpdate();
  }
}

bool EpubReaderFootnoteSelectActivity::drawHighlightWithSnapshot(const int index) {
  if (footnoteLinks.empty()) return false;
  const uint32_t lines = footnoteLinks[index].links;

  // Save every line's box before drawing any: the boxes of adjacent lines can overlap.
  // More boxes or pixels than the buffer holds leave the next move a full repaint.
  bool saved = snapshot != nullptr;
  size_t used = 0;
  snapshotBoxCount = 0;
  for (uint32_t bits = lines; bits != 0 && saved; bits &= bits - 1) {
    const PageLink& link = page->links[__builtin_ctz(bits)];
    int hx = link.x + marginLeft - 2;
    int hy = link.y + marginTop - 2;
    int hw = link.width + 4;
    int hh = link.height + 4;
    if (hx < 0) {
      hw += hx;
      hx = 0;
    }
    if (hy < 0) {
      hh += hy;
      hy = 0;
    }
    size_t bytes = 0;
    if (snapshotBoxCount < MAX_SNAPSHOT_BOXES && hw > 0 && hh > 0) {
      bytes = renderer.readFramebufferRegion(hx, hy, hw, hh, snapshot.get() + used, SNAPSHOT_CAPACITY - used);
    }
    saved = bytes > 0;
    if (!saved) break;
    snapshotBoxes[snapshotBoxCount++] = {static_cast<int16_t>(hx), static_cast<int16_t>(hy), static_cast<int16_t>(hw),
                                         static_cast<int16_t>(hh), static_cast<uint16_t>(used)};
    used += bytes;
  }
  snapshotIdx = saved ? index : -1;

  for (uint32_t bits = lines; bits != 0; bits &= bits - 1) {
    const PageLink& link = page->links[__builtin_ctz(bits)];
    const int x = link.x + marginLeft;
    const int y = link.y + marginTop;
    renderer.fillRectDither(x, y, link.width, link.height, Color::LightGray, GfxRenderer::DitherMode::PreserveInk);
    renderer.drawRect(x - 2, y - 2, link.width + 4, link.height + 4, true);
  }
  return saved;
}

void EpubReaderFootnoteSelectActivity::render(RenderLock&&) {
  // loop() moves the selection without the render lock: read it once per render.
  const int target = selected;

  // Differential fast path: only the highlight moved and the framebuffer still
  // holds a clean page.
  if (snapshotIdx >= 0 && !footnoteLinks.empty() && target != snapshotIdx) {
    for (uint8_t i = 0; i < snapshotBoxCount; i++) {
      const SnapshotBox& box = snapshotBoxes[i];
      renderer.writeFramebufferRegion(box.x, box.y, box.w, box.h, snapshot.get() + box.offset);
    }
    if (drawHighlightWithSnapshot(target)) {
      drawHints();
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      return;
    }
    // Snapshot failed — fall through to a full repaint.
  }

  renderer.clearScreen();

  // Render the page underneath
  if (page) {
    page->render(renderer, SETTINGS.getReaderFontId(), marginLeft, marginTop);
  }

  if (!footnoteLinks.empty()) {
    drawHighlightWithSnapshot(target);
  }

  drawHints();

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
