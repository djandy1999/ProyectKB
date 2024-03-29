using UnityEngine;
using UnityEngine.Events;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading;
using System.IO.Ports;

public abstract class BBHThread : MonoBehaviour { 

	public SerialPort device;

	#region Variables
    //SerialPort
	private string port;
	private int baudRate;
	private int readTimeout = 1000;
    private int writeTimeout = 1000;
    private int i = 10;
    public string lastRead = null;
    public bool BoardConnected = false;

    //Threads
	private Thread RWthread;
	private Thread BBHthread;
	public bool threadRun = true;
    private int threadRestartTrials = 0;
	public int threadIdleDelay = 16; //16 for 60fps
    public bool skipMessageQueue = false;
    //Queue
    public Queue readQueue, writeQueue;
	public int QueueLength = 10;
    //Miscellaneous
    public bool isApplicationQuiting = false;
    public float delayBeforeDetect = 1f; //For Arduino Mega must be greater than 1f; for uno it can be 0.5f 
	#endregion

    #region Serialized Variables
    [SerializeField]
//Limit Send Rate
    private bool limitSR = false;
    public bool LimitSR
    {
        get { return limitSR; }
        set {
            if (limitSR == value)
                return;
           }

        }
/// SendRateSpeed
    [SerializeField]
    private int SRDelay = 20;
    public int Delay
    {
        get { return SRDelay; }
        set { SRDelay = value; }
    }
    #endregion

    #region Functions
//Thread Variables for Connection *with Timeout and QueueLength
	public BBHThread(string port, int baudRate, int QueueLength) { 
			this.port = port;
            this.baudRate = baudRate;
			this.QueueLength = QueueLength;
	}
//*Without
	public BBHThread(string port, int baudRate) { 
		this.port = port;
		this.baudRate = baudRate;
	}

    public void DiscoverWithDelay(float delay = -1)
        {
           StartCoroutine("DelayedDiscover", delay);
        }

    IEnumerator DelayedDiscover(float delay = -1)
        {
            if (delay == -1) delay = delayBeforeDetect;
            yield return new WaitForSeconds(delay);
            openConnection();
        }

//Open Serial port Connection and Board Detection
	public void openConnection() {
		readQueue = Queue.Synchronized( new Queue() );
		writeQueue  = Queue.Synchronized( new Queue() );
        
        device= new SerialPort(this.port, this.baudRate, Parity.None, 8, StopBits.One); 
		device.ReadTimeout = this.readTimeout; 
        device.WriteTimeout = this.writeTimeout;
        device.Close();
        device.Dispose();
        device.Open();
        device.DiscardInBuffer();
        device.DiscardOutBuffer();
        device.BaseStream.Flush();
        device.DtrEnable = true; 
        Debug.Log("Opening stream on port <color=#006D77>[" + this.port + "]</color>");
        Detect();


	}
    
    public virtual void Detect()
        {

        if (Application.isPlaying)
        {
            try
            {
                Thread detectThread = null;
                detectThread = new Thread(() => DetectionThread());
                detectThread.Start();
            }
            catch (System.Exception e)
            {
                Debug.Log(e);
            }
    }
    }

    public void DetectionThread()
        {

            int j = 0;
            Thread.Sleep(Mathf.FloorToInt(delayBeforeDetect* 1000));
            do
            {
                if (TryToFind(true))
                    return;
                Thread.Sleep(100);

            } while (j++ < i-1 && !isApplicationQuiting);

        }

    public virtual void GotMessage(string msg)
        {
            ReSuccess(msg);
            if (msg != null && readQueue.Count < QueueLength)
                lock (readQueue)
                    readQueue.Enqueue(msg);
        }

    public virtual void ReSuccess(string msg)
        {
            lastRead = msg;
            if (msg.Split(' ')[0] == "BBHidentity")
                return;
            
        }

    bool TryToFind( bool callAsync = false)
        {
                string reading = Read("!identity");
                Debug.Log("Trying to get name on <color=#006D77>[" + this.port + "]</color>.");
                if (reading == null) reading = lastRead;
                if (reading != null)
                {
                    string name = reading;
                    includeBoard(name);
                return true;
                }
                else
                {
                    Debug.Log("Impossible to get name on <color=#9C0D38>[" + this.port+ "]</color>. Retrying.");
                }
            return false;
        }


    public void includeBoard(string name)
        {
            lock (device)
            {
                //Debug.Log("Probe2");
                try
                {
                    Debug.Log("Board <color=#83C5BE>" + name + "</color> <color=#006D77>[" + this.port + "]</color> detected.");
                    BoardConnected = true;
                    
                }
                catch (Exception)
                {
                    Debug.Log("Board <color=#83C5BE>" + name + "</color> <color=#9C0D38>[" + this.port + "]</color> NOT detected.");
                }
                finally
                {

                    Write("!connected");
                }
            }
        }
    //--
    //Required Functions
    public void OnApplicationQuit()
    {
        isApplicationQuiting = true;
        FullReset();
    }

    private void OnDestroy()
    {
        isApplicationQuiting = true;
        FullReset();
    }

    void OnDisable()
    {
        isApplicationQuiting = true;
        FullReset();
    }

    public void FullReset()
    {
        Close();
        StopThread();
		ClearQueues();

    }
	public void ClearQueues()
    {
        WL();

        lock (readQueue)
            readQueue.Clear();
        lock (writeQueue)
            writeQueue.Clear();
    }
    void Update()
        {       
            // Read and Write Threading Loop
            if (RWthread != null && !isApplicationQuiting)
            {
                StopThread();
                StartThread(true);
            }
            //MainThreadLoop();
		}
    #endregion

    #region Thread Loops

//Start Threads and Queues
	public void StartThread(bool isForced = false) {

		if (isForced && threadRestartTrials > 10)
            {
                Debug.Log("Thread cannot restart.");
                return;
            }
        if (Application.isPlaying && RWthread == null)
            {
                try
                {
                    if(isForced)
                    {
                        Debug.Log("Resarting Read/Write Thread");
                        threadRestartTrials++;
                    }
                    Debug.Log("Starting Read/Write thread.");
                    RWthread = new Thread(new ThreadStart(StartPorts));
                    threadRun = true;
                    RWthread.Start();
                    RWthread.IsBackground = true;
                }
                catch (System.Exception e)
                {
                    Debug.Log(e);
                }
            }
            else
            {
                Debug.Log("Read/Write Thread already started.");
            }

		//BBHthread = new Thread (MainThreadLoop);
		//BBHthread.Start ();
        }

    
//Read and Write from Arduino in Port from openConnection()
	public void StartPorts(){
		while(!isApplicationQuiting)
		{
        	lock (device)
            {
                WL();
                RL();
            }
            Thread.Sleep(threadIdleDelay);
            if (limitSR) Thread.Sleep((int)SRDelay / 2);
        }
        RWthread = null;			
	}
	
	public bool looping = true;
//Stops Both threads
	public void StopThread () { 
 		lock (this) 
 		{
 			looping = false; 
 			threadRun = false;
        	RWthread = null;
		}
	}

    //Read And Write
        public virtual bool Write(string message, object value = null)
        {
            if (message == null || message == "")
                return false;

            if (value != null)
                message = message + " " + value.ToString(); 

            if (message.Length > 140)
                Debug.Log("The message length and parameters are too long)");

            AddwriteQueue(message);

            //Debug.Log(message);

            if (!WL()) 
                return false;

            return true;
        }
        
        public virtual string Read(string message = null)
        {

            AddwriteQueue(message);
            //Debug.Log(message);
         
            if (message != null)
            {
                RL(true);
            }

            lock (readQueue)
            {
                if (readQueue.Count == 0){
                    //Debug.Log("Nothing");
                    return null;
                }

                string finalMessage = (string)readQueue.Dequeue();
                //Debug.Log(finalMessage);
                return finalMessage;
            }
        }
//Read and Write Loop
        public virtual bool WL() { 

            if (device == null)
                return false;

            lock (writeQueue)
            {
                if (writeQueue.Count == 0)
                    return false;

                string message = (string)writeQueue.Dequeue();
                try
                {
                    try
                    {
                        device.WriteLine(message);
                        device.BaseStream.Flush();
                        //Debug.Log("<color=#98CE00>" + message + "</color> sent to <color=#006D77>[" + this.port + "]</color>");
                    }
                    catch (Exception e)
                    {
                        writeQueue.Enqueue(message);
                        Debug.Log("Impossible to send the message <color=#98CE00>" + message + "</color> to <color=#006D77>[" + this.port + "]</color>: " + e); 
                        return false;
                    }
            
                }
                catch (Exception e)
                {
                    Debug.Log("Error on port <color=#9C0D38>[" + this.port + "]</color>: " + e);
                    return false;
                }

            }
            return true;
        }



        public virtual bool RL(bool forceReading = false)
        {
            if (device == null)
                return false;
            if (forceReading)
            {
                WL();
                return true;
            }
            try
            {
                try
                {
                    int lineCount = 0;
                    while (lineCount < 5)
                    {
                        string readedLine = device.ReadLine();
                        GotMessage(readedLine);
                        //Debug.Log(readedLine);
                        lineCount++;
                    }
                    if (lineCount > 5)
                    {
                        device.DiscardOutBuffer();
                        device.DiscardInBuffer();
                    }
                    return true;
                }
                catch (TimeoutException e)
                {
                    if(device != null){}
                        //Debug.Log("ReadTimeout. Are you sure something is written in the Serial of the board? \n" + e);
                }
            }
            catch (Exception e)
            {
                if (e.GetType() == typeof(System.IO.IOException) || e.InnerException.GetType() == typeof(System.IO.IOException))
                {
                    Debug.Log("Impossible to read message from arduino. Arduino has restarted."); // TODO : Reconnected
                }
                else
                    Debug.Log(e);



 
            }
            return false;
        
        }

        public virtual void AddreadQueue(object message)
        {
            lock (readQueue)
                readQueue.Enqueue(message);
        }


        public virtual bool AddwriteQueue(string message)
        {
            if (message == null)
                return false;
            //Debug.Log(writeQueue.Count);

            lock (writeQueue)
            {
                message += "\r\n";
                if (!writeQueue.Contains(message)) 
                {
                    if (writeQueue.Count < QueueLength)
                        writeQueue.Enqueue(message);
                    else Debug.Log("The queue is full. Send less frequently or increase queue length.");
                }
            }

           return false;
        }

    #endregion

    #region Close
    public void Close()
    {
            device.Close();

            if (device != null)
            {
                Debug.Log("Closing port : <color=#EDF6F9>[" + this.port + "]</color>");
                System.Threading.Thread.Sleep(30);
                device.Close();
                device.Dispose();
                device.Open();
                device.DiscardInBuffer();
                device.DiscardOutBuffer();
                device.BaseStream.Flush();
                
                device = null;
            }
        }
    #endregion



}


