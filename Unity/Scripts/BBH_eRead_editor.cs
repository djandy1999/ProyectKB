using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using System.IO;

[CustomEditor(typeof(BBH_eRead))]

public class BBH_eRead_editor : Editor
{
    BBH_eRead targetManager;
    // Start is called before the first frame update
    private void OnEnable() {

        targetManager = (BBH_eRead)target;
        
    }

    public override void OnInspectorGUI()
    {
        base.OnInspectorGUI();
        BBH_eRead script = (BBH_eRead)target;
        if(GUILayout.Button("Connect", GUILayout.Height(35)))
        {
            script.Connect();

        }
        if(GUILayout.Button("Start Recording Electrode Data", GUILayout.Height(35)))
        {
            string path = Application.dataPath + "/CUSTOM/BBH_ELECTRODES/eLog"+ script.LogIndex + ".txt";
            Debug.Log("Creating eLog "+ script.LogIndex);

            if (!File.Exists(path)){
                File.WriteAllText(path, "\n\n");
            }    
        }
    }
}