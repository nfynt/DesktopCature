using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using uWindowCapture;

[RequireComponent(typeof(UwcWindowTexture))]
public sealed class StreamTest : MonoBehaviour
{
    [SerializeField]
    private bool isStreaming;
    [SerializeField]
    private string rtspURL="rtsp://localhost:8554/test";        //rtsp://195.201.128.31:8554/test
    [SerializeField]
    private int targetFps = 24;
    [SerializeField]
    private bool enableEncryption;
    [SerializeField]
    private int win_id;

    IEnumerator Start()
    {
        //Register Unity function for rtsp interrupt callback
        Lib.RTSPStreamInterrupted(RtspInterruptCallback);
        yield return new WaitForSeconds(2f);
        win_id = GetComponent<UwcWindowTexture>().window.id; 
    }

    void RtspInterruptCallback(string info)
    {
        Debug.LogError("Rtsp stream interrupted: " + info);
        //if stream send stop stream event
    }

    private void Update()
    {
        if(Input.GetKeyDown(KeyCode.P) && GetComponent<UwcWindowTexture>().window.isAlive)
        {
            if (isStreaming)
            {
                Debug.Log("Stopping rtsp streaming");
                Lib.StopRTSPStream();
                isStreaming = false;
            }
            else
            {
                Debug.Log("Starting rtsp streaming");
                win_id = GetComponent<UwcWindowTexture>().window.id;
                isStreaming = Lib.StartRTSPStream(win_id, rtspURL,targetFps, enableEncryption);
                if (!isStreaming) Debug.Log("[Unity]Failed to start RTSP stream");
            }
        }
    }
}
