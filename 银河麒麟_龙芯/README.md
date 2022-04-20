# LongArch64 平台 AIUI SDK

## 1 功能简介

- 通过websocket与AIUI云端进行交互，获取语音识别、语义理解、语音合成结果。协议内容详见https://aiui.xfyun.cn/doc/aiui/3_access_service/access_interact/websocket.html。

- 提供对云端下发的识别、语义、合成结果解析和处理的基础实现，开发者可以在此基础上更一步完善，打造自己的功能。

- 内容播放接口定义。定义合成音频，音乐相关的播放和控制接口，开发者针对自己的平台添加相应实现，即可以实现内容播放功能。

## 2 适配需求

- SDK逻辑简单，对算力要求不高，适用于使用C语言开发，运行linux，rtos等操作系统的嵌入式设备。只需要简单的适配即可运行起来，适配要求如下：

    | 适配要求 | 说明 |
    | :------| :------ |
    | 录音 | 调用目标平台录音接口，录制16K采样率，16bit编码的PCM音频 |
    | OS相关接口   | 定义在aiui_wrappers_os.h中，包括信号量，锁和线程等接口，需要开发者针对特定的平台实现。aiui_os_linux.c即为针对linux实现的参考示例代码 |
    | socket接口  | 在某些平台上，aiui_socket_api.c中的接口要改成平台相关实现。大部分平台都不需求适配 |
    | url播放  | 需要播放的资源都是以url形式下发（具体格式为mp3，设备端需要自行实现相应的播放器|
    | 加密交互 | 依赖mbedtls实现，如果不支持，将aiui_socket_api.c中的AIUI_SUPPORT_SSL宏注释 |

## 3 快速体验

- 以下介绍在Ubuntu（其他Linux发行版本类似，只需改成相应的包安装命令即可）Linux PC上如何快速体验SDK demo。

- 安装必需软件：

    ```shell
    sudo apt install cmake
    sudo apt install libasound2-dev
    ```

- 编译、运行：

    ```shell
    mkdir build
    cd build
    cmake ..
    make
    ./aiui_demo
    ```
- 通过语音唤醒:
    唤醒词 小薇小薇
- 输入命令：

    按提示输入相应的命令后回车即可体验相应功能。示例：输入“real”后回车，即可开启实时语音交互，这时用户可以说出”今天天气怎么样？“，然后等待结果打印。

## 4 SDK集成

 - SDK以源码方式提供，需要直接将源文件加入到工程中编译。

 - 源代码介绍:
    
    | 头文件              | 内容说明 |
    | :----------------- | :------ |
    | aiui_api.h         | aiui交互接口 |
    | aiui_log.h         | log打印接口 |
    | aiui_audio_control.h | 音频播放控制接口，需要开发者在相应的.c文件自行实现 |
    | aiui_convert_url.h | 酷我音乐授权及itemID转url功能，需要接入音乐版权内容时使用 |
    | aiui_socket_api.h  | socket接口的二次封装，某些平台需要在.c文件自行实现相应函数 |
    | aiui_http_api.h      | http相关接口 |
    | aiui_websocket_api.h | websocket相关接口 |
    | aiui_message_parse.h | 对云端下发的json数据进行解析，解析出识别(iat)，语义(nlp)，合成(tts)结果 |
    | aiui_skill_parse.h   | 对云端的语义技能进行解析，目前仅对媒体资源，播放控制命令进行解析。若有其他技能需求(比如自定义技能)，可在.c文件添加实现 |
    | aiui_media_parse.h    | 解析tts url，以及媒资类url |
    | aiui_player .h      | 只是定义url播发器播放，暂停，继续播放等接口，需要自己去实现。也可以删除，然后用aiui_audio_control.h中的接口替换 |
    | base64.h，cjosn.h，md5.h，sha256.h | base64编解码，json解析以及Hash计算。如果与系统本身自带的函数有冲突，先删掉，然后在代码中引用系统自带接口即可 |
    | aiui_device_info.h | aiui交互需要的appid，apikey等必要信息 |

    与头文件对应的.c文件在aiui/src目录下。

- aiui主要接口：

    | 名称 | 说明 |
    | :------| :------ |
    | aiui_init    | 创建一个AIUI交互实例 |
    | aiui_start   | 一般在唤醒或者按键响应之后调用，用于开启一轮会话 |
    | aiui_send    | 向云端发送音频或文本数据 |
    | aiui_stop    | 向云端发送数据结束 |
    | aiui_uninit  | 销毁AIUI交互实例，释放相应资源 |

     <font color="red">主要接口在aiui_api.h中，每个函数的具体参数详见接口注释。</font>

## 5 调用示例

- 文本交互

```c
    // 定义回调函数
    static void on_event(void *param, AIUI_EVENT_TYPE type, const char *msg)
    {
        switch (type) {
            case AIUI_EVENT_ANSWER:
                break;

            case AIUI_EVENT_FINISH:
                aiui_hal_sem_post(s_sem_finish);
                break;
        }
    }

    static const char *s_tts_text = "今天天气怎么样";

    // 创建交互实例，文本交互和语音交互可以共用一个实例
    s_aiui_interactive_handle = aiui_init(AIUI_AUDIO_INTERACTIVE, on_event, NULL);

    // 开启文本交互
    aiui_start(s_aiui_interactive_handle, AIUI_TEXT_INTERACTIVE);

    // 直接发送文本。文本交互时，不需要等待AIUI_EVENT_START_SEND事件
    aiui_send(s_aiui_interactive_handle, s_tts_text, strlen(s_tts_text));
    
    // 发送完成
    aiui_stop(s_aiui_interactive_handle);
```

- 语音合成

```c
    // 定义回调函数
    static void on_event_tts(void *param, AIUI_EVENT_TYPE type, const char *msg)
    {
        switch (type) {
            case AIUI_EVENT_FINISH:
                aiui_hal_sem_post(s_sem_finish);
                break;
        }
    }

    static const char *s_tts_text = "今天天气怎么样";
    
    // 创建语音合成实例。注意：合成需要单独的实例，不能与交互实例共用
    s_aiui_tts_handle = aiui_init(AIUI_TTS, on_event_tts, NULL);
    
    // 开启语音合成
    aiui_start(s_aiui_tts_handle, AIUI_TTS);
    
    // 直接发送文本。语音合成时，不需要等待AIUI_EVENT_START_SEND事件
    aiui_send(s_aiui_tts_handle, s_tts_text, strlen(s_tts_text));
    
    // 发送完成
    aiui_stop(s_aiui_tts_handle);
```
- 语音交互

```c
    详见aiui_main.c源文件。
```
## 6 注意事项

- aiui_device_info.h中的设备和应用信息，在实际使用时要替换成自己的，并且需要确保每台设备的auth_id唯一。

- 音频交互时，调用aiui_start到收到AIUI_EVENT_START_SEND事件会有一定延迟，所以在调用aiui_start后，开始采集录音并保存到缓存队列中，收到AIUI_EVENT_START_SEND，才开始从缓存队列中获取数据，调用aiui_send向云端发送数据，收到AIUI_EVENT_STOP_SEND，停止发送数据。

- 场景参数scene设置。一般地，对于平台上创建的场景A，scene=A表示使用正式环境，scene=A_box则会使用测试环境。所以如果您的产品在测试时scene使用main_box，产品正式上线时需要改成main。

