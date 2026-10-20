#include "../src/tim_app.h"
#include <assert.h>
#include <string.h>

static void test_home_navigation_wraps(void) {
    TimAppState app;
    tim_app_init(&app);
    assert(app.screen == TIM_SCREEN_HOME);
    assert(app.home_selection == 0);
    tim_app_home_move(&app, -1);
    assert(app.home_selection == TIM_HOME_ACTION_COUNT - 1);
    tim_app_home_move(&app, 1);
    assert(app.home_selection == 0);
    tim_app_home_move(&app, 1);
    assert(app.home_selection == 1);
    tim_app_home_move(&app, 0);
    assert(app.home_selection == 1);
}

static void test_home_actions(void) {
    TimAppState app;
    tim_app_init(&app);
    tim_app_home_confirm(&app);
    assert(app.screen == TIM_SCREEN_CATALOG);
    tim_app_back(&app);
    assert(app.screen == TIM_SCREEN_HOME);
    tim_app_home_move(&app, 1);
    tim_app_home_confirm(&app);
    assert(app.screen == TIM_SCREEN_DOWNLOADS);
    tim_app_back(&app);
    tim_app_home_move(&app, 1);
    tim_app_home_confirm(&app);
    assert(app.screen == TIM_SCREEN_SETTINGS);
    tim_app_back(&app);
    tim_app_home_move(&app, 1);
    tim_app_home_confirm(&app);
    assert(app.should_exit == 1);
}

static void test_labels(void) {
    assert(strcmp(tim_app_home_label(0), "Game catalog") == 0);
    assert(strcmp(tim_app_home_label(3), "Exit") == 0);
    assert(strcmp(tim_app_home_label(4), "") == 0);
}

int main(void) {
    test_home_navigation_wraps();
    test_home_actions();
    test_labels();
    return 0;
}
