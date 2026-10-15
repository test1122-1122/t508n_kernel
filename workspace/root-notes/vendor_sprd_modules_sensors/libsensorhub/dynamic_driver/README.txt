    构建动态加载驱动需包含dynamic_driver.h，sensorhub_interface.h，sensorhub_interface.c文件和
(平台名)_dynamic_driver文件夹下的platform_config.h，然后根据项目需求，参考dynamc_driver_(xxxx).c文件
实现动态加载驱动。
1. dynamic_driver.h定义动态加载驱动需要实现的接口原型；
2. sensorhub_interface.h，sensorhub_interface.c两个文件定义了动态加载驱动中，
   需要使用的I2C读写接口，sleep延时接口，log打印接口实现；
3. (平台名)_dynamic_driver文件夹下的platform_config.h为对应平台相关配置，构建此平台加载驱动需要包含。
   dynamic_driver_(平台名).ld为构建对应平台动态加载驱动需要使用的链接脚本。
   上述两个文件不同平台不可混用。
   dynamc_driver_(xxxx).c为动态加载驱动接口的具体实现，开发者可参考现有软件自行修改客制化。
4. 动态加载驱动详细介绍，参考文档《SensorHub动态加载驱动指导》、《EmBitz编译Sensorhub动态加载驱动介绍》pressure_driver
   文档可以从iSupport网站获取；
5. akm_cali文件夹包含的是akm09918 sensor动态加载接口实现，在构建包含此sensor的动态加载工程时，将此文件夹加入工程；
6. ltr578_driver_interface文件夹为ltr578 sensor动态加载驱动接口实现，如需使用，将此文件加入工程编译；
7. pressure_driver 文件夹为bmp280/380/390 pressure sensor动态加载驱动接口实现，如需使用，将此文件加入工程编译；
8. sc7a20_driver_interface 文件夹为士兰微sc7a20动态加载驱动接口实现，如需使用，将此文件夹加入工程编译；