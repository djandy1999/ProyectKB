using System.Collections.Generic;
using System.Linq;
using System.Collections;
using System;
using System.IO;
using UnityEngine;
using UnityEngine.UI;

public class TKE_MAfinal : MonoBehaviour
{
    public float[] inputArray = new float[8];
    public decimal[] outputArray;
    bool startWrite = false;
    private Queue<float>[] fifoQueue;
    private Queue<float>[] tkeQueue;
    private int windowSize = 3;
    private float[] tkeArray;
    private decimal[] movingAverageArray;
    private int movingAverageWindowSize = 15;
    public Queue<float[]> inArrayQueue = new Queue<float[]>();
    float[] inputs = new float[8];
    bool isProcessing = false;
    public float[] myInput = new float[]{0,0,0,0,0,0,0,0,0,0};

    private BBHWrite BBHWrite;
    // Start is called before the first frame update
    void Start()
    {
        StartCoroutine(invokeLoop());
        BBHWrite = GameObject.FindObjectOfType<BBHWrite>();
        fifoQueue = new Queue<float>[inputArray.Length];
        for (int i = 0; i < fifoQueue.Length; i++)
        {
            fifoQueue[i] = new Queue<float>();
        }

        tkeQueue = new Queue<float>[inputArray.Length];
        for (int i = 0; i < tkeQueue.Length; i++)
        {
            tkeQueue[i] = new Queue<float>();
        }


        tkeArray = new float[inputArray.Length];
        movingAverageArray = new decimal[inputArray.Length];
    }
    public void eDataAppend(string eInput){
        try{
            float[] inArray = Array.ConvertAll(eInput.Split(';'), float.Parse);
            if (inArray.Length == 10)
            {
                if(inArray[1] > 5){
                    inArrayQueue.Enqueue(inArray);
                    //Debug.Log("in " + string.Join(";", inArray));
                }

            }
        }catch(Exception ex){
            //Log Ex 
            //Debug.LogError("An error occurred while processing the string: " + ex.Message);
        }

    }
        void inputDataQueue(){
        
                if (inArrayQueue.Count > 0)
                {
                    float[] oldestArray = inArrayQueue.Dequeue();
                    //Debug.Log("old " + string.Join(";", oldestArray));
                    // Use the oldestArray here, for example, print the values
                    myInput = oldestArray;
                    PreprocessData();
                    
                }
                }

        IEnumerator invokeLoop(){ 
            InvokeRepeating("inputDataQueue", 0f, 0.01f);
            yield return new WaitForSeconds(0.1f);

        }

    void PreprocessData()
    {

        for(int i = 2;  i < 10; i++){
            inputArray[i-2] = myInput[i];
        }
        //Debug.Log("ina " + string.Join(";", inputArray));
        if(inputArray != null){
            ProcessSamples(inputArray);
        }
        
           
        
        }

    // Update is called once per frame
    public void ProcessSamples(float[] eInput)
    {

        // Process each electrode value separately
        for (int i = 0; i < eInput.Length; i++)
        {
            // Add value to the FIFO queue
            fifoQueue[i].Enqueue(eInput[i]);
            

            // If FIFO queue size exceeds window size, remove oldest value
            if (fifoQueue[i].Count > windowSize)
            {
                fifoQueue[i].Dequeue();
                //Debug.Log("f " + string.Join(";", fifoQueue[i]));
            }
            if (tkeQueue[i].Count >= movingAverageWindowSize)
            {
                                // Calculate moving average
                tkeQueue[i].Dequeue();
                //Debug.Log("f " + string.Join(";", fifoQueue[i]));
            }

            

            // Calculate TKE
            if (fifoQueue[i].Count == windowSize)
            {
                float[] dataArray = fifoQueue[i].ToArray();
                float tke = CalculateTKE(dataArray);
                tkeArray[i] = tke;
                tkeQueue[i].Enqueue(tke);
                //Debug.Log(string.Join(";", tkeQueue[i])+" t " + string.Join(";", tkeArray));

                if (i == eInput.Length - 1)
                {
                    movingAverageArray = CalculateMovingAverage(tkeQueue, movingAverageWindowSize);
                    outputArray = movingAverageArray;
                   }
                    //Debug.Log("o " + string.Join(";", outputArray));
                            // Save results to CSV file


            }

        }
        
        
            if(!startWrite){
                float sum = 0;
                
                foreach(float item in outputArray){
                    sum  += item;
                }   
                if(sum > 7){
                    startWrite = true;
                }

            }
                if(startWrite){
                    
                    string filePath = Application.dataPath + "/output.csv";
                    if(outputArray != null){
                        using (StreamWriter writer = new StreamWriter(filePath , true))
                        {
                            //writer.WriteLine("Input,TKE,Moving Average");
                            string csv = string.Join(";", outputArray);
                            BBHWrite.UpdateElectrode(csv);
                            //Debug.Log(csv);
                            writer.WriteLine(myInput[1] + ";"  + csv);
                            
                        }
                        
                    }
                }
    }

    float CalculateTKE(float[] dataArray)
    {
        float sum = 0;

        sum = (dataArray[1]*dataArray[1]) - (dataArray[0]*dataArray[2]);
            
        
        return sum;
    }

    decimal[] CalculateMovingAverage(Queue<float>[] dataArray, int windowSize)
    {
        float[] output = new float[8];
        decimal[] dec_output = new decimal[8];

        for (int i = 0; i < 8; i++)
        {
            float sum = 0;
            int count = 0;
            //Debug.Log("m " + string.Join(";", dataArray[i].ToArray())); 
            if(dataArray[i].ToArray().Length == windowSize){
                for (int j = 0; j <= windowSize-1; j++)
                {
                    sum += dataArray[i].ToArray()[j];
                    //Debug.Log("m " + string.Join(";", dataArray[i].ToArray())); 
                    count++;
                }
                
                output[i] =  sum / count;

            }else{output[i] = 0;}
            }
            for (int i = 0; i < output.Length; i++)
            {
                decimal decimalValue = (decimal)Math.Round(output[i]);
                dec_output[i] = decimalValue;
            }
            //Debug.Log("m " + string.Join(";", dec_output));    
            return dec_output;

            
}
    }  


