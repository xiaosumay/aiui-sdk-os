#ifndef _AIUI_UPLOAD_DYNAMIC_ENTITY_H
#define _AIUI_UPLOAD_DYNAMIC_ENTITY_H

typedef enum AIUI_DYNAMIC_LEVEL {
    AIUI_DYNAMIC_APP = 0,    // 应用级别的动态实体，比如打开爱奇艺/酷我等， 当前应用都可以使用
    AIUI_DYNAMIC_USER,    // 用户级别的动态实体，比如电话联系人，只有当前用户使用
    AIUI_DYNAMIC_CUSTOM,    // 自定义级别的动态实体，比如餐厅中菜谱名称，不同的餐厅菜谱可能不同,但是同一个餐厅里不同的设备共用同一个
    //菜谱实体，由管理设备上传菜谱名称，该餐厅里的设备共用该菜谱名称实体
    AIUI_DYNAMIC_MAX = 100
} AIUI_DYNAMIC_LEVEL_T;
/*
    param：实体内容，utf-8编码
    dynamic_name:实体名称，在aiui.xunfei.com技能工作室或文档中查询，比如联系人实体名称为：IFLYTEK.telephone_contact
    level：实体级别
    id:实体对应的id：需要websocket参数pers_param中支持，详见websocket参数和文档
        AIUI_DYNAMIC_APP: AIUI_APP_ID; 
        AIUI_DYNAMIC_USER: AIUI_AUTH_ID;
        AIUI_DYNAMIC_CUSTOM: 需要自己创建
*/
int aiui_upload_dynamic_entity(const char *param,
                               const char *dynamic_name,
                               const AIUI_DYNAMIC_LEVEL_T level,
                               const char *id);
/*
    实际用于调试，用来测试实体是否上传成功， sid的值在上传函数里打印出来的 必须等上传5s之后才能查
*/
int aiui_check_dynamic_entity(const char *sid);

#endif