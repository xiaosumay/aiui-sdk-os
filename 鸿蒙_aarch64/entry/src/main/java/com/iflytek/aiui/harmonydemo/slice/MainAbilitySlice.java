package com.iflytek.aiui.harmonydemo.slice;

import android.content.Context;
import com.iflytek.aiui.*;
import com.iflytek.aiui.harmonydemo.ResourceTable;
import ohos.aafwk.ability.AbilitySlice;
import ohos.aafwk.content.Intent;
import ohos.agp.components.Button;
import ohos.agp.components.Component;
import ohos.agp.components.TextField;
import ohos.agp.utils.LayoutAlignment;
import ohos.agp.window.dialog.ToastDialog;
import ohos.agp.window.service.WindowManager;
import ohos.global.resource.RawFileEntry;
import ohos.global.resource.Resource;
import ohos.global.resource.ResourceManager;
import ohos.hiviewdfx.HiLog;
import ohos.hiviewdfx.HiLogLabel;
import org.json.JSONObject;

import java.io.*;
import java.lang.reflect.Method;

public class MainAbilitySlice extends AbilitySlice implements Component.ClickedListener {
    private static HiLogLabel TAG = new HiLogLabel(HiLog.LOG_APP, 0x0102, "aiuidemo");

    private TextField mResultText;
    private Button mCreateBtn;
    private Button mDestroyBtn;
    private Button mWriteAudioBtn;
    private Button mStopWriteBtn;
    private Button mTextBtn;
    private Button mClearBtn;

    @Override
    public void onStart(Intent intent) {
        super.onStart(intent);
        super.setUIContent(ResourceTable.Layout_ability_main);

        getWindow().setLayoutFlags(WindowManager.LayoutConfig.MARK_ALT_FOCUSABLE_IM,
                WindowManager.LayoutConfig.MARK_ALT_FOCUSABLE_IM);

        initUI();
    }

    @Override
    public void onActive() {
        super.onActive();
    }

    @Override
    public void onForeground(Intent intent) {
        super.onForeground(intent);
    }

    @Override
    protected void onStop() {
        super.onStop();

        destroyAgent();
    }

    private void initUI() {
        mResultText = findComponentById(ResourceTable.Id_tf_result);
        mCreateBtn = findComponentById(ResourceTable.Id_btn_create);
        mDestroyBtn = findComponentById(ResourceTable.Id_btn_destroy);
        mWriteAudioBtn = findComponentById(ResourceTable.Id_btn_write);
        mStopWriteBtn = findComponentById(ResourceTable.Id_btn_stop_write);
        mTextBtn = findComponentById(ResourceTable.Id_btn_text);
        mClearBtn = findComponentById(ResourceTable.Id_btn_clear);

        mCreateBtn.setClickedListener(this);
        mDestroyBtn.setClickedListener(this);
        mWriteAudioBtn.setClickedListener(this);
        mStopWriteBtn.setClickedListener(this);
        mTextBtn.setClickedListener(this);
        mClearBtn.setClickedListener(this);
    }

    @Override
    public void onClick(Component component) {
        switch (component.getId()) {
            case ResourceTable.Id_btn_create: {
                createAgent();
            } break;

            case ResourceTable.Id_btn_destroy: {
                destroyAgent();
            } break;

            case ResourceTable.Id_btn_write: {
                writeAudio();
            } break;

            case ResourceTable.Id_btn_stop_write: {
                stopWrite();
            } break;

            case ResourceTable.Id_btn_text: {
                writeText();
            } break;

            case ResourceTable.Id_btn_clear: {
                mResultText.setText("");
            } break;
        }
    }

    private void destroyAgent() {
        if (mAIUIAgent != null) {
            mAIUIAgent.destroy();
            mAIUIAgent = null;

            showTip("成功销毁");
        }
    }

    private String readAIUICfg() {
        ResourceManager resManager = getResourceManager();
        RawFileEntry entry = resManager.getRawFileEntry("resources/rawfile/cfg/aiui_phone.cfg");

        try {
            Resource res = entry.openRawFile();
            byte[] buffer = new byte[res.available()];
            res.read(buffer);
            res.close();

            return new String(buffer);
        } catch (IOException e) {
            e.printStackTrace();
        }

        return "";
    }

    private byte[] readPcm() {
        ResourceManager resManager = getResourceManager();
        RawFileEntry entry = resManager.getRawFileEntry("resources/rawfile/test.pcm");

        try {
            Resource res = entry.openRawFile();
            byte[] buffer = new byte[res.available()];
            res.read(buffer);
            res.close();

            return buffer;
        } catch (IOException e) {
            e.printStackTrace();
        }

        return null;
    }

    private void createAgent() {
        if (mAIUIAgent != null) {
            return;
        }

        AIUISetting.setSystemInfo(AIUIConstant.KEY_SERIAL_NUM, "1234abcd");

        mAIUIAgent = AIUIAgent.createAgent(getAndroidContext(), readAIUICfg(),
                mAIUIListener);

        showTip("创建成功");
    }

    private void writeAudio() {
        if (mWriteThread !=  null) {
            return;
        }

        mWriteThread = new WriteThread();
        mWriteThread.start();

        showTip("开始写音频");
    }

    private void stopWrite() {
        if (mWriteThread != null) {
            mWriteThread.stopRun();
            mWriteThread = null;
        }

        showTip("停止写入");
    }

    private void writeText() {
        if (mAIUIAgent != null) {
            AIUIMessage wakeupMessage = new AIUIMessage(AIUIConstant.CMD_WAKEUP, 0, 0,
                    "", null);
            mAIUIAgent.sendMessage(wakeupMessage);

            AIUIMessage writeMessage = new AIUIMessage(AIUIConstant.CMD_WRITE, 0, 0,
                    "data_type=text", "你会干什么呢？".getBytes());
            mAIUIAgent.sendMessage(writeMessage);
        }
    }

    private void appendResult(String result) {
        if (mResultText.length() > 5000) {
            mResultText.setText("");
        }

        mResultText.append(result + "\n\n");
    }

    private AIUIAgent mAIUIAgent;

    private final AIUIListener mAIUIListener = new AIUIListener() {
        @Override
        public void onEvent(AIUIEvent event) {
            switch (event.eventType) {
                case AIUIConstant.EVENT_CONNECTED_TO_SERVER:
                    showTip("已连接服务器");
                    break;

                case AIUIConstant.EVENT_SERVER_DISCONNECTED:
                    showTip("与服务器断连");
                    break;

                case AIUIConstant.EVENT_WAKEUP:
                    showTip( "进入识别状态" );
                    break;

                case AIUIConstant.EVENT_VAD: {
                    HiLog.debug(TAG, "EVENT_VAD, arg1=%{public}d, arg2=%{public}d",
                            event.arg1, event.arg2);
                } break;

                case AIUIConstant.EVENT_RESULT: {
                    try {
                        JSONObject bizParamJson = new JSONObject(event.info);
                        JSONObject data = bizParamJson.getJSONArray("data").getJSONObject(0);
                        JSONObject params = data.getJSONObject("params");
                        JSONObject content = data.getJSONArray("content").getJSONObject(0);
                        String sub = params.optString("sub");

                        if (content.has("cnt_id") && !"tts".equals(sub)) {
                            String cnt_id = content.getString("cnt_id");
                            String cntStr = new String(event.data.getByteArray(cnt_id), "utf-8");

                            // 获取该路会话的id，将其提供给支持人员，有助于问题排查
                            // 也可以从Json结果中看到
                            String sid = event.data.getString("sid");
                            String tag = event.data.getString("tag");

                            appendResult(cntStr);

                            HiLog.debug(TAG, "result=" + cntStr);
                        }
                    } catch (Throwable e) {
                        e.printStackTrace();

                        appendResult(e.getLocalizedMessage());
                    }
                } break;

                case AIUIConstant.EVENT_ERROR: {
                    appendResult("error=" + event.arg1 + ", des=" + event.info);
                } break;

                default:
            }
        }
    };

    private WriteThread mWriteThread;

    private class WriteThread extends Thread {
        private boolean mNeedStop;

        public void stopRun() {
            mNeedStop = true;
            interrupt();
        }

        @Override
        public void run() {
            byte[] pcmData = readPcm();
            int dataLen = (pcmData == null) ? 0 : pcmData.length;

            HiLog.debug(TAG, "pcmLen=%{public}d", dataLen);

            if (dataLen == 0) {
                return;
            }

            if (mAIUIAgent != null) {
                AIUIMessage wakeupMessage = new AIUIMessage(AIUIConstant.CMD_WAKEUP, 0, 0,
                        "", null);
                mAIUIAgent.sendMessage(wakeupMessage);
            }

            int offset = 0;

            HiLog.debug(TAG, "start write");

            while (!mNeedStop) {
                int left = dataLen - offset;
                int copyLen = Math.min(left, 1280);
                byte[] buffer = new byte[copyLen];

                System.arraycopy(pcmData, offset, buffer, 0, copyLen);
                if (mAIUIAgent != null) {
                    AIUIMessage writeMessage = new AIUIMessage(AIUIConstant.CMD_WRITE, 0, 0,
                            "data_type=audio", buffer);
                    mAIUIAgent.sendMessage(writeMessage);
                }

                offset += copyLen;

                if (offset == dataLen) {
                    break;
                }

                try {
                    Thread.sleep(10);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }

            HiLog.debug(TAG, "stop write");

            if (mAIUIAgent != null) {
                AIUIMessage stopWriteMessage = new AIUIMessage(AIUIConstant.CMD_STOP_WRITE, 0, 0,
                        "data_type=audio", null);
                mAIUIAgent.sendMessage(stopWriteMessage);
            }

            stopWrite();
        }
    }

    private ToastDialog mToast;

    private void showTip(final String tip) {
        getMainTaskDispatcher().asyncDispatch(() -> {
            if (mToast == null) {
                mToast = new ToastDialog(getContext());
            }

            mToast.setText(tip).setAlignment(LayoutAlignment.CENTER).show();
        });
    }

    /**
     * 取得Android Context对象。
     *
     * @return Context
     */
    public static Context getAndroidContext() {
        try {
            Class<?> ActivityThread = Class.forName("android.app.ActivityThread");

            // 获取currentActivityThread 对象
            Method method = ActivityThread.getMethod("currentActivityThread");
            Object currentActivityThread = method.invoke(ActivityThread);

            // 获取Context对象
            Method method2 = currentActivityThread.getClass().getMethod("getApplication");
            Context context = (Context) method2.invoke(currentActivityThread);

            return context;
        } catch (Exception e) {
            e.printStackTrace();
        }

        return null;
    }

}
