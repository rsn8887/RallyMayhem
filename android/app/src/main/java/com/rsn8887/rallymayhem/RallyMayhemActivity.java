// Rally Mayhem by rsn8887 - Android entry point
package com.rsn8887.rallymayhem;

import org.libsdl.app.SDLActivity;

public class RallyMayhemActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "SDL2_ttf", "main" };
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        // The game keeps its state in C++ globals, so after quitting start the next launch
        // in a fresh process instead of re-running main() in this one.
        if (isFinishing()) {
            android.os.Process.killProcess(android.os.Process.myPid());
        }
    }
}
