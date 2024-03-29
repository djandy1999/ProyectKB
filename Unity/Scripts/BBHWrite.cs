using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.Linq;
using System.Linq;
using System;
using System.IO;

// This script is used to read the data coming from the device.
public class BBHWrite : MonoBehaviour {

	BBH myDevice = new BBH(); //Set the BBH Device to be used

	public string Values;
	static int SERVONUM = 16;
    public int[] allValues = new int[SERVONUM];
	public float[] eValues = new float[]{0,0,0,0,0,0,0,0};
	public int ServoIndex = 0;
    public float RandGendelay = 0.01f;
	public bool e_bool = false;
	public bool d_bool = false;

	public bool randDegGen = false;
	public bool hearbeat = false;

	public bool ReadFromBoard = false;
	public bool WriteToBoard = false;
	private bool Sendlooping = false;
	private bool RandGenlooping = false;
	public float sendFreq = 20f;
	private Queue<int> samplesBuffer = new Queue<int>();
    private int windowSize = 5;



	public void UpdateAngle(int ang, int servoInd ){
        if(e_bool){
			e_bool = false;
		}
		d_bool = true;

		allValues[(int)servoInd] = ang;
    }
	public void UpdateElectrode(string inE){

		float[] eVal = Array.ConvertAll(inE.Split(";"), float.Parse);

		//Debug.Log("gd " + string.Join(" ", eVal));
		eValues = eVal;
    }


	[Tooltip("SerialPort of your device.")]
	public string portName = "COM8";

	[Tooltip("Baudrate")]
	public int baudRate = 250000;

	[Tooltip("QueueLength")]
	public int QueueLength = 1;

	void Start () {
		myDevice.set (portName, baudRate, QueueLength);
		myDevice.connect ();
	}

	// Update is called once per frame
	void Update () {
		if(ReadFromBoard == true){
			string msg = myDevice.readQueue();
			if(msg != null){
				Debug.Log("<color=#EDF6F9>[" + msg + "]</color>");
			}
		}

		if (hearbeat == true){
			StartCoroutine(HeartBeatLoop());
		}

		if (randDegGen == true && RandGenlooping == false){
			StartCoroutine(RandGenLoop());
		}

		if(randDegGen== false && RandGenlooping == true){
			CancelInvoke("RandDegGen");
			RandGenlooping = false;
		}

		if(WriteToBoard == true && Sendlooping == false){
			StartCoroutine(StartSendLoop());
		}

		if(WriteToBoard == false && Sendlooping == true){
			CancelInvoke("DataSend");
			Sendlooping = false;
		}

	}

	IEnumerator HeartBeatLoop(){

		myDevice.send("!heartbeat");
		yield return new WaitForSeconds(0.1f);
		hearbeat = false;
	}

	IEnumerator RandGenLoop(){

		RandGenlooping = true;
		InvokeRepeating("RandDegGen", 0f, RandGendelay);
		yield return new WaitForSeconds(0.1f);
	}

	IEnumerator StartSendLoop(){
		float sendDel = 1/sendFreq;
		Debug.Log(sendDel);
		Sendlooping = true;
		InvokeRepeating("DataSend", 1.0f, 0.045f);
		yield return new WaitForSeconds(0.01f);

	}

    void RandDegGen()
    {
        int number = UnityEngine.Random.Range(0,180);
        string num_str = number.ToString();
        //Debug.Log(num_str);
        UpdateAngle(number, ServoIndex);
    }

	void DataSend()
    {
		if(d_bool){
        string send_msg = string.Format("!UD");
		myDevice.send(send_msg, allValues[0], allValues[1], allValues[2], allValues[3], allValues[4], allValues[5], allValues[6], allValues[7], allValues[8], allValues[9], allValues[10], allValues[11], allValues[12], allValues[13], allValues[14], allValues[15]);
		}
		if(e_bool){
        string send_msg = string.Format("!UG");
		myDevice.send(send_msg, eValues[0], eValues[1], eValues[2], eValues[3], eValues[4], eValues[5], eValues[6], eValues[7]);
		}
	}

	void OnApplicationQuit() { // close the Thread and Serial Port
		myDevice.close();
	}

}

