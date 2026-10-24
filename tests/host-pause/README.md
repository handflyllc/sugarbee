# Host pause menu manual checks

Build the engine and package the Pong Lab cartridge using its README.
Run these checks with two controllers and the keyboard.

## Menu and gameplay

- Start on either controller, or keyboard Enter, opens the menu.
- Resume is selected whenever the menu opens.
- Holding Start does not repeatedly toggle pause.
- Up/Down selects Resume or Exit Game.
- A confirms Resume; keyboard J does the same.
- B or Start resumes from either selection; keyboard K also resumes.
- Opening the menu while pressing A does not immediately activate a choice.
- The ball, paddles, and background remain frozen while paused.
- Resuming does not produce a burst of catch-up movement.

## Input suppression

- Hold a direction while resuming: the paddle remains still until the
  direction is released and pressed again.
- Repeat with both controllers and the keyboard.
- Pause on Pong's winner screen and resume with B held: the match does not
  restart. Release B and press it again to restart.

## Exit and error handling

- Select Exit Game with each controller and keyboard J in separate runs.
  The idle host screen should appear and the window should remain open.
- Confirm Start, directions, and A have no effect on the idle screen.
- Close the idle host with a fresh B press or keyboard K.
- Run the shutdown-test cartridge in its normal case and exit through the
  menu. The callback's entered/completed messages should each appear once.
  Closing the idle host afterward must not print them again.
- Run an existing Guru test case and confirm its error screen still displays.

## Automated coverage

Run `ctest --test-dir build --output-on-failure`.

The graphics smoke test checks initial frame contents, retained-image reuse,
overlay separation, and clearing for a new frame. The manual checks above
exercise host menu behavior that the automated graphics test does not cover.