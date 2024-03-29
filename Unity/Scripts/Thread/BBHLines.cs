
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class BBHLines : BBHThread { 
	//Serial Port Connection and needed Parameters
	public BBHLines(string port, int baudRate, int QueueLenght) : base(port, baudRate, QueueLenght) {
	}
	public BBHLines(string port, int baudRate) : base(port, baudRate){
	}

}
