#include "tim_app.h"

static const char *const home_labels[TIM_HOME_ACTION_COUNT] = {
    "Game catalog",
    "Downloads",
    "Settings",
    "Exit"
};

void tim_app_init(TimAppState *app) {
    if (!app) return;
    app->screen = TIM_SCREEN_HOME;
    app->home_selection = 0;
    app->catalog_selection = 0;
    app->download_selection = 0;
    app->should_exit = 0;
}

void tim_app_home_move(TimAppState *app, int delta) {
    int next;
    if (!app || app->screen != TIM_SCREEN_HOME || delta == 0) return;
    next = (int)app->home_selection + (delta > 0 ? 1 : -1);
    if (next < 0) next = TIM_HOME_ACTION_COUNT - 1;
    if (next >= TIM_HOME_ACTION_COUNT) next = 0;
    app->home_selection = (unsigned int)next;
}

void tim_app_home_confirm(TimAppState *app) {
    if (!app || app->screen != TIM_SCREEN_HOME) return;
    switch (app->home_selection) {
        case TIM_HOME_CATALOG: app->screen = TIM_SCREEN_CATALOG; break;
        case TIM_HOME_DOWNLOADS: app->screen = TIM_SCREEN_DOWNLOADS; break;
        case TIM_HOME_SETTINGS: app->screen = TIM_SCREEN_SETTINGS; break;
        case TIM_HOME_EXIT: app->should_exit = 1; break;
        default: app->home_selection = 0; break;
    }
}

void tim_app_back(TimAppState *app) {
    if (!app) return;
    if (app->screen != TIM_SCREEN_HOME) {
        app->screen = TIM_SCREEN_HOME;
        return;
    }
    app->should_exit = 1;
}

const char *tim_app_home_label(unsigned int index) {
    if (index >= TIM_HOME_ACTION_COUNT) return "";
    return home_labels[index];
}
