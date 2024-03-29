using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using System.IO;


public class Servo_Angle_KNC : MonoBehaviour
{
    //ANGLE 2
    public Transform origin;
    public Transform palm;
    //public Text Text2;
    public static float angle_2;
    public int Angle2; 
    //

    private BBHWrite BBHWrite;

    //public int servoNumm;
    int prevAng = 0;
    int sendAngle = 0;

    //TRACK
    public Transform track;
    private Transform cachedTransform;
    public static Vector3 cachedPosition;
    //

    

    public int ServoIndex = 0;
    
    //[HideInInspector]
    //public string[] ServoIndex = new string[] { "0", "1", "2", "3", "4","5", "6", "7", "8", "9","10", "11", "12", "13", "14", "15"};
    
    void Start()
    {
        cachedTransform = GetComponent<Transform>();
        if (track)
        {
            cachedPosition = track.position;
        }

        BBHWrite = GameObject.FindObjectOfType<BBHWrite>();

    }

    // Update is called once per frame
    void Update()
    {
        //Angle 2

        string path = Application.dataPath + "/CUSTOM/Log"+ ServoIndex +".txt";
        //Vector3 targetDir =  cachedPosition - origin.position;

        Vector3 targetDir =   cachedTransform.position - origin.position;
        angle_2 = Vector3.SignedAngle(targetDir, origin.up, palm.forward);            
            
        //Debug.Log(targetDir);

        
        
        if ((int)angle_2 != prevAng){
            if ((int)angle_2 > prevAng + 2 || (int)angle_2 < prevAng - 2 ){
                if ((int)angle_2 > 0){
                    //UduinoManager.Instance.sendCommand("servoUp",((int)angle_2*2));
                    BBHWrite.UpdateAngle((int)angle_2*2, ServoIndex);
                    prevAng= (int)angle_2; 
                    Angle2 = (int)angle_2;  
                }else{
                    sendAngle = 90 + ((-1)*(int)angle_2);
                    BBHWrite.UpdateAngle(0 , ServoIndex);
                    //UduinoManager.Instance.sendCommand("servoUp",((int)sendAngle*2));
                    prevAng= (int)angle_2;

                } 

            }

            //UduinoManager.Instance.sendCommand("servoUp",(int)angle_2);
            //UduinoManager.Instance.sendCommand("servoDown",currAng);
            //Text2.text = angle_2.ToString();
            

        }


        if (File.Exists(path)){

                string content = angle_2.ToString() + "\n";
                File.AppendAllText(path, content);
        }
        

        

        


    }
}
