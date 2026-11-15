// at-terminal: the language at-tmux and at-terminal share (at-protocol, ATT-7).
// Neither program is ever used without the other, so what the terminal
// protocol can't say travels as at-protocol messages inside APC strings,
// which the parser hands here (OutputStateMachineEngine::ActionApcDispatch).
//
// - hello: tmux says its version; the terminal answers with its own, which is
//   the at-protocol commit it was built from. tmux decides what a mismatch
//   means; the terminal only answers.
// - image-store: a picture to keep by ID, with the grid of cells it fills.
// - image-place: cells of a kept picture's grid, written into the text
//   buffer's image slices at screen cells, the structure sixel images are
//   kept in, so scrolling and redrawing are the buffer's.
// - image-drop: forget a kept picture; what it drew stays until text is
//   written over it, as any image slice does.

#include "precomp.h"
#include "adaptDispatch.hpp"
#include "SixelParser.hpp"
#include "../buffer/out/ImageSlice.hpp"
#include "../types/inc/viewport.hpp"

#include <at_protocol.h>

using namespace Microsoft::Console::VirtualTerminal;
using Microsoft::Console::Types::Viewport;

// The APC string's contents, which are ASCII, as the bytes at_proto_decode
// reads; empty if any character isn't.
static std::string _bytesOf(const std::wstring_view string)
{
    std::string bytes;
    bytes.reserve(string.size());
    for (const auto wch : string)
    {
        if (wch > 0x7f)
        {
            return {};
        }
        bytes.push_back(static_cast<char>(wch));
    }
    return bytes;
}

void AdaptDispatch::AtProtocolMessage(const std::wstring_view string)
{
    const auto bytes = _bytesOf(string);
    at_proto_msg msg{};
    if (bytes.empty() || at_proto_decode(bytes.data(), bytes.size(), &msg) != 1)
    {
        return;
    }
    switch (msg.kind)
    {
    case AT_PROTO_HELLO:
        _atHello();
        break;
    case AT_PROTO_IMAGE_STORE:
    {
        const auto& s = msg.u.image_store;
        AtPicture picture;
        picture.width = s.width;
        picture.height = s.height;
        picture.cols = s.cols;
        picture.rows = s.rows;
        picture.bytesPerPixel = static_cast<uint32_t>(s.format);
        picture.pixels.assign(s.pixels, s.pixels + s.len);
        _atPictures[s.id] = std::move(picture);
        break;
    }
    case AT_PROTO_IMAGE_PLACE:
    {
        const auto& p = msg.u.image_place;
        _atPlace(p.id,
                 til::point{ static_cast<til::CoordType>(p.col), static_cast<til::CoordType>(p.row) },
                 til::point{ static_cast<til::CoordType>(p.src_col), static_cast<til::CoordType>(p.src_row) },
                 til::size{ static_cast<til::CoordType>(p.cols), static_cast<til::CoordType>(p.rows) });
        break;
    }
    case AT_PROTO_IMAGE_DROP:
        _atPictures.erase(msg.u.image_drop.id);
        break;
    }
    at_proto_msg_free(&msg);
}

// Answer a hello with this build's version, on the input stream, as a device
// attributes report goes.
void AdaptDispatch::_atHello()
{
    at_proto_msg hello{};
    hello.kind = AT_PROTO_HELLO;
    strncpy_s(hello.u.hello.version, at_proto_version(), _TRUNCATE);
    char* apc = nullptr;
    size_t len = 0;
    if (at_proto_encode(&hello, &apc, &len) != 0)
    {
        return;
    }
    const std::wstring response{ apc, apc + len };
    free(apc);
    _api.ReturnResponse(response);
}

// Show cells of picture id's grid, from the cell at from, at the screen cells
// from at (the active page's viewport, 0-based), cells wide and high: each
// screen cell gets the matching block of the picture's pixels, nearest
// neighbour, at the nominal cell size sixel images use, which the renderer
// scales to the font. Rows past the page and columns past its width are left
// out; a picture cell past the grid is left as it was.
void AdaptDispatch::_atPlace(const uint32_t id, const til::point at, const til::point from, const til::size cells)
{
    const auto found = _atPictures.find(id);
    if (found == _atPictures.end())
    {
        return;
    }
    const auto& picture = found->second;
    if (picture.cols == 0 || picture.rows == 0 || picture.width == 0 || picture.height == 0)
    {
        return;
    }
    auto page = _pages.ActivePage();
    const auto cellSize = SixelParser::CellSizeForLevel();
    const auto columnBegin = std::max(at.x, 0);
    const auto columnEnd = std::min(at.x + cells.width, page.Width());
    if (columnBegin >= columnEnd || cellSize.width <= 0 || cellSize.height <= 0)
    {
        return;
    }
    // A source pixel for a destination pixel of a picture cell: the cell's
    // block of the picture, nearest neighbour.
    const auto sourceX = [&](const til::CoordType pictureCol, const til::CoordType px) {
        const auto x = (static_cast<uint64_t>(pictureCol) * cellSize.width + px) * picture.width / (static_cast<uint64_t>(picture.cols) * cellSize.width);
        return static_cast<size_t>(std::min<uint64_t>(x, picture.width - 1));
    };
    const auto sourceY = [&](const til::CoordType pictureRow, const til::CoordType py) {
        const auto y = (static_cast<uint64_t>(pictureRow) * cellSize.height + py) * picture.height / (static_cast<uint64_t>(picture.rows) * cellSize.height);
        return static_cast<size_t>(std::min<uint64_t>(y, picture.height - 1));
    };

    auto firstRow = -1;
    auto lastRow = -1;
    for (til::CoordType r = 0; r < cells.height; r++)
    {
        const auto pictureRow = from.y + r;
        const auto bufferRow = page.Top() + at.y + r;
        if (pictureRow < 0 || static_cast<uint32_t>(pictureRow) >= picture.rows || at.y + r < 0 || bufferRow >= page.Bottom())
        {
            break;
        }
        auto& dstRow = page.Buffer().GetMutableRowByOffset(bufferRow);
        auto dstSlice = dstRow.GetMutableImageSlice();
        if (!dstSlice)
        {
            dstSlice = dstRow.SetImageSlice(std::make_unique<ImageSlice>(cellSize));
            __assume(dstSlice != nullptr);
        }
        auto dstIterator = dstSlice->MutablePixels(columnBegin, columnEnd);
        for (til::CoordType py = 0; py < cellSize.height; py++)
        {
            for (auto column = columnBegin; column < columnEnd; column++)
            {
                const auto pictureCol = from.x + (column - at.x);
                if (pictureCol < 0 || static_cast<uint32_t>(pictureCol) >= picture.cols)
                {
                    continue;
                }
                for (til::CoordType px = 0; px < cellSize.width; px++)
                {
                    const auto src = (sourceY(pictureRow, py) * picture.width + sourceX(pictureCol, px)) * picture.bytesPerPixel;
                    const auto* pixel = &til::at(picture.pixels, src);
                    if (picture.bytesPerPixel == 4 && pixel[3] < 128)
                    {
                        continue;
                    }
                    til::at(dstIterator, (column - columnBegin) * cellSize.width + px) = RGBQUAD{
                        .rgbBlue = pixel[2],
                        .rgbGreen = pixel[1],
                        .rgbRed = pixel[0],
                        .rgbReserved = 255
                    };
                }
            }
            std::advance(dstIterator, dstSlice->PixelWidth());
        }
        if (firstRow < 0)
        {
            firstRow = bufferRow;
        }
        lastRow = bufferRow;
    }
    if (firstRow >= 0)
    {
        page.Buffer().TriggerRedraw(Viewport::FromExclusive({ 0, firstRow, page.Width(), lastRow + 1 }));
    }
}
