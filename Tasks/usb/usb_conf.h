/*设置usb相关的事件组成员定义*/
#ifndef USB_CONF_H
#define USB_CONF_H

//usb插拔事件组
#define USB_EVT_READY        (1UL << 0)   /* USB 设备连接 */
#define USB_EVT_DISCONNECT   (1UL << 1)   /* USB 设备断开 */

//挂载消息事件组
#define MOUNT_EVT_IDLE         (1UL << 0)   /* USB 设备空闲 */
#define MOUNT_EVT_READY        (1UL << 1)   /* USB 设备连接，文件系统挂载成功 */
#define MOUNT_EVT_DISCONNECT   (1UL << 2)   /* USB 设备断开 */

//UI 消息事件组
#define UI_MSG_EVT_DISC        (1UL << 0)   /* USB 设备断开消息事件 */
#define UI_MSG_EVT_UPDATE      (1UL << 1)   /* 更新文件列表事件 */

#endif  /* USB_CONF_H */