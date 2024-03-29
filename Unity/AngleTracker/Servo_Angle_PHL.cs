using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using System.IO;


public class Servo_Angle_PHL : MonoBehaviour
{
    //ANGLE 2
    public Transform origin;
    public Transform palm;
    public static float angle_2;
    public int Angle2; 
    public int Adjustment;
    //

    
    private List<OVRBone> fingerBones;

    private BBHWrite BBHWrite;

    //public int servoNumm;
    int prevAng = 0;


    //TRACK
    public Transform track;
    private Transform cachedTransform;
    public static Vector3 cachedPosition;
    //

    public int ServoIndex = 0;
    
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

        string path = Application.dataPath + "/CUSTOM/mLog"+ ServoIndex +".txt";
        //Vector3 targetDir =  cachedPosition - origin.position;

        Vector3 relPosOrigin = palm.transform.InverseTransformPoint(origin.position);
        Vector3 relPosTarget = palm.transform.InverseTransformPoint(track.position);

        Vector3 targetDir = relPosTarget  - relPosOrigin;
        //Debug.DrawRay(relPosOrigin, targetDir, Color.green);
        angle_2 = Mathf.Atan2(targetDir.y, targetDir.x)*Mathf.Rad2Deg;            
            
        //Debug.Log(relPosOrigin);

        
        
        if ((int)angle_2 != prevAng){
            if ((int)angle_2 > prevAng + 2 || (int)angle_2 < prevAng - 2 ){
                if ((int)angle_2 > 0){

                    BBHWrite.UpdateAngle((int)angle_2+Adjustment, ServoIndex);
                    prevAng= (int)angle_2;  
                    //Debug.Log(angle_2.ToString());
                    Angle2 = (int)angle_2;
                if (File.Exists(path)){

                    string content = Time.time+";"+angle_2.ToString() + "\n";
                    //System.DateTime.Now.Hour+ ":" +System.DateTime.Now.Minute+":"+System.DateTime.Now.Second+";"+angle_2.ToString() + "\n";
                    File.AppendAllText(path, content);
                }


                } 

            }
            

        }



        

        

        


    }
}
