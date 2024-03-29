using System.Collections;
using System.Collections.Generic;
using UnityEngine;


public class BBH {



	private BBHLines deviceReader;

	// gives the thread the vars needed for connecting your device and Unity:
	public void set(string port, int baudRate, int QueueLenght){ 
		deviceReader = new BBHLines(port, baudRate, QueueLenght); 
	}

	// setting up the connection beetwen your device and Unity without readTimeout:
	public void set(string port, int baudRate){ 
		deviceReader = new BBHLines(port, baudRate); 
	}

	// connect the device and unity
	public void connect(){
		deviceReader.openConnection(); // Open the Serial Port
		deviceReader.StartThread(); // Start the threads.
	}

	// Close the connection beetwen your device and Unity:
	public void close(){
		deviceReader.Write("!DC");
		deviceReader.OnApplicationQuit(); // Stop the threads and disconnect.
		
	}

	// read the data from your device:
	public string readQueue(){
		if (deviceReader.BoardConnected == true){
			return deviceReader.Read(); // call the Thread read method.
		}else{
			return null;
		}
	}

	public void send(string dataToSend, params object[] value ){
		if (deviceReader.BoardConnected == true)
			deviceReader.Write(dataToSend,BuildParameters(value)); // call the Thread write method.
		
	}
	//Build Message parameters to be sent to Arduino according to the CommandHandler.h format
    public static string BuildParameters(params object[] parameters)
    {
        string outputMessage = null;

        for(int i =0;i < parameters.Length;i++)
        {
            outputMessage += parameters[i];
            if (i != parameters.Length - 1)
                outputMessage += " ";
        }
        return outputMessage;
    }
}
