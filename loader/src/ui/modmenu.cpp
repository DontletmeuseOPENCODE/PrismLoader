#include <cstdio>
#include <string>
#include <vector>

namespace prism {

class ModMenu {
public:
    void show();
    void hide();

    bool isVisible() const { return visible; }

private:
    bool visible = false;
};

void ModMenu::show() {
    visible = true;
    FILE* log = fopen("/tmp/prismloader.log", "a");
    if (log) {
        fprintf(log, "[Prism] In-game menu shown (stub)\n");
        fclose(log);
    }
}

void ModMenu::hide() {
    visible = false;
}

} // namespace prism
