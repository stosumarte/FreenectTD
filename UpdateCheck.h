//
//  UpdateCheck.h
//  FreenectTD
//
//  Created by marte on 09/10/2026.
//

#pragma once

// Asks GitHub for the latest FreenectTD release in the background and shows the result in a macOS dialog,
// with a button to open the releases page when an update is available. Does nothing if a check is already running.
void checkForUpdates(const char* installedVersion);

// Opens the FreenectTD releases page in the default browser
void openReleasesPage();
