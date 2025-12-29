//
// Created by 19571 on 2025/12/28.
//

#ifndef ABOLUO_EXIT_UI_INTERFACE_H
#define ABOLUO_EXIT_UI_INTERFACE_H
#include <stdbool.h>

void ui_show_notify_safe(bool is_blocking, bool show_bar, const char* text);

void app_update_progress_safe(int percent);

void app_close_notify_safe(void);

#endif //ABOLUO_EXIT_UI_INTERFACE_H