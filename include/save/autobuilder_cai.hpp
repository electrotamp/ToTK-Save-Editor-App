#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "save/variable_store.hpp"

namespace totk {

// The Autobuild ".cai" file format, reverse-engineered by Marc Robledo's
// community save editor and confirmed on 2026-08-17 by downloading a real
// blueprint from HyruleWorks (CloudFront-fronted; see hyruleworks_client.hpp)
// and hex-checking it: 6712 bytes, "CmbAct" magic at offset 0, matching
// CAI_EDITOR_SIZE below exactly.
struct AutobuilderCai {
    static constexpr const char* kHeader = "CmbAct";
    static constexpr size_t kHeaderSize = 6;

    // Raw CombinedActorInfo blob as stored per-slot in the save (starts with
    // the "CmbAct" header itself). This is the only part the game actually
    // reads back from AutoBuilder.Draft.Content.CombinedActorInfo array
    // elements, and every slot's stored element must stay exactly this size.
    static constexpr size_t kCaiSize = 6688;
    // In-progress drafts on disk additionally carry a Yiga schematic +
    // schema-stones internal block. Not used by this editor (we only ever
    // read/write the CAI_SIZE actor blob + camera vectors), but a .cai file
    // saved mid-draft from the game itself can be this size.
    static constexpr size_t kDraftSize = kCaiSize + 1152;
    // What a save editor exports/imports: the actor blob plus the two Vec3
    // camera vectors (cameraPos, cameraAt) tacked on the end, 12 bytes each.
    static constexpr size_t kEditorSize = kCaiSize + 12 + 12;

    std::vector<uint8_t> combinedActorInfo;  // exactly kCaiSize bytes when valid
    Vec3 cameraPos{};
    Vec3 cameraAt{};
    bool hasCamera = false;

    bool valid() const { return combinedActorInfo.size() == kCaiSize; }
};

// Parses raw bytes (a loaded .cai file, or a HyruleWorks blueprint download —
// same format) into an AutobuilderCai. Accepts kCaiSize, kDraftSize (extra
// trailing bytes ignored), or kEditorSize (camera vectors read) inputs, all
// gated on the "CmbAct" magic. Returns false with `error` set otherwise.
bool parseAutobuilderCai(const std::vector<uint8_t>& bytes, AutobuilderCai& out, std::string& error);

// Serializes to the kEditorSize (6712-byte) layout used for SD export/import
// and matching what HyruleWorks itself distributes.
std::vector<uint8_t> exportAutobuilderCai(const AutobuilderCai& blueprint);

}  // namespace totk
