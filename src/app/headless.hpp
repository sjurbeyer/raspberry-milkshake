#pragma once

#include <QString>

namespace rm {

class LibraryModel;

// No touch display connected (or --headless): there is no one to drive the touch UI, so the app
// runs the interactive collection on the main output by itself and skips creating a window. Checked
// once at startup (design-spec.md section 15.1, item 3); a display plugged in later needs a restart.
bool should_run_headless(int screen_count, bool force_headless, bool force_ui);

// What to play when starting headless: the first interactive collection in library order
// (LibraryModel already sorts interactive collections first). Empty if the library has none.
QString find_default_interactive_id(const LibraryModel& model);

}  // namespace rm
