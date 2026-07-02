#pragma once

namespace ScreenBracket {
    // bracketView : 0 = arbre final (1/4 -> finale), 1-4 = regions (32es -> 1/4)
    void draw();
    void update();

    int  viewCount();   // nombre de vues (arbre + regions disponibles)
}
