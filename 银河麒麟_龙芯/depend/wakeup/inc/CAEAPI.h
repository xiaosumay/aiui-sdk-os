#pragma once

#include "Types.h"

// 类型定义CAE_HANDLE、cae_ivw_fn、cae_ivw_audio_fn、cae_audio_fn见Types.h

// windows版本
#ifdef _WIN32
#    ifdef __DLL_IMPLEMENT__
#        define C_API extern "C" __declspec(dllexport)
#    else
#        define C_API extern "C" __declspec(dllimport)
#    endif
#    define APIATTR
#else    // Linux及其他版本
#    ifndef APIATTR
#        define APIATTR    //__attribute__((visibility("default")))
#    endif
#    define C_API extern "C"
#endif

/* 
* 功能：初始化CAE实例
* 参数：
*     cae           实例地址[in|out]
*     enginecfg     引擎配置文件[in] 
*     ivwCb         唤醒信息回调[in]
*     ivwAudioCb    唤醒音频回调[in]
*     audioCb       识别音频回调[in] 
*     vtnCfg     中间层配置文件（指这动态库）[in]
*     userData      用户回调数据[in]
* 返回值： 0 成功，其他失败
*/
C_API int APIATTR CAENew(CAE_HANDLE *cae,
                         const char *sn,
                         cae_ivw_fn ivwCb,
                         cae_ivw_audio_fn ivwAudioCb,
                         cae_audio_fn audioCb,
                         const char *vtnCfg,
                         void *userData);
typedef int(APIATTR *Proc_CAENew)(CAE_HANDLE *cae,
                                  const char *sn,
                                  cae_ivw_fn ivwCb,
                                  cae_ivw_audio_fn ivwAudioCb,
                                  cae_audio_fn audioCb,
                                  const char *vtnCfg,
                                  void *userData);

/*
* 功能：重新加载资源,新引擎接口已变，且应用层用不到，暂不实现
* 参数：
*     cae           实例地址[in]
*     resPath       资源路径[in]
* 返回值： 0 成功，其他失败
*/
C_API int APIATTR CAEReloadResource(CAE_HANDLE cae, const char *resPath);
typedef int(APIATTR *Proc_CAEReloadResource)(CAE_HANDLE cae, const char *resPath);

/*
* 功能：重启降噪引擎，目前只重启了唤醒引擎的唤醒实例
* 参数：
*     cae           实例地址[in]
* 返回值： 0 成功，其他失败
*/
C_API int APIATTR CAERestart(CAE_HANDLE cae);
typedef int(APIATTR *Proc_CAERestart)(CAE_HANDLE cae);

/* 
* 功能：写入音频数据
* 参数：
*     cae           实例地址[in]
*     audioData     录音数据地址[in]
*     audioLen      录音数据长度[in]
* 返回值： 0 成功，其他失败
*/
C_API int APIATTR CAEAudioWrite(CAE_HANDLE cae, const void *audioData, unsigned int audioLen);
typedef int(APIATTR *Proc_CAEAudioWrite)(CAE_HANDLE cae,
                                         const void *audioData,
                                         unsigned int audioLen);

/*
* 功能：设置麦克风编号
* 参数：
*     cae           实例地址[in]
*     beam          麦克风编号[in]
* 返回值： 0 成功，其他失败
*/
C_API int APIATTR CAESetRealBeam(CAE_HANDLE cae, int beam);
typedef int(APIATTR *Proc_CAESetRealBeam)(CAE_HANDLE cae, int beam);

/*
* 功能：根据参数id设置参数值，可设置参数参考ivCAEParam.h
* 参数：
*     cae           实例地址[in]
*     id            参数编号[in]
*     value         设置的参数值[in]
* 返回值： 0 成功， 其他失败
*/
C_API int APIATTR CAESetParameter(CAE_HANDLE cae, int id, const char *value);
typedef int(APIATTR *Proc_CAESetParameter)(CAE_HANDLE cae, int id, const char *value);

/*
* 功能：根据参数id获取参数值，可取参数参考ivCAEParam.h
* 参数：
*     cae           实例地址[in]
*     id            参数编号[in]
*     value         设置的参数值[out]
*     len           获取的参数值的长度[out]
* 返回值： 0 成功， 其他失败
*/
C_API int APIATTR CAEGetParameter(CAE_HANDLE cae, int id, char *value, unsigned int &len);
typedef int(APIATTR *Proc_CAEGetParameter)(CAE_HANDLE cae, int id, char *value, int &len);

/*
* 功能：获取CAE版本号
* 参数：
* 返回值：CAE版本号字符串
*/
C_API const char APIATTR *CAEGetVersion();
typedef const char *(APIATTR *Proc_CAEGetVersion)();

/* 
* 功能：销毁实例
* 参数：
*     cae           实例地址[in]
* 返回值：0 成功，其他失败
*/
C_API int APIATTR CAEDestroy(CAE_HANDLE cae);
typedef int(APIATTR *Proc_CAEDestroy)(CAE_HANDLE cae);

/* 
* 功能：设置日志级别
* 参数：
*     logLevel      日志级别[in] 1 debug, 2 info, 3 warn, 4 error, 5 fatal
* 返回值：0 成功， 其他失败
*/
C_API int APIATTR CAESetShowLog(int logLevel);
typedef int(APIATTR *Proc_CAESetShowLog)(int logLevel);

/* 
* 功能： 请求鉴权
* 参数：
*     sn            设备授权编号[in]
* 返回值：0 授权通过，其他失败
*/
C_API int APIATTR CAEAuth(CAE_HANDLE cae, const char *sn);
typedef int(APIATTR *Proc_CAEAuth)(CAE_HANDLE cae, const char *sn);

/*
* 功能： 获取角度和波束编号
* 参数：
*     cae            实例地址[in]
*     pAngle         当前mic角度[out]
*     pBeam          当前mic编号[out]
* 返回值：0 成功，其他失败
*/
C_API int APIATTR CAEGetAngleBeam(CAE_HANDLE cae, int *pAngle, int *pBeam);
typedef int(APIATTR *Proc_CAEGetAngleBeam)(CAE_HANDLE cae, int *pAngle, int *pBeam);

/*
* 功能： 获取指定波束能量值
* 参数：
*     cae            实例地址[in]
*     beam           波束值[in]
*     power          能量值[out]
* 返回值：0 成功，其他失败
*/
C_API int APIATTR CAEGetPower(CAE_HANDLE cae, int beam, float *power, float *curpower);
typedef int(APIATTR *Proc_CAEGetPower)(CAE_HANDLE cae, int *beam, float *power, float *curpower);

/*
* 功能： 获取待处理缓冲区里帧的数量
* 参数： 无
* 返回值：待处理缓冲区里帧的数量
*/
C_API int APIATTR CAEGetFrameSize(CAE_HANDLE cae);
typedef int(APIATTR *Proc_CAEGetFrameSize)(CAE_HANDLE cae);

/*
* 功能： 获取声纹检测结果
* 参数： 
*     cae         实例地址[in] 
*     pdata       存放结果的指针
*     max_len     缓冲区最多长度
*     out_len     结果内容的时间长度 
* 返回值：待处理缓冲区里帧的数量
*/
C_API int APIATTR CAEGetVprResult(CAE_HANDLE cae, char *pdata, int max_len, int *out_len);
typedef int(APIATTR *Proc_CAEGetVprResult)(CAE_HANDLE cae, char *pdata, int max_len, int *out_len);

/*
* 功能： 获取性别年龄检测结果
* 参数：
*     cae         实例地址[in]
*     pdata       存放结果的指针
*     max_len     缓冲区最多长度
*     out_len     结果内容的时间长度
* 返回值：待处理缓冲区里帧的数量
*/
C_API int APIATTR CAEGetGenderResult(CAE_HANDLE cae, char *pdata, int max_len, int *out_len);
typedef int(APIATTR *Proc_CAEGetGenderResult)(CAE_HANDLE cae,
                                              char *pdata,
                                              int max_len,
                                              int *out_len);

/*
* 功能： 单独获取唤醒角度
* 参数：
*     cae         实例地址[in]
*     start_ms       唤醒开始时间
*     end_ms     唤醒结束时间
* 返回值：唤醒角度
*/
C_API float APIATTR CAEGetWakeUpAngle(CAE_HANDLE cae, int start_ms, int end_ms);
typedef float(APIATTR *Proc_CAEGetWakeUpAngle)(CAE_HANDLE cae, int start_ms, int end_ms);

/***************************新增vpr训练接口***************************/
/*
* 功能：启动Vpr声纹训练
* 参数：
*      vpr_save_path   训练后的vpr资源保存路径
*      res_cb          用户注册的回调函数用于反馈训练状态
*      user_data       回调时返回给用户的私有数据的指针
*  返回值： 0 成功，其它失败
*/
C_API int APIATTR VprStartTrain(CAE_HANDLE cae,
                                const char *vpr_save_path,
                                vpr_train_res_cb res_cb,
                                void *user_data);
typedef int(APIATTR *Proc_StartVprTrain)(CAE_HANDLE cae,
                                         const char *vpr_save_path,
                                         vpr_train_res_cb res_cb,
                                         void *user_data);

/*
* 功能： 获取声纹训练的状态
* 参数：
*        status  训练状态： 1 训练中， 2 训练成功, 3 训练失败， 4 未启动训练
*        resc    已收录的有效音频条数
*        total_recs 需要收录的总数量
*/
C_API int APIATTR VprGetStatus(CAE_HANDLE cae, int *status, int *recs, int *total_recs);
typedef int(APIATTR *Proc_GetVprStatus)(CAE_HANDLE cae, int *status, int *recs, int *total_recs);

/*
* 功能：停止Vpr声纹训练
* 参数：
* 返回值：0 成功，其它失败
*/
C_API int APIATTR VprStopTrain(CAE_HANDLE cae);
typedef int(APIATTR *Proc_StopVprTrain)(CAE_HANDLE cae);
/***************************新增vpr训练接口***************************/

//功能:callback(应用层注册到API的接口), 抛出打印日志
C_API void APIATTR ResLogCB(log_res_cb log_write_cb);