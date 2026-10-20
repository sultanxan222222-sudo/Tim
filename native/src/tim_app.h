#ifndef TIM_APP_H
#define TIM_APP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TimScreen {
    TIM_SCREEN_HOME = 0,
    TIM_SCREEN_CATALOG,
    TIM_SCREEN_DOWNLOADS,
    TIM_SCREEN_SETTINGS
} TimScreen;

typedef enum TimHomeAction {
    TIM_HOME_CATALOG = 0,
    TIM_HOME_DOWNLOADS,
    TIM_HOME_SETTINGS,
    TIM_HOME_EXIT,
    TIM_HOME_ACTION_COUNT
} TimHomeAction;

typedef struct TimAppState {
    TimScreen screen;
    unsigned int home_selection;
    unsigned int catalog_selection;
    unsigned int download_selection;
    int should_exit;
} TimAppState;

void tim_app_init(TimAppState *app);
void tim_app_home_move(TimAppState *app, int delta);
void tim_app_home_confirm(TimAppState *app);
void tim_app_back(TimAppState *app);
const char *tim_app_home_label(unsigned int index);

#ifdef __cplusplus
}
#endif

#endif
