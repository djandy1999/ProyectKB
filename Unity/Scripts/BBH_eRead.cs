using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.IO;

public class BBH_eRead : MonoBehaviour
{
    BBH myDevice = new BBH(); //Set the BBH Device to be used
    public bool ReadFromBoard = false; 
    public int LogIndex = 0;

	[Tooltip("SerialPort of your device.")]
	public string portName = "COM8";

	[Tooltip("Baudrate")]
	public int baudRate = 250000;

	[Tooltip("QueueLength")]
	public int QueueLength = 1;
    public string gesture = "0";

    //variable needed to Send Received e-Data to TKE_MAfinal for processing
    private TKE_MAfinal Stream;

    void Start()
    {
        myDevice.set (portName, baudRate, QueueLength); 
        myDevice.connect ();

        //TKE-MA
        Stream = GameObject.FindObjectOfType<TKE_MAfinal>(); 
		
    }
    public void Connect(){
        myDevice.send("!identity");
    }


	// Update is called once per frame
	void Update () {
        string path = Application.dataPath + "/CUSTOM/BBH_ELECTRODES/eLog"+ LogIndex +".txt";

		if(ReadFromBoard == true){
			string msg = myDevice.readQueue();
			if(msg != null)
                //Debug.Log("<color=#EDF6F9>[" + msg + "]</color>");
                Stream.eDataAppend(msg);
                if (File.Exists(path)){
                    string content = Time.time + ";" + gesture + ";"+ msg + "\n";
                    File.AppendAllText(path, content);
                }
		}


	}


    public void updateGesture(string g){
        gesture = g;
    }
    
    }
