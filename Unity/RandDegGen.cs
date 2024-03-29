using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class RandDegGen : MonoBehaviour
{

    
    private BBHWrite BBHWrite;
    public int ServoIndex = 0;
    public float delay = 0.01f;
    // Start is called before the first frame update
    void Start()
    {
        
        BBHWrite = GameObject.FindObjectOfType<BBHWrite>();
        InvokeRepeating("UpdateDeg", 0f, delay );
        
    }

    // Update is called once per frame
    void UpdateDeg()
    {
        int number = Random.Range(0,180);
        string num_str = number.ToString();
        //Debug.Log(num_str);
        BBHWrite.UpdateAngle(number, ServoIndex);
    }
}
