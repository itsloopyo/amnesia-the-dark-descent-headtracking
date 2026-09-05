#pragma once

namespace AmnesiaHT {

// Resolves gpBase, the single cLuxBase global the game's state hangs off.
// Returns false if the anchor it is read from is not found, which leaves
// IsInGameplay permanently false.
bool InitGameState();

// The live cLuxBase, or nullptr before the game has constructed it.
const char* LuxBase();

// True only while the player is in gameplay. False in the main menu (which is
// what Escape opens), the inventory, the journal, a load screen, the credits,
// the pre-menu and the debug menu.
bool IsInGameplay();

}  // namespace AmnesiaHT
