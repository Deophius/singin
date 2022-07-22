#ifndef SPIRIT_APP_H
#define SPIRIT_APP_H

#include "singd.h"

// The final piece of the puzzle, the main and window manip funcs.
namespace Spirit {
    // Hides the console window, to conceal our program
    // Uses win32 API to do this, errors are ignored.
    void hide_window() noexcept;

    // Validates a configuration. If all the required entries are present and
    // of the correct type, returns true. Returns false otherwise.
    bool validate(const Configuration& config);

    // Displays an error message.
    void error_dialog(std::string_view caption, std::string_view text);
}

// Main function for the spirit program.
int main();

#endif