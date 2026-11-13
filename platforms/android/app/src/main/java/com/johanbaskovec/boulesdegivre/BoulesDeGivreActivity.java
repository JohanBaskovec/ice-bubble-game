package com.johanbaskovec.boulesdegivre;

import org.libsdl.app.SDLActivity;

/** Activite principale : SDL charge libSDL3.so puis libmain.so (le jeu). */
public class BoulesDeGivreActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }
}
