/**
 * android_ble_client.h
 * * 这是一个基于 Apache NimBLE host stack 的封装层，旨在提供
 * 类似于 Android BluetoothGatt 和 BluetoothLeScanner 的 C 语言接口。
 */

#ifndef ANDROID_BLE_CLIENT_H
#define ANDROID_BLE_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* 回调定义 (对应 BluetoothGattCallback / ScanCallback)                       */
/* -------------------------------------------------------------------------- */

typedef struct {
    /**
     * 对应 onScanResult
     * @param addr_str 设备地址字符串 (e.g. "11:22:33:AA:BB:CC")
     * @param rssi 信号强度
     * @param adv_data 广播数据指针
     * @param adv_len 广播数据长度
     */
    void (*on_scan_result)(const char *addr_str, int rssi, const uint8_t *adv_data, int adv_len);

    /**
     * 对应 onConnectionStateChange
     * @param conn_handle 连接句柄
     * @param status 操作状态 (0 为成功)
     * @param new_state 新状态 (0: Disconnected, 2: Connected)
     */
    void (*on_connection_state_change)(uint16_t conn_handle, int status, int new_state);

    /**
     * 对应 onServicesDiscovered
     * @param conn_handle 连接句柄
     * @param status 操作状态
     */
    void (*on_services_discovered)(uint16_t conn_handle, int status);

    /**
     * 对应 onCharacteristicRead
     */
    void (*on_characteristic_read)(uint16_t conn_handle, int status, uint16_t char_handle, const uint8_t *data, uint16_t len);

    /**
     * 对应 onCharacteristicWrite
     */
    void (*on_characteristic_write)(uint16_t conn_handle, int status, uint16_t char_handle);

    /**
     * 对应 onCharacteristicChanged (Notify/Indicate)
     */
    void (*on_characteristic_changed)(uint16_t conn_handle, uint16_t char_handle, const uint8_t *data, uint16_t len);

    /**
     * 对应 onMtuChanged
     */
    void (*on_mtu_changed)(uint16_t conn_handle, int mtu, int status);

} android_ble_callbacks_t;

/* -------------------------------------------------------------------------- */
/* API 接口                                                                   */
/* -------------------------------------------------------------------------- */

/**
 * 初始化 BLE 客户端模块
 * @param callbacks 回调函数结构体指针
 */
void android_ble_init(android_ble_callbacks_t *callbacks);

/**
 * 对应 BluetoothLeScanner.startScan()
 * 开始扫描 BLE 设备
 * @return 0 表示成功，非 0 表示错误码
 */
int android_ble_start_scan(void);

/**
 * 对应 BluetoothLeScanner.stopScan()
 * 停止扫描
 */
int android_ble_stop_scan(void);

/**
 * 对应 BluetoothDevice.connectGatt()
 * 连接到指定设备
 * @param addr_str 目标设备地址字符串 (格式 "XX:XX:XX:XX:XX:XX")
 * @param addr_type 地址类型 (0: Public, 1: Random)
 */
int android_ble_connect(const char *addr_str, uint8_t addr_type);

/**
 * 对应 BluetoothGatt.disconnect()
 * 断开当前连接
 */
int android_ble_disconnect(void);

/**
 * 对应 BluetoothGatt.discoverServices()
 * 发现服务（这会递归触发发现特征值）
 */
int android_ble_discover_services(void);

/**
 * 对应 BluetoothGatt.readCharacteristic()
 * 读取特征值
 * @param char_handle 特征值的 Attribute Handle (注意：不是 UUID)
 */
int android_ble_read_char(uint16_t char_handle);

/**
 * 对应 BluetoothGatt.writeCharacteristic()
 * 写入特征值
 * @param char_handle 特征值的 Attribute Handle
 * @param data 要写入的数据
 * @param len 数据长度
 * @param type 写入类型 (1: No Response, 2: With Response)
 */
int android_ble_write_char(uint16_t char_handle, const uint8_t *data, uint16_t len, int type);

/**
 * 对应 BluetoothGatt.setCharacteristicNotification() + writeDescriptor()
 * 启用或禁用通知/指示
 * @param cccd_handle 该特征对应的 CCCD (Client Characteristic Configuration Descriptor) Handle
 * @param enable true 开启, false 关闭
 * @param is_indication true 为 Indication, false 为 Notification
 */
int android_ble_enable_notification(uint16_t cccd_handle, bool enable, bool is_indication);

/**
 * 对应 BluetoothGatt.requestMtu()
 * 请求更改 MTU 大小
 * @param mtu 请求的 MTU 大小 (通常最大 517)
 */
int android_ble_request_mtu(int mtu);

#ifdef __cplusplus
}
#endif

#endif // ANDROID_BLE_CLIENT_H