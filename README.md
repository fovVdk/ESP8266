# ESP8266
esp8266 wifi complete example
目录结构
usr目录--
user_main.c 程序主入口
wifi.c  创建tcp服务及tcp传输的机制
Zyt_json.c json数据包处理
lp_uart.c uart串口机制
base64.c base64加密
update.c 固件升级
fifo.c fifo存储器调度算法
sprintf.c 字符串格式化
utils.c 字符校验工具
include目录--
应用程序相关头文件
driver目录--
外围驱动支持I2C,SPI,外部按键，PWM，双UART
结构介绍：
1.主入口
user_rf_cal_sector_set（flash初始化）---调用头文件c_types.h(数据类型）user_interface.h(接口函数)
wifi联网检测模块
LED初始化
参数初始化
初始化运行动作
入口程序
2.创建tcp服务及tcp传输的机制
