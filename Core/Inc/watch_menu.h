#ifndef LV_MENU_IMPL_H
#define LV_MENU_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief 切换到功能菜单页面 (总入口)
     * * 调用此函数将创建或加载包含时间设置和查找手机功能的 UI 页面。
     */
    void switchToMenu(void);

    /**
     * @brief 触发“查找手表”警报
     * * 当收到手机端发送的查找指令时调用此函数。
     * 它会显示全屏警告弹窗，并在底层实现中触发蜂鸣器/震动。
     */
    void trigger_find_my_watch(void);

    /**
     * @brief 停止“查找手表”警报
     * * 当手机端取消查找，或用户在手表端点击停止时调用。
     * 它会隐藏弹窗并停止蜂鸣器/震动。
     */
    void stop_find_my_watch(void);

#ifdef __cplusplus
}
#endif

#endif // LV_MENU_IMPL_H