#pragma once
#include "engine/render/Renderer.h"
#include "game/content/Catalog.h"
#include "game/model/Types.h"
#include <string>

namespace thorium::presentation {
/** Return a mineral's presentation color, shared by the cutaway, cues and cost chips. */
engine::Color mineOreColor(game::Ore ore);
/** Return the conventional color of one nest-count clue number. */
engine::Color clueColor(int clue);
/** Draw a small pickaxe glyph rotated by `angle` radians around its center. */
void drawPickaxe(engine::Renderer &graphics, engine::ScreenPoint center, float size, engine::Color head,
                 float angle);
/** Return a relic's effect line with the catalog's actual bonus numbers. */
std::string relicEffect(game::Relic relic, const game::RunBalance &balance);
/** Return a relic's accent color for icons, banners and pop-ups. */
engine::Color relicColor(game::Relic relic);
/** Draw a relic emblem inside a dark disc of diameter `size`. */
void drawRelicIcon(engine::Renderer &graphics, game::Relic relic, engine::ScreenPoint center, float size);
/** Return a special tile's accent color. */
engine::Color mineSpecialColor(game::MineSpecial special);
/** Draw a small special-tile glyph (chest, fossil, energy crystal or cracks) of height about `size`. */
void drawSpecialGlyph(engine::Renderer &graphics, game::MineSpecial special, engine::ScreenPoint center,
                      float size);
/** Estimate the drawn width of a UTF-8 label in logical pixels for plates and tooltips. */
float labelWidth(const std::string &value, float size);
} // namespace thorium::presentation
