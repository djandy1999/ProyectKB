using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using System.IO;
 
[CustomEditor(typeof(Servo_Angle_PHL))]


public class Servo_Angle_PHL_Editor : Editor
{
    Servo_Angle_PHL targetManager;
    private static int selec;
    //private string[] ServoIndex = new [] { "0", "1", "2", "3", "4","5", "6", "7", "8", "9","10", "11", "12", "13", "14", "15"};
    //public int arrayIdx = 0;

    private void OnEnable() {

        targetManager = (Servo_Angle_PHL)target;
        
    }

    public override void OnInspectorGUI()
    {
        base.OnInspectorGUI();

        Servo_Angle_PHL script = (Servo_Angle_PHL)target;

        //GUIContent arrayLabel = new GUIContent("Servo Index");
        //script.arrayIdx = EditorGUILayout.Popup(arrayLabel, script.arrayIdx, script.ServoIndex);
        

        if(GUILayout.Button("Start Recording Animation", GUILayout.Height(35)))
        {
            string path = Application.dataPath + "/CUSTOM/mLog"+ script.ServoIndex + ".txt";
            Debug.Log("Creating Log "+ script.ServoIndex);
            selec = script.ServoIndex;    
            
            if (!File.Exists(path)){
                File.WriteAllText(path,"\n\n");
            }    
        }

    }
}
