using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class sim_eData : MonoBehaviour
{
    // Start is called before the first frame update
    static string eDataPath = "Assets/CUSTOM/BBH_ELECTRODES/eLog28.txt";
    string fileData = System.IO.File.ReadAllText(eDataPath);
    string[] lines;
    
    string myEInput;
    private TKE_MAfinal Stream;
    void Start()
    {   
            
        Stream = GameObject.FindObjectOfType<TKE_MAfinal>();
        lines = fileData.Split("\n"[0]);
        StartCoroutine(invokeLoop());  
    }
    private IEnumerator invokeLoop(){ 

        WaitForSeconds wait = new WaitForSeconds(0.0005f );
        foreach(string line in lines){

            myEInput = line;
            //Debug.Log(line);
            Stream.eDataAppend(myEInput);
            yield return wait;
            
        }


    }


}

