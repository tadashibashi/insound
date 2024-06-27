package io.insound;

import org.libsdl.app.SDLActivity;

import android.os.Bundle;

/// Example Activity implementation.
/// @note In this example, we're using SDL2 for windowing and input.
///       Our Activity inherits from SDLActivity since SDL2 requires a lot of glue code to function.
///       To run our `int main`, it requires our code to be loaded in a shared library with the
//        specific name "main".
///       If you're handling windowing and input by other means you don't need SDLActivity at all,
///       but will need to load your library yourself.
public class InsoundActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        /// Initialize the Insound's native runtime code
        io.insound.Insound.init(this);
    }

    @Override
    protected void onDestroy()
    {
        /// Clean up Insound's native runtime code
        io.insound.Insound.close();

        super.onDestroy();
    }
}
